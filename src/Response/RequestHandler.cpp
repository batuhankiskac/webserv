#include "RequestHandler.hpp"
#include "Response.hpp"
#include "ServerBlock.hpp"
#include "LocationBlock.hpp"
#include "HttpConstants.hpp"
#include <sstream>
#include <fstream>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <ctime>
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <vector>
#include <string>

RequestHandler::RequestHandler() {}

struct MimeEntry {
	const char*	ext;
	const char*	type;
};

static const MimeEntry	g_mimeTable[] = {
	{ ".html", "text/html" },
	{ ".htm", "text/html" },
	{ ".txt", "text/plain" },
	{ ".css", "text/css" },
	{ ".js", "application/javascript" },
	{ ".json", "application/json" },
	{ ".png", "image/png" },
	{ ".jpg", "image/jpeg" },
	{ ".jpeg", "image/jpeg" },
	{ ".gif", "image/gif" },
	{ ".ico", "image/x-icon" }
};

static const size_t	g_mimeTableSize = sizeof(g_mimeTable) / sizeof(g_mimeTable[0]);

static std::string	_lookupMimeType(const std::string& path) {
	size_t	dot = path.rfind('.');
	if (dot == std::string::npos)
		return ("application/octet-stream");

	std::string	ext = path.substr(dot);
	for (size_t i = 0; i < ext.size(); ++i)
		ext[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[i])));

	for (size_t i = 0; i < g_mimeTableSize; ++i) {
		if (ext == g_mimeTable[i].ext)
			return (g_mimeTable[i].type);
	}
	return ("application/octet-stream");
}

static std::string	_sizeToString(size_t n) {
	std::stringstream	ss;
	ss << static_cast<long>(n);
	return (ss.str());
}

std::string	RequestHandler::_extractHost(const Client& client) {
	std::string	host = client.request.getHeader("host");
	if (host.empty())
		return ("");
	size_t	colon = host.find(':');
	if (colon != std::string::npos)
		host = host.substr(0, colon);
	return (host);
}

const ServerBlock&	RequestHandler::_selectServerBlock(const WebservConfig& config, int port, const std::string& host) {
	const std::vector<ServerBlock>&	servers = config.getServers();
	const ServerBlock*	firstMatch = NULL;

	for (size_t i = 0; i < servers.size(); ++i) {
		if (servers[i].getPort() != port)
			continue;
		if (!firstMatch)
			firstMatch = &servers[i];
		const std::vector<std::string>&	names = servers[i].getServerNames();
		for (size_t j = 0; j < names.size(); ++j) {
			if (names[j] == host)
				return (servers[i]);
		}
	}

	if (firstMatch)
		return (*firstMatch);
	return (servers[0]);
}

const LocationBlock*	RequestHandler::_selectLocationBlock(const ServerBlock& server, const std::string& path) {
	const std::vector<LocationBlock>&	locations = server.getLocations();
	const LocationBlock*	best = NULL;
	size_t	bestLen = 0;

	for (size_t i = 0; i < locations.size(); ++i) {
		const std::string&	locPath = locations[i].getPath();
		if (path.size() >= locPath.size() && path.compare(0, locPath.size(), locPath) == 0) {
			if (locPath.size() > bestLen) {
				best = &locations[i];
				bestLen = locPath.size();
			}
		}
	}
	return (best);
}

void	RequestHandler::_serveError(Client& client, int code, const ServerBlock& server) {
	Response	resp = Response::error(code, server);
	client.response = resp.serialize();
}

