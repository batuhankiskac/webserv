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

static std::string	_buildAllowHeaderValue(const std::vector<std::string>& allowed) {
	if (allowed.empty())
		return ("GET");

	std::string	out;
	for (size_t i = 0; i < allowed.size(); ++i) {
		bool	seen = false;
		for (size_t j = 0; j < i; ++j) {
			if (allowed[j] == allowed[i]) {
				seen = true;
				break;
			}
		}
		if (seen)
			continue;
		if (!out.empty())
			out += ", ";
		out += allowed[i];
	}
	return (out);
}

static void	_respond(Client& client, int status, const std::string& body,
		const std::string& contentType) {
	Response	resp;
	resp.setHttpVersion(client.request.getHttpVersion());
	resp.setStatus(status);
	resp.setBody(body);
	if (!contentType.empty())
		resp.addHeader("Content-Type", contentType);
	resp.addHeader("Content-Length", _sizeToString(body.size()));
	resp.addHeader("Connection", "close");
	client.response = resp.serialize();
}

static bool	_isImplementedMethod(const std::string& method) {
	return (method == "GET" || method == "POST" || method == "DELETE");
}

static bool	_isKnownHttpMethod(const std::string& method) {
	return (
		method == "GET" || method == "HEAD" || method == "POST" ||
		method == "PUT" || method == "DELETE" || method == "CONNECT" ||
		method == "OPTIONS" || method == "TRACE" || method == "PATCH"
	);
}

static bool	_isMethodAllowed(const LocationBlock& location, const std::string& method) {
	const std::vector<std::string>& allowed = location.getAllowMethods();
	for (size_t i = 0; i < allowed.size(); ++i) {
		if (allowed[i] == method)
			return (true);
	}
	return (false);
}

static void	_serveMethodNotAllowed(Client& client, const ServerBlock& server,
		const LocationBlock& location) {
	Response	resp = Response::error(HTTP_METHOD_NOT_ALLOWED, server);
	resp.setHttpVersion(client.request.getHttpVersion());
	resp.addHeader("Allow", _buildAllowHeaderValue(location.getAllowMethods()));
	client.response = resp.serialize();
}

static std::string	_joinLocationPath(const LocationBlock& loc, const std::string& reqPath) {
	std::string	root = loc.getRoot();
	if (!root.empty() && root[root.size() - 1] == '/' &&
		!reqPath.empty() && reqPath[0] == '/')
		root.erase(root.size() - 1);
	else if (!root.empty() && root[root.size() - 1] != '/' &&
		!reqPath.empty() && reqPath[0] != '/')
		root += '/';
	return root + reqPath;
}

static bool	_hasParentTraversal(const std::string& path) {
	std::stringstream	ss(path);
	std::string	part;

	while (std::getline(ss, part, '/')) {
		if (part == "..")
			return true;
	}
	return false;
}

static std::string	_htmlEscape(const std::string& value) {
	std::string out;
	for (size_t i = 0; i < value.size(); ++i) {
		switch (value[i]) {
			case '&': out += "&amp;"; break;
			case '<': out += "&lt;"; break;
			case '>': out += "&gt;"; break;
			case '"': out += "&quot;"; break;
			case '\'': out += "&#39;"; break;
			default: out += value[i];
		}
	}
	return out;
}

const ServerBlock&	RequestHandler::_selectServerBlock(const WebservConfig& config, int port) {
	const std::vector<ServerBlock>&	servers = config.getServers();

	for (size_t i = 0; i < servers.size(); ++i) {
		if (servers[i].getPort() == port)
			return (servers[i]);
	}

	return (servers[0]);
}

