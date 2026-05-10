#include "Request.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>

Request::Request(size_t maxBodySize) :
	_phase(REQUEST),
	_readIndex(0),
	_contentLength(0),
	_maxBodySize(maxBodySize),
	_currentChunkSize(0),
	_isChunked(false),
	_errorCode(0) { }

Request::~Request() { }

std::string Request::_getNextLine() {
	size_t pos = _buffer.find("\r\n", _readIndex);
	if (pos == std::string::npos) {
		return "";
	}

	std::string line = _buffer.substr(_readIndex, pos - _readIndex);
	_readIndex = pos + 2;

	return line;
}

void Request::_flushBuffer() {
	if (_readIndex > 0) {
		_buffer.erase(0, _readIndex);
		_readIndex = 0;
	}
}

void Request::_setError(int code) {
	_errorCode = code;
	_phase = ERROR;
}

std::string Request::_trim(const std::string& str, const std::string& chars) {
	size_t start = str.find_first_not_of(chars);
	if (start == std::string::npos) {
		return "";
	}
	size_t end = str.find_last_not_of(chars);
	return str.substr(start, end - start + 1);
}

void Request::_parseRequestLine(const std::string& line) {
	size_t methodEnd = line.find(' ');
	if (methodEnd == std::string::npos) {
		_setError(400);
		return;
	}

	size_t pathEnd = line.find(' ', methodEnd + 1);
	if (pathEnd == std::string::npos) {
		_setError(400);
		return;
	}

	_method = line.substr(0, methodEnd);
	std::string fullPath = line.substr(methodEnd + 1, pathEnd - methodEnd - 1);
	_httpVersion = line.substr(pathEnd + 1);

	size_t queryPos = fullPath.find('?');
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

void Request::_parseHeaderLine(const std::string& line) {
	size_t colonPos = line.find(':');
	if (colonPos == std::string::npos) {
		_setError(400);
		return;
	}

	std::string key = line.substr(0, colonPos);
	std::string value = _trim(line.substr(colonPos + 1));

	_headers[key] = value;
}

void Request::_transitionToBody() {
	std::map<std::string, std::string, CaseInsensitiveCompare>::iterator itChunk = _headers.find("transfer-encoding");
	std::map<std::string, std::string, CaseInsensitiveCompare>::iterator itLength = _headers.find("content-length");

	if (itChunk != _headers.end() && itChunk->second.find("chunked") != std::string::npos) {
		_isChunked = true;
		_phase = CHUNKED_SIZE;
	} else if (itLength != _headers.end()) {
		std::stringstream ss(itLength->second);
		ss >> _contentLength;

		if (ss.fail()) {
			_setError(400);
		} else if (_maxBodySize > 0 && _contentLength > _maxBodySize) {
			_setError(413);
		} else {
			_phase = BODY;
		}
	} else {
		_phase = COMPLETE;
	}
}

bool Request::_handleRequestLine() {
	std::string line = _getNextLine();
	if (line.empty() && _buffer.find("\r\n", _readIndex) == std::string::npos) {
		return false;
	}
	_parseRequestLine(line);
	return true;
}

bool Request::_handleHeaders() {
	std::string line = _getNextLine();
	if (line.empty() && _buffer.find("\r\n", _readIndex) == std::string::npos) {
		return false;
	}
	if (line.empty()) {
		_transitionToBody();
	} else {
		_parseHeaderLine(line);
	}
	return true;
}

bool Request::_handleBody() {
	size_t needed = _contentLength - _body.length();
	size_t available = _buffer.length() - _readIndex;

	size_t toRead = std::min(needed, available);

	_body.append(_buffer, _readIndex, toRead);
	_readIndex += toRead;

	if (_body.length() == _contentLength) {
		_phase = COMPLETE;
		return true;
	}
	return false;
}

bool Request::_handleChunkedSize() {
	std::string line = _getNextLine();
	if (line.empty() && _buffer.find("\r\n", _readIndex) == std::string::npos) {
		return false;
	}

	size_t semiPos = line.find(';');
	if (semiPos != std::string::npos) {
		line = line.substr(0, semiPos);
	}

	std::stringstream ss;
	ss << std::hex << line;
	ss >> _currentChunkSize;

	if (ss.fail()) {
		_setError(400);
		return true;
	}

	if (_currentChunkSize == 0) {
		std::string trailer = _getNextLine();
		if (!trailer.empty() || _buffer.find("\r\n", _readIndex) != std::string::npos) {
			_readIndex += 2;
			_phase = COMPLETE;
		}
		return true;
	}

	if (_maxBodySize > 0 && (_body.length() + _currentChunkSize) > _maxBodySize) {
		_setError(413);
		return true;
	}

	_phase = CHUNKED_DATA;
	return true;
}

bool Request::_handleChunkedData() {
	size_t available = _buffer.length() - _readIndex;
	size_t needed = _currentChunkSize + 2;

	if (available < needed) {
		return false;
	}

	_body.append(_buffer, _readIndex, _currentChunkSize);
	_readIndex += needed;
	_phase = CHUNKED_SIZE;
	return true;
}

void Request::parse(const std::string& rawRequest) {
	_buffer.append(rawRequest);

	static bool (Request::*const handlers[])(void) = {
		&Request::_handleRequestLine,
		&Request::_handleHeaders,
		&Request::_handleBody,
		&Request::_handleChunkedSize,
		&Request::_handleChunkedData
	};

	while (_phase != COMPLETE && _phase != ERROR) {
		bool (Request::*handler)(void) = handlers[_phase];
		if (!(this->*handler)()) {
			break;
		}
	}

	_flushBuffer();
}
