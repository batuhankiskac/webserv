#include "RequestHandler.hpp"
#include "Response.hpp"
#include "ServerBlock.hpp"
#include "LocationBlock.hpp"
#include "HttpConstants.hpp"

#include <sstream>
#include <fstream>
#include <sys/stat.h>
#include <cctype>

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

void	RequestHandler::_serveFile(Client& client, const ServerBlock& server, const LocationBlock& loc, const std::string& reqPath) {
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
			_serveError(client, HTTP_NOT_FOUND, server);
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
	resp.setStatus(200);
	resp.setBody(body);
	resp.addHeader("Content-Type", _lookupMimeType(filePath));
	resp.addHeader("Content-Length", _sizeToString(body.size()));
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

	if (loc->getReturnCode() != 200) {
		Response	resp;
		resp.setStatus(loc->getReturnCode());
		resp.addHeader("Location", loc->getReturnUrl());
		resp.addHeader("Content-Length", "0");
		resp.addHeader("Connection", "close");
		client.response = resp.serialize();
		return;
	}

	_serveFile(client, server, *loc, reqPath);
}
