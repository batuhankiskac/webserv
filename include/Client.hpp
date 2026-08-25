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

enum ChunkState
{
	CHUNK_READING_SIZE,
	CHUNK_READING_DATA,
	CHUNK_READING_DATA_CRLF,
	CHUNK_READING_TRAILERS
};

enum CgiState
{
	CGI_NONE,
	CGI_QUEUED,
	CGI_RUNNING,
	CGI_SENDING_RESPONSE
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
	ChunkState	chunkState;
	std::size_t	chunkBytesRemaining;
	std::size_t	chunkTrailerBytes;

	std::string	rawBuffer;
	std::string	requestHeader;
	std::string requestBody;

	std::string	response;
	std::size_t	responseOffset;

	RequestParser	request;

	int			cgiOutFd;
	pid_t		cgiPid;
	int			cgiBodyFd;
	int			cgiBodyWriteFd;
	std::size_t	cgiBytesReceived;
	std::size_t	cgiBodyRemaining;
	std::string	cgiBodyBuffer;
	std::size_t	cgiBodyBufferOffset;
	CgiState	cgiState;
	bool		cgiSlotHeld;

	Client();
	~Client();
};

#endif //CLIENT_HPP
