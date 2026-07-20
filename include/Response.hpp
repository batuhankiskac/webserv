#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include <string>
#include <map>
#include "ServerBlock.hpp"

struct ErrorInfo {
	int code;
	const char* reason;
	const char* message;
};

class Response {
private:
	int _status;
	std::map<std::string, std::string> _headers;
	std::string _body;
	std::string _httpVersion;

	static const ErrorInfo* _lookupError(int code);
	static std::string _defaultErrorHtml(int code);
	static std::string _intToString(long n);
	static bool _readFileToString(const std::string& path, std::string& out);

public:
	Response();

	void setStatus(int code);
	void addHeader(const std::string& k, const std::string& v);
	void setBody(const std::string& b);
	void setHttpVersion(const std::string& version);
	const std::string& getBody() const;

	std::string serialize() const;
	static Response error(int code, const ServerBlock& server);
};

#endif
