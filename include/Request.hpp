#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <string>
#include <map>
#include <cctype>

struct CaseInsensitiveCompare {
	bool operator()(const std::string& a, const std::string& b) const {
		size_t i = 0;
		while (i < a.length() && i < b.length()) {
			char ca = std::tolower(static_cast<unsigned char>(a[i]));
			char cb = std::tolower(static_cast<unsigned char>(b[i]));
			if (ca < cb) return true;
			if (ca > cb) return false;
			i++;
		}
		return a.length() < b.length();
	}
};

enum Phase {
	REQUEST,
	HEADERS,
	BODY,
	CHUNKED_SIZE,
	CHUNKED_DATA,
	COMPLETE,
	ERROR
};

class Request {
private:
	Phase _phase;
	std::string _buffer;
	size_t _readIndex;

	std::string _method;
	std::string _path;
	std::string _queryString;
	std::string _httpVersion;
	std::string _body;

	std::map<std::string, std::string, CaseInsensitiveCompare> _headers;
	size_t _contentLength;
	size_t _maxBodySize;
	size_t _currentChunkSize;

	bool _isChunked;
	int _errorCode;

	std::string _getNextLine();
	void _flushBuffer();

	void _parseRequestLine(const std::string& line);
	void _parseHeaderLine(const std::string& line);
	void _transitionToBody();

	bool _handleRequestLine();
	bool _handleHeaders();
	bool _handleBody();
	bool _handleChunkedSize();
	bool _handleChunkedData();

	void _setError(int code);
	static std::string _trim(const std::string& str, const std::string& chars = " \t");

public:
	Request(size_t maxBodySize = 0);
	~Request();

	void parse(const std::string& rawRequest);
};

#endif