const LocationBlock*	RequestHandler::_selectLocationBlock(const ServerBlock& server, const std::string& path) {
	const std::vector<LocationBlock>&	locations = server.getLocations();
	const LocationBlock*	best = NULL;
	size_t	bestLen = 0;

	for (size_t i = 0; i < locations.size(); ++i) {
		const std::string&	locPath = locations[i].getPath();
		bool boundary = locPath == "/" || path.size() == locPath.size() ||
			(path.size() > locPath.size() && path[locPath.size()] == '/');
		if (boundary && path.compare(0, locPath.size(), locPath) == 0) {
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
	resp.setHttpVersion(client.request.getHttpVersion());
	client.response = resp.serialize();
}

void	RequestHandler::_handleGet(Client& client, const ServerBlock& server, const LocationBlock& loc, const std::string& reqPath) {
	std::string	root = loc.getRoot();
	if (root.empty()) {
		_serveError(client, HTTP_NOT_FOUND, server);
		return;
	}

	std::string filePath = _joinLocationPath(loc, reqPath);
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
				_serveError(client, HTTP_NOT_FOUND, server);
				return;
			}
			std::string	html = _generateAutoindex(filePath, reqPath);
			_respond(client, HTTP_OK, html, "text/html");
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

	_respond(client, HTTP_OK, body, _lookupMimeType(filePath));
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
			if (stat(fullPath.c_str(), &entrySt) == 0 && S_ISDIR(entrySt.st_mode)) {
				isDir = true;
			}
			ss << "<a href=\"" << _htmlEscape(name) << "\">"
				<< _htmlEscape(name);
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
	envStorage.push_back("SERVER_SOFTWARE=webserv/1.0");
	envStorage.push_back("SERVER_NAME=" + server.getIp());
	envStorage.push_back("SERVER_PORT=" + _sizeToString(static_cast<size_t>(server.getPort())));
	envStorage.push_back("SERVER_PROTOCOL=" + client.request.getHttpVersion());
	envStorage.push_back("REMOTE_ADDR=" + client.clientIp);
	envStorage.push_back("REQUEST_METHOD=" + client.request.getMethod());
	envStorage.push_back("QUERY_STRING=" + client.request.getQueryString());
	std::string requestUri = reqPath;
	if (!client.request.getQueryString().empty())
		requestUri += "?" + client.request.getQueryString();
	envStorage.push_back("REQUEST_URI=" + requestUri);
	envStorage.push_back("SCRIPT_NAME=" + reqPath);
	envStorage.push_back("SCRIPT_FILENAME=" + _joinLocationPath(loc, reqPath));
	envStorage.push_back("PATH_INFO=" + reqPath);
	envStorage.push_back("PATH_TRANSLATED=" + _joinLocationPath(loc, reqPath));
	envStorage.push_back("REDIRECT_STATUS=200");

	if (client.contentLength >= 0) {
		envStorage.push_back("CONTENT_LENGTH=" + _sizeToString(static_cast<size_t>(client.contentLength)));
	}
	std::string contentType = client.request.getHeader("Content-Type");
	if (!contentType.empty()) {
		envStorage.push_back("CONTENT_TYPE=" + contentType);
	}
	if (client.contentLength < 0)
		envStorage.push_back("CONTENT_LENGTH=0");

	const std::map<std::string, std::string, CaseInsensitiveCompare>& headers = client.request.getHeaders();
	for (std::map<std::string, std::string, CaseInsensitiveCompare>::const_iterator it = headers.begin(); it != headers.end(); ++it) {
		std::string key = it->first;
		for (size_t i = 0; i < key.size(); ++i) {
			if (key[i] == '-') key[i] = '_';
			else key[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(key[i])));
		}
		if (key != "CONTENT_LENGTH" && key != "CONTENT_TYPE")
			envStorage.push_back("HTTP_" + key + "=" + it->second);
	}

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
	if (client.requestBodyFd != -1 && !client.requestBodyPath.empty())
		stdinFd = open(client.requestBodyPath.c_str(), O_RDONLY);

	if (stdinFd == -1 && !client.requestBody.empty()) {
		std::stringstream ss;
		ss << "/tmp/webserv_cgi_" << client.clientFd << "_" << static_cast<long>(std::time(NULL));
		std::string tmpPath = ss.str();
		int tempFd = open(tmpPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
		if (tempFd == -1) {
			close(cgiOut[0]);
			close(cgiOut[1]);
			_serveError(client, HTTP_INTERNAL_SERVER_ERROR, server);
			return;
		}
		std::size_t written = 0;
		while (written < client.requestBody.size()) {
			ssize_t n = write(tempFd, client.requestBody.c_str() + written, client.requestBody.size() - written);
			if (n <= 0) {
				close(tempFd);
				std::remove(tmpPath.c_str());
				close(cgiOut[0]); close(cgiOut[1]);
				_serveError(client, HTTP_INTERNAL_SERVER_ERROR, server);
				return;
			}
			written += static_cast<std::size_t>(n);
		}
		close(tempFd);
		stdinFd = open(tmpPath.c_str(), O_RDONLY);
		std::remove(tmpPath.c_str());
		if (stdinFd == -1) {
			close(cgiOut[0]); close(cgiOut[1]);
			_serveError(client, HTTP_INTERNAL_SERVER_ERROR, server);
			return;
		}
	}
	if (client.requestBodyFd != -1 && stdinFd == -1) {
		close(cgiOut[0]); close(cgiOut[1]);
		_serveError(client, HTTP_INTERNAL_SERVER_ERROR, server);
		return;
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
		std::string physicalPath = _joinLocationPath(loc, reqPath);
		std::string cgiPath = loc.getCgiPath();
		char* argv[3];
		argv[0] = const_cast<char*>(cgiPath.c_str());
		argv[1] = const_cast<char*>(physicalPath.c_str());
		argv[2] = NULL;
		execve(argv[0], argv, envp);
		std::string fail = "Status: " + _sizeToString(HTTP_INTERNAL_SERVER_ERROR) + " Internal Server Error\r\n"
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

void	RequestHandler::_handlePost(Client& client, const ServerBlock& server, const LocationBlock& loc) {
	if (!loc.getUploadEnable()) {
		_serveError(client, HTTP_FORBIDDEN, server);
		return;
	}

	std::string	uploadStore = loc.getUploadStore();
	if (uploadStore.empty()) {
		_serveError(client, HTTP_INTERNAL_SERVER_ERROR, server);
		return;
	}

	static unsigned long uploadCounter = 0;
	std::stringstream	nameSS;
	nameSS << "upload_" << static_cast<unsigned long>(std::time(NULL)) << "_" << client.clientFd << "_" << ++uploadCounter << ".dat";
	std::string	filename = nameSS.str();

	std::string	targetPath = uploadStore;
	if (targetPath[targetPath.size() - 1] != '/')
		targetPath += '/';
	targetPath += filename;

	bool	written = false;
	std::ofstream out(targetPath.c_str(), std::ios::binary);
	if (out.is_open()) {
		if (!client.requestBody.empty()) {
			out.write(client.requestBody.data(), static_cast<std::streamsize>(client.requestBody.size()));
			written = !out.bad();
		} else if (client.requestBodyFd != -1) {
			int readFd = open(client.requestBodyPath.c_str(), O_RDONLY);
			if (readFd != -1) {
				char buf[4096];
				ssize_t n;
				written = true;
				while ((n = read(readFd, buf, sizeof(buf))) > 0)
					out.write(buf, static_cast<std::streamsize>(n));
				close(readFd);
				written = written && !out.bad();
			}
		} else {
			written = true;
		}
		out.close();
	}

	if (!written) {
		_serveError(client, HTTP_INTERNAL_SERVER_ERROR, server);
		return;
	}

	std::string	body = "Upload successful.\r\n";
	_respond(client, HTTP_OK, body, "text/plain");
}

void	RequestHandler::_handleDelete(Client& client, const ServerBlock& server, const LocationBlock& loc, const std::string& reqPath) {
	std::string	root = loc.getRoot();
	if (root.empty()) {
		_serveError(client, HTTP_NOT_FOUND, server);
		return;
	}

	std::string filePath = _joinLocationPath(loc, reqPath);
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
	resp.setHttpVersion(client.request.getHttpVersion());
	resp.setStatus(HTTP_NO_CONTENT);
	resp.addHeader("Content-Length", "0");
	resp.addHeader("Connection", "close");
	client.response = resp.serialize();
}

void	RequestHandler::handle(Client& client, const WebservConfig& config, int port) {
	const ServerBlock&	server = _selectServerBlock(config, port);

	std::string	reqPath = client.request.getPath();
	const LocationBlock*	loc = _selectLocationBlock(server, reqPath);
	if (!loc) {
		_serveError(client, HTTP_NOT_FOUND, server);
		return;
	}
	if (_hasParentTraversal(reqPath)) {
		_serveError(client, HTTP_FORBIDDEN, server);
		return;
	}

	if (loc->getReturnCode() != 200) {
		Response	resp;
		resp.setHttpVersion(client.request.getHttpVersion());
		resp.setStatus(loc->getReturnCode());
		resp.addHeader("Location", loc->getReturnUrl());
		resp.addHeader("Content-Length", "0");
		resp.addHeader("Connection", "close");
		client.response = resp.serialize();
		return;
	}

	const std::string&	method = client.request.getMethod();
	const bool	isImplemented = _isImplementedMethod(method);
	if (!isImplemented && !_isKnownHttpMethod(method)) {
		_serveError(client, HTTP_NOT_IMPLEMENTED, server);
		return;
	}
	if (!isImplemented || !_isMethodAllowed(*loc, method)) {
		_serveMethodNotAllowed(client, server, *loc);
		return;
	}

	const size_t	maxBodySize = loc->hasClientMaxBodySize()
		? loc->getClientMaxBodySize() : server.getClientMaxBodySize();
	long long	contentLen = client.contentLength;
	if (maxBodySize != 0 && contentLen > 0
		&& static_cast<size_t>(contentLen) > maxBodySize) {
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
		_handlePost(client, server, *loc);
	else if (method == "DELETE")
		_handleDelete(client, server, *loc, reqPath);
	else
		_serveError(client, HTTP_NOT_IMPLEMENTED, server);
}
