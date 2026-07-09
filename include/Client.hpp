#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <unistd.h>

#include "RequestParser.hpp"

enum RequestState
{
	READING_HEADERS,
	READING_BODY,
	READING_CHUNKS,
	REQUEST_COMPLETE
};

struct Client
{
	int			clientFd;
	int			requestBodyFd;
	int			port;

	long long	contentLength;
	RequestState	state;
	std::size_t	bodyReceived;

	std::string	rawBuffer;
	std::string	requestHeader;
	std::string requestBody;

	std::string	response;

	RequestParser	request;

	Client();
	~Client();
};

#endif //CLIENT_HPP