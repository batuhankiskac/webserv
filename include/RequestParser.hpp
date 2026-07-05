#ifndef REQUEST_PARSER_HPP
#define REQUEST_PARSER_HPP

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
	COMPLETE,
	ERROR
};

class RequestParser {
private:
	Phase _phase;
	std::string _buffer;
	size_t _readIndex;

	std::string _method;
	std::string _path;
	std::string _queryString;
	std::string _httpVersion;

	std::map<std::string, std::string, CaseInsensitiveCompare> _headers;
	int _errorCode;

	void _flushBuffer();

	void _parseRequestLine(const std::string& line);
	void _parseHeaderLine(const std::string& line);

	bool _handleRequestLine();
	bool _handleHeaders();

	void _setError(int code);
	static std::string _trim(const std::string& str, const std::string& chars = " \t");

public:
	RequestParser();
	~RequestParser();

	void parse(const std::string& rawRequest);

	std::string getMethod() const;
	std::string getPath() const;
	std::string getQueryString() const;
	std::string getHttpVersion() const;
	const std::map<std::string, std::string, CaseInsensitiveCompare>& getHeaders() const;
	std::string getHeader(const std::string& key) const;
	long long getContentLength() const;
	bool isChunked() const;
	bool hasError() const;
	int getErrorCode() const;
};

#endif