void	RequestHandler::_handleGet(Client& client, const ServerBlock& server, const LocationBlock& loc, const std::string& reqPath) {
	std::string	root = loc.getRoot();
	if (root.empty()) {
		_serveError(client, HTTP_NOT_FOUND, server);
		return;
	}

	if (reqPath.find("..") != std::string::npos) {
		_serveError(client, HTTP_FORBIDDEN, server);
		return;
	}

	std::string	filePath = root + reqPath;

	struct stat	st;
	if (stat(filePath.c_str(), &st) != 0) {
		_serveError(client, HTTP_NOT_FOUND, server);
		return;
	}

	if (S_ISDIR(st.st_mode)) {
		const std::vector<std::string>&	index = loc.getIndex();
		bool	found = false;
		for (size_t i = 0; i < index.size(); ++i) {
			std::string	indexPath = filePath;
			if (indexPath[indexPath.size() - 1] != '/')
				indexPath += '/';
			indexPath += index[i];

			struct stat	idxSt;
			if (stat(indexPath.c_str(), &idxSt) == 0 && S_ISREG(idxSt.st_mode)) {
				filePath = indexPath;
				found = true;
				break;
			}
		}
		if (!found) {
			if (!loc.getAutoIndex()) {
				_serveError(client, HTTP_FORBIDDEN, server);
				return;
			}
			std::string	html = _generateAutoindex(filePath, reqPath);
			Response	resp;
			resp.setStatus(HTTP_OK);
			resp.setBody(html);
			resp.addHeader("Content-Type", "text/html");
			resp.addHeader("Content-Length", _sizeToString(html.size()));
			resp.addHeader("Connection", "close");
			client.response = resp.serialize();
			return;
		}
	} else if (!S_ISREG(st.st_mode)) {
		_serveError(client, HTTP_FORBIDDEN, server);
		return;
	}

	std::ifstream	f(filePath.c_str(), std::ios::binary);
	if (!f.is_open()) {
		_serveError(client, HTTP_FORBIDDEN, server);
		return;
	}

	std::stringstream	ss;
	ss << f.rdbuf();
	std::string	body = ss.str();
	if (f.bad()) {
		_serveError(client, HTTP_INTERNAL_SERVER_ERROR, server);
		return;
	}

	Response	resp;
	resp.setStatus(HTTP_OK);
	resp.setBody(body);
	resp.addHeader("Content-Type", _lookupMimeType(filePath));
	resp.addHeader("Content-Length", _sizeToString(body.size()));
	resp.addHeader("Connection", "close");
	client.response = resp.serialize();
}

std::string	RequestHandler::_generateAutoindex(const std::string& filePath, const std::string& reqPath) {
	std::stringstream	ss;
	ss << "<!DOCTYPE html>\r\n";
	ss << "<html>\r\n";
	ss << "<head><title>Index of " << reqPath << "</title></head>\r\n";
	ss << "<body>\r\n";
	ss << "<h1>Index of " << reqPath << "</h1>\r\n";
	ss << "<hr>\r\n";

	DIR*	dir = opendir(filePath.c_str());
	if (!dir) {
		ss << "<p>Unable to read directory.</p>\r\n";
	} else {
		struct dirent*	entry;
		while ((entry = readdir(dir)) != NULL) {
			std::string	name = entry->d_name;
			if (name == ".")
				continue;
			std::string	fullPath = filePath;
			if (fullPath[fullPath.size() - 1] != '/')
				fullPath += '/';
			fullPath += name;
			struct stat	entrySt;
			bool	isDir = false;
			if (stat(fullPath.c_str(), &entrySt) == 0 && S_ISDIR(entrySt.st_mode))
				isDir = true;
			ss << "<a href=\"" << reqPath;
			if (reqPath.empty() || reqPath[reqPath.size() - 1] != '/')
				ss << "/";
			ss << name << "\">" << name;
			if (isDir)
				ss << "/";
			ss << "</a><br>\r\n";
		}
		closedir(dir);
	}

	ss << "<hr>\r\n";
	ss << "<i>webserv</i>\r\n";
	ss << "</body>\r\n";
	ss << "</html>\r\n";
	return (ss.str());
}

char** RequestHandler::_buildCgiEnv(Client& client, const ServerBlock& server,
		const LocationBlock& loc, const std::string& reqPath,
		std::vector<std::string>& envStorage) {
	envStorage.push_back("GATEWAY_INTERFACE=CGI/1.1");
	envStorage.push_back("SERVER_PROTOCOL=HTTP/1.1");
	envStorage.push_back("REQUEST_METHOD=" + client.request.getMethod());
	envStorage.push_back("QUERY_STRING=" + client.request.getQueryString());
	envStorage.push_back("SCRIPT_NAME=" + reqPath);
	envStorage.push_back("PATH_INFO=" + reqPath);

	std::string locPath = loc.getPath();
	std::string relativePath;
	if (reqPath.size() > locPath.size())
		relativePath = reqPath.substr(locPath.size());
	else
		relativePath = "/";
	envStorage.push_back("PATH_TRANSLATED=" + loc.getRoot() + relativePath);

	envStorage.push_back("SERVER_NAME=" + server.getIp());
	envStorage.push_back("SERVER_PORT=" + _sizeToString(static_cast<size_t>(server.getPort())));

	if (client.contentLength > 0) {
		envStorage.push_back("CONTENT_LENGTH=" + _sizeToString(static_cast<size_t>(client.contentLength)));
		envStorage.push_back("CONTENT_TYPE=" + client.request.getHeader("Content-Type"));
	} else {
		envStorage.push_back("CONTENT_LENGTH=0");
	}

	std::string cookie = client.request.getHeader("Cookie");
	if (!cookie.empty())
		envStorage.push_back("HTTP_COOKIE=" + cookie);

	char** envp = new char*[envStorage.size() + 1];
	for (std::size_t i = 0; i < envStorage.size(); ++i)
		envp[i] = const_cast<char*>(envStorage[i].c_str());
	envp[envStorage.size()] = NULL;
	return (envp);
}

