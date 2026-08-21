#include "Response.hpp"
#include "HttpConstants.hpp"
#include <sstream>
#include <fstream>

static const ErrorInfo	g_errorTable[] = {
	{ HTTP_OK,							"OK",							"" },
	{ HTTP_CREATED,						"Created",						"" },
	{ HTTP_NO_CONTENT,					"No Content",					"" },
	{ HTTP_MOVED_PERMANENTLY,			"Moved Permanently",			"" },
	{ HTTP_FOUND,						"Found",						"" },
	{ HTTP_SEE_OTHER,					"See Other",					"" },
	{ HTTP_TEMPORARY_REDIRECT,			"Temporary Redirect",			"" },
	{ HTTP_PERMANENT_REDIRECT,			"Permanent Redirect",			"" },
	{ HTTP_BAD_REQUEST,					"Bad Request",					"Malformed request syntax." },
	{ HTTP_UNAUTHORIZED,				"Unauthorized",					"Authentication is required." },
	{ HTTP_FORBIDDEN,					"Forbidden",					"Access is denied." },
	{ HTTP_NOT_FOUND,					"Not Found",					"The requested resource could not be found." },
	{ HTTP_METHOD_NOT_ALLOWED,			"Method Not Allowed",			"The HTTP method is not allowed for this resource." },
	{ HTTP_LENGTH_REQUIRED,				"Length Required",				"The Content-Length header is required." },
	{ HTTP_PAYLOAD_TOO_LARGE,			"Payload Too Large",			"The request body exceeds the maximum allowed size." },
	{ HTTP_URI_TOO_LONG,				"URI Too Long",					"The request URI is too long." },
	{ HTTP_REQUEST_HEADER_FIELDS_TOO_LARGE,	"Request Header Fields Too Large",	"The request headers are too large." },
	{ HTTP_INTERNAL_SERVER_ERROR,		"Internal Server Error",		"The server encountered an unexpected condition." },
	{ HTTP_NOT_IMPLEMENTED,				"Not Implemented",				"The HTTP method is not implemented." },
	{ HTTP_SERVICE_UNAVAILABLE,			"Service Unavailable",			"The server is temporarily unavailable." },
	{ HTTP_VERSION_NOT_SUPPORTED,		"HTTP Version Not Supported",	"The HTTP version is not supported." },
	{ HTTP_BAD_GATEWAY,					"Bad Gateway",					"The upstream service returned an invalid response." },
	{ HTTP_GATEWAY_TIMEOUT,				"Gateway Timeout",				"The upstream service did not respond in time." },
};

static const std::size_t	g_errorTableSize = sizeof(g_errorTable) / sizeof(g_errorTable[0]);

Response::Response() : _status(200), _httpVersion("HTTP/1.1") { }

void Response::setStatus(int code) { _status = code; }
void Response::addHeader(const std::string& k, const std::string& v) { _headers[k] = v; }
void Response::setBody(const std::string& b) { _body = b; }
void Response::setHttpVersion(const std::string& version) { if (version == "HTTP/1.0" || version == "HTTP/1.1") _httpVersion = version; }
const std::string& Response::getBody() const { return _body; }

const ErrorInfo* Response::_lookupError(int code) {
	for (std::size_t i = 0; i < g_errorTableSize; ++i) {
		if (g_errorTable[i].code == code)
			return &g_errorTable[i];
	}

	static ErrorInfo	unknown = { 0, "Error", "Unknown error." };
	unknown.code = code;

	return &unknown;
}

std::string	Response::_defaultErrorHtml(int code) {
	const ErrorInfo*	e = _lookupError(code);
	std::stringstream	ss;

	ss << "<!DOCTYPE html>" << "\r\n";
	ss << "<html>" << "\r\n";
	ss << "<head><title>" << code << " " << e->reason << "</title></head>" << "\r\n";
	ss << "<body>" << "\r\n";
	ss << "<h1>" << code << " " << e->reason << "</h1>" << "\r\n";
	ss << "<p>" << e->message << "</p>" << "\r\n";
	ss << "<hr>" << "\r\n";
	ss << "<i>webserv</i>" << "\r\n";
	ss << "</body>" << "\r\n";
	ss << "</html>" << "\r\n";

	return ss.str();
}

bool Response::_readFileToString(const std::string& path, std::string& out) {
	std::ifstream	f(path.c_str(), std::ios::binary);
	if (!f.is_open())
		return false;

	std::stringstream	ss;
	ss << f.rdbuf();
	out = ss.str();

	if (f.bad())
		return false;

	return true;
}

std::string	Response::_intToString(long n) {
	std::stringstream	ss;
	ss << n;
	return ss.str();
}

Response Response::error(int code, const ServerBlock& server) {
	Response	r;
	r.setStatus(code);

	const std::string	codeStr = _intToString(code);
	std::string	body;
	bool	ok = false;

	const std::map<std::string, std::string>&	pages = server.getErrorPages();
	std::map<std::string, std::string>::const_iterator	it = pages.find(codeStr);
	if (it != pages.end())
	{
		ok = _readFileToString(it->second, body);
		if (!ok && !it->second.empty() && it->second[0] == '/') {
			const std::vector<LocationBlock>& locations = server.getLocations();
			for (std::size_t i = 0; i < locations.size() && !ok; ++i)
				if (locations[i].getPath() == "/" && !locations[i].getRoot().empty())
					ok = _readFileToString(locations[i].getRoot() + it->second, body);
		}
	}

	if (!ok)
		body = _defaultErrorHtml(code);

	r.setBody(body);
	r.addHeader("Content-Type", "text/html");
	r.addHeader("Content-Length", _intToString(static_cast<long>(body.size())));
	r.addHeader("Connection", "close");

	return r;
}

std::string	Response::serialize() const {
	const ErrorInfo*	e = _lookupError(_status);
	std::string	out;

	out += _httpVersion;
	out += " ";
	out += _intToString(_status);
	out += " ";
	out += e->reason;
	out += "\r\n";

	for (std::map<std::string, std::string>::const_iterator it = _headers.begin(); it != _headers.end(); ++it) {
		out += it->first;
		out += ": ";
		out += it->second;
		out += "\r\n";
	}

	out += "\r\n";
	out += _body;

	return out;
}
