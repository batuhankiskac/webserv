#include "RequestParser.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>

RequestParser::RequestParser() :
	_phase(REQUEST),
	_readIndex(0),
	_errorCode(0) { }

RequestParser::~RequestParser() { }

void RequestParser::_flushBuffer() {
	if (_readIndex > 0) {
		_buffer.erase(0, _readIndex);
		_readIndex = 0;
	}
}

void RequestParser::_setError(int code) {
	_errorCode = code;
	_phase = ERROR;
}

std::string RequestParser::_trim(const std::string& str, const std::string& chars) {
	size_t	start = str.find_first_not_of(chars);
	if (start == std::string::npos) {
		return "";
	}
	size_t	end = str.find_last_not_of(chars);
	return str.substr(start, end - start + 1);
}

void RequestParser::_parseRequestLine(const std::string& line) {
	size_t	methodEnd = line.find(' ');
	if (methodEnd == std::string::npos) {
		_setError(400);
		return;
	}

	size_t	pathEnd = line.find(' ', methodEnd + 1);
	if (pathEnd == std::string::npos) {
		_setError(400);
		return;
	}

	_method = line.substr(0, methodEnd);
	std::string	fullPath = line.substr(methodEnd + 1, pathEnd - methodEnd - 1);
	_httpVersion = line.substr(pathEnd + 1);
	if (_method.empty() || fullPath.empty() || _httpVersion.empty()) {
		_setError(400);
		return;
	}
	if (fullPath.size() > 8190) {
		_setError(414);
		return;
	}

	size_t	queryPos = fullPath.find('?');
	if (queryPos != std::string::npos) {
		_path = fullPath.substr(0, queryPos);
		_queryString = fullPath.substr(queryPos + 1);
	} else {
		_path = fullPath;
		_queryString = "";
	}

	if (_httpVersion != "HTTP/1.1" && _httpVersion != "HTTP/1.0") {
		_setError(505);
		return;
	}

	_phase = HEADERS;
}

void RequestParser::_parseHeaderLine(const std::string& line) {
	size_t	colonPos = line.find(':');
	if (colonPos == std::string::npos) {
		_setError(400);
		return;
	}

	std::string	key = line.substr(0, colonPos);
	std::string	value = _trim(line.substr(colonPos + 1));

	if (key.empty() || _headers.find(key) != _headers.end()) {
		_setError(400);
		return;
	}
	_headers[key] = value;
}

void RequestParser::_validateHeaders() {
	std::string cl = getHeader("content-length");
	for (size_t i = 0; i < cl.size(); ++i) {
		if (!std::isdigit(static_cast<unsigned char>(cl[i]))) {
			_setError(400);
			return;
		}
	}
	if (!cl.empty() && !getHeader("transfer-encoding").empty()) {
		_setError(400);
		return;
	}
	std::string te = getHeader("transfer-encoding");
	std::string lower = te;
	std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
	if (!te.empty() && lower != "chunked")
		_setError(400);
}

bool RequestParser::_handleRequestLine() {
	size_t	pos = _buffer.find("\r\n", _readIndex);
	if (pos == std::string::npos) {
		return false;
	}
	std::string	line = _buffer.substr(_readIndex, pos - _readIndex);
	_readIndex = pos + 2;
	_parseRequestLine(line);
	return true;
}

bool RequestParser::_handleHeaders() {
	size_t	pos = _buffer.find("\r\n", _readIndex);
	if (pos == std::string::npos) {
		return false;
	}
	std::string	line = _buffer.substr(_readIndex, pos - _readIndex);
	_readIndex = pos + 2;
	if (line.empty()) {
		_validateHeaders();
		if (_phase == ERROR)
			return true;
		if (_httpVersion == "HTTP/1.1" && getHeader("host").empty()) {
			_setError(400);
		} else {
			_phase = COMPLETE;
		}
	} else {
		_parseHeaderLine(line);
	}
	return true;
}

void RequestParser::parse(const std::string& rawRequest) {
	_buffer.append(rawRequest);

	static bool (RequestParser::*const handlers[])(void) = {
		&RequestParser::_handleRequestLine,
		&RequestParser::_handleHeaders
	};

	while (_phase != COMPLETE && _phase != ERROR) {
		bool (RequestParser::*handler)(void) = handlers[_phase];
		if (!(this->*handler)()) {
			break;
		}
	}

	_flushBuffer();
}

std::string RequestParser::getMethod() const { return _method; }
std::string RequestParser::getPath() const { return _path; }
std::string RequestParser::getQueryString() const { return _queryString; }
std::string RequestParser::getHttpVersion() const { return _httpVersion; }

const std::map<std::string, std::string, CaseInsensitiveCompare>& RequestParser::getHeaders() const {
	return _headers;
}

std::string RequestParser::getHeader(const std::string& key) const {
	std::map<std::string, std::string, CaseInsensitiveCompare>::const_iterator	it = _headers.find(key);
	if (it == _headers.end()) {
		return "";
	}
	return it->second;
}

long long RequestParser::getContentLength() const {
	std::string	val = getHeader("content-length");
	if (val.empty()) {
		return -1;
	}
	std::stringstream	ss(val);
	long long	result = -1;
	ss >> result;
	if (ss.fail() || result < 0) {
		return -1;
	}
	return result;
}

bool RequestParser::isChunked() const {
	std::string	val = getHeader("transfer-encoding");
	std::string	lower = val;
	std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
	return lower.find("chunked") != std::string::npos;
}

bool RequestParser::hasError() const { return _phase == ERROR; }
int RequestParser::getErrorCode() const { return _errorCode; }
