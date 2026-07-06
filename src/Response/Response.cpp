#include "Response.hpp"
#include "HttpConstants.hpp"
#include <sstream>
#include <fstream>

static const ErrorInfo g_errorTable[] = {
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
	{ HTTP_VERSION_NOT_SUPPORTED,		"HTTP Version Not Supported",	"The HTTP version is not supported." }
};

static const std::size_t g_errorTableSize = sizeof(g_errorTable) / sizeof(g_errorTable[0]);

struct ReasonPhrase {
	int code;
	const char* reason;
};

static const ReasonPhrase g_reasonTable[] = {
	{ 200, "OK" },
	{ 301, "Moved Permanently" },
	{ 302, "Found" },
	{ 303, "See Other" },
	{ 307, "Temporary Redirect" },
	{ 308, "Permanent Redirect" }
};

static const std::size_t g_reasonTableSize = sizeof(g_reasonTable) / sizeof(g_reasonTable[0]);

Response::Response() : _status(200) { }

void Response::setStatus(int code) { _status = code; }
void Response::addHeader(const std::string& k, const std::string& v) { _headers[k] = v; }
void Response::setBody(const std::string& b) { _body = b; }
const std::string& Response::getBody() const { return _body; }

const ErrorInfo* Response::_lookupError(int code) {
	for (std::size_t i = 0; i < g_errorTableSize; ++i) {
		if (g_errorTable[i].code == code)
			return &g_errorTable[i];
	}

	static ErrorInfo unknown = { 0, "Error", "Unknown error." };
	unknown.code = code;

	return &unknown;
}

const char* Response::_reasonPhrase(int code) {
	for (std::size_t i = 0; i < g_reasonTableSize; ++i) {
		if (g_reasonTable[i].code == code)
			return g_reasonTable[i].reason;
	}
	return NULL;
}

std::string	Response::_defaultErrorHtml(int code) {
	const ErrorInfo* e = _lookupError(code);
	std::stringstream ss;

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
	std::ifstream f(path.c_str(), std::ios::binary);
	if (!f.is_open())
		return false;

	std::stringstream ss;
	ss << f.rdbuf();
	out = ss.str();

	if (f.bad())
		return false;

	return true;
}

std::string	Response::_intToString(long n) {
	std::stringstream ss;
	ss << n;
	return ss.str();
}

Response Response::error(int code, const ServerBlock& server) {
	Response r;
	r.setStatus(code);

	const std::string codeStr = _intToString(code);
	std::string body;
	bool ok = false;

	const std::map<std::string, std::string>& pages = server.getErrorPages();
	std::map<std::string, std::string>::const_iterator it = pages.find(codeStr);
	if (it != pages.end())
		ok = _readFileToString(it->second, body);

	if (!ok)
		body = _defaultErrorHtml(code);

	r.setBody(body);
	r.addHeader("Content-Type", "text/html");
	r.addHeader("Content-Length", _intToString(static_cast<long>(body.size())));
	r.addHeader("Connection", "close");

	return r;
}

std::string	Response::serialize() const {
	const char* reason = _reasonPhrase(_status);
	if (!reason)
		reason = _lookupError(_status)->reason;
	std::string out;

	out += "HTTP/1.1 ";
	out += _intToString(_status);
	out += " ";
	out += reason;
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
