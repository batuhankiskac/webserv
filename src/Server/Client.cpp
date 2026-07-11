#include "Client.hpp"

Client::Client() :
		clientFd(-1),
		requestBodyFd(-1),
		port(-1),
		contentLength(-1),
		state(READING_HEADERS),
		bodyReceived(0),
		cgiOutFd(-1),
		cgiPid(-1),
		cgiActive(false)
{}

Client::~Client()
{
	if (requestBodyFd != -1)
		close(requestBodyFd);
	if (cgiOutFd != -1)
		close(cgiOutFd);
}
