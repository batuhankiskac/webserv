#include "Client.hpp"

Client::Client() :
		clientFd(-1),
		requestBodyFd(-1),
		port(-1),
		contentLength(-1),
		state(READING_HEADERS),
		bodyReceived(0)
{}

Client::~Client()
{
	if (requestBodyFd != -1)
		close(requestBodyFd);
}
