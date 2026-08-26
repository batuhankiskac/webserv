#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <unistd.h>
#include <sys/types.h>

#include "RequestParser.hpp"

enum RequestState
{
	READING_HEADERS,
	READING_BODY,
	READING_CHUNKS,
	REQUEST_COMPLETE
};

struct CgiSpoolState
{
	int			readFd;
	int			writeFd;
	std::size_t	bytesReceived;
	std::size_t	bodyRemaining;

	CgiSpoolState() :
		readFd(-1),
		writeFd(-1),
		bytesReceived(0),
		bodyRemaining(0)
	{}
};

struct Client
{
	int			clientFd;
	int			requestBodyFd;
	std::string	requestBodyPath;
	int			port;
	std::string	clientIp;

	long long	contentLength;
	RequestState	state;
	std::size_t	bodyReceived;

	std::string	rawBuffer;
	std::string	requestHeader;
	std::string requestBody;

	std::string	response;
	std::size_t	responseOffset;

	RequestParser	request;

	int			cgiOutFd;
	pid_t		cgiPid;
	CgiSpoolState	cgiSpool;
	bool		cgiActive;

	Client();
	~Client();
};

#endif //CLIENT_HPP