void RequestHandler::_handleCgi(Client& client, const ServerBlock& server,
		const LocationBlock& loc, const std::string& reqPath) {
	int cgiOut[2];
	if (pipe(cgiOut) == -1) {
		_serveError(client, HTTP_INTERNAL_SERVER_ERROR, server);
		return;
	}
	if (fcntl(cgiOut[0], F_SETFL, O_NONBLOCK) == -1) {
		close(cgiOut[0]);
		close(cgiOut[1]);
		_serveError(client, HTTP_INTERNAL_SERVER_ERROR, server);
		return;
	}

	int stdinFd = -1;
	if (client.requestBodyFd != -1) {
		lseek(client.requestBodyFd, 0, SEEK_SET);
		stdinFd = client.requestBodyFd;
	} else if (!client.requestBody.empty()) {
		std::stringstream ss;
		ss << "/tmp/webserv_cgi_" << client.clientFd;
		std::string tmpPath = ss.str();
		stdinFd = open(tmpPath.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0600);
		if (stdinFd == -1) {
			close(cgiOut[0]);
			close(cgiOut[1]);
			_serveError(client, HTTP_INTERNAL_SERVER_ERROR, server);
			return;
		}
		write(stdinFd, client.requestBody.c_str(), client.requestBody.size());
		lseek(stdinFd, 0, SEEK_SET);
	}

	std::vector<std::string> envStorage;
	char** envp = _buildCgiEnv(client, server, loc, reqPath, envStorage);

	pid_t pid = fork();
	if (pid == -1) {
		delete[] envp;
		close(cgiOut[0]);
		close(cgiOut[1]);
		if (stdinFd != -1 && stdinFd != client.requestBodyFd)
			close(stdinFd);
		_serveError(client, HTTP_INTERNAL_SERVER_ERROR, server);
		return;
	}

	if (pid == 0) {
		close(cgiOut[0]);
		dup2(cgiOut[1], STDOUT_FILENO);
		close(cgiOut[1]);
		if (stdinFd != -1) {
			dup2(stdinFd, STDIN_FILENO);
			close(stdinFd);
		}
		if (chdir(loc.getRoot().c_str()) == -1)
			std::exit(1);
		std::string locPath = loc.getPath();
		std::string relativePath;
		if (reqPath.size() > locPath.size())
			relativePath = reqPath.substr(locPath.size());
		else
			relativePath = "/";
		std::string physicalPath = loc.getRoot() + relativePath;
		std::string cgiPath = loc.getCgiPath();
		char* argv[3];
		argv[0] = const_cast<char*>(cgiPath.c_str());
		argv[1] = const_cast<char*>(physicalPath.c_str());
		argv[2] = NULL;
		execve(argv[0], argv, envp);
		std::string fail = "Status: 500 Internal Server Error\r\n"
		                   "Content-Type: text/plain\r\n\r\nCGI execution failed";
		write(STDOUT_FILENO, fail.c_str(), fail.size());
		std::exit(1);
	}

	close(cgiOut[1]);
	if (stdinFd != -1 && stdinFd != client.requestBodyFd)
		close(stdinFd);
	delete[] envp;

	client.cgiOutFd = cgiOut[0];
	client.cgiPid = pid;
	client.cgiResponse.clear();
	client.cgiActive = true;
}

void	RequestHandler::_handlePost(Client& client, const ServerBlock& server, const LocationBlock& loc, const std::string& reqPath) {
	(void)reqPath;
	if (!loc.getUploadEnable()) {
		_serveError(client, HTTP_FORBIDDEN, server);
		return;
	}

	std::string	uploadStore = loc.getUploadStore();
	if (uploadStore.empty()) {
		_serveError(client, HTTP_INTERNAL_SERVER_ERROR, server);
		return;
	}

	std::stringstream	nameSS;
	nameSS << "upload_" << time(NULL) << "_" << client.clientFd << ".dat";
	std::string	filename = nameSS.str();

	std::string	targetPath = uploadStore;
	if (targetPath[targetPath.size() - 1] != '/')
		targetPath += '/';
	targetPath += filename;

	bool	written = false;
	if (!client.requestBody.empty()) {
		std::ofstream	out(targetPath.c_str(), std::ios::binary);
		if (out.is_open()) {
			out.write(client.requestBody.data(), static_cast<std::streamsize>(client.requestBody.size()));
			out.close();
			written = !out.bad();
		}
	} else if (client.requestBodyFd != -1) {
		if (lseek(client.requestBodyFd, 0, SEEK_SET) == static_cast<off_t>(-1)) {
			_serveError(client, HTTP_INTERNAL_SERVER_ERROR, server);
			return;
		}
		std::ofstream	out(targetPath.c_str(), std::ios::binary);
		if (out.is_open()) {
			char	buf[4096];
			ssize_t	n;
			while ((n = read(client.requestBodyFd, buf, sizeof(buf))) > 0)
				out.write(buf, static_cast<std::streamsize>(n));
			out.close();
			written = !out.bad();
		}
	}

	if (!written) {
		_serveError(client, HTTP_INTERNAL_SERVER_ERROR, server);
		return;
	}

	std::string	body = "Upload successful.\r\n";
	Response	resp;
	resp.setStatus(HTTP_CREATED);
	resp.setBody(body);
	resp.addHeader("Content-Type", "text/plain");
	resp.addHeader("Content-Length", _sizeToString(body.size()));
	resp.addHeader("Connection", "close");
	client.response = resp.serialize();
}

void	RequestHandler::_handleDelete(Client& client, const ServerBlock& server, const LocationBlock& loc, const std::string& reqPath) {
	std::string	root = loc.getRoot();
	if (root.empty()) {
		_serveError(client, HTTP_NOT_FOUND, server);
		return;
	}

	if (reqPath.find("..") != std::string::npos) {
		_serveError(client, HTTP_FORBIDDEN, server);
		return;
	}

	std::string	filePath = root + reqPath;

	struct stat	st;
	if (stat(filePath.c_str(), &st) != 0) {
		_serveError(client, HTTP_NOT_FOUND, server);
		return;
	}

	if (S_ISDIR(st.st_mode)) {
		_serveError(client, HTTP_FORBIDDEN, server);
		return;
	}

	if (std::remove(filePath.c_str()) != 0) {
		int	err = errno;
		if (err == EACCES || err == EPERM)
			_serveError(client, HTTP_FORBIDDEN, server);
		else
			_serveError(client, HTTP_INTERNAL_SERVER_ERROR, server);
		return;
	}

	Response	resp;
	resp.setStatus(HTTP_NO_CONTENT);
	resp.addHeader("Content-Length", "0");
	resp.addHeader("Connection", "close");
	client.response = resp.serialize();
}

void	RequestHandler::handle(Client& client, const WebservConfig& config, int port) {
	std::string	host = _extractHost(client);
	const ServerBlock&	server = _selectServerBlock(config, port, host);

	std::string	reqPath = client.request.getPath();
	const LocationBlock*	loc = _selectLocationBlock(server, reqPath);
	if (!loc) {
		_serveError(client, HTTP_NOT_FOUND, server);
		return;
	}

	if (loc->getReturnCode() != 200) {
		Response	resp;
		resp.setStatus(loc->getReturnCode());
		resp.addHeader("Location", loc->getReturnUrl());
		resp.addHeader("Content-Length", "0");
		resp.addHeader("Connection", "close");
		client.response = resp.serialize();
		return;
	}

	const std::string&	method = client.request.getMethod();
	const std::vector<std::string>&	allowed = loc->getAllowMethods();
	bool	methodOk = false;
	for (size_t i = 0; i < allowed.size(); ++i) {
		if (allowed[i] == method) {
			methodOk = true;
			break;
		}
	}
	if (!methodOk) {
		_serveError(client, HTTP_METHOD_NOT_ALLOWED, server);
		return;
	}

	long long	contentLen = client.contentLength;
	if (contentLen > 0 && static_cast<size_t>(contentLen) > server.getClientMaxBodySize()) {
		_serveError(client, HTTP_PAYLOAD_TOO_LARGE, server);
		return;
	}

	std::string	cgiExt = loc->getCgiExt();
	std::string	cgiPath = loc->getCgiPath();
	if (!cgiExt.empty() && !cgiPath.empty() &&
		reqPath.size() >= cgiExt.size() &&
		reqPath.compare(reqPath.size() - cgiExt.size(), cgiExt.size(), cgiExt) == 0) {
		_handleCgi(client, server, *loc, reqPath);
		return;
	}

	if (method == "GET")
		_handleGet(client, server, *loc, reqPath);
	else if (method == "POST")
		_handlePost(client, server, *loc, reqPath);
	else if (method == "DELETE")
		_handleDelete(client, server, *loc, reqPath);
	else
		_serveError(client, HTTP_NOT_IMPLEMENTED, server);
}
