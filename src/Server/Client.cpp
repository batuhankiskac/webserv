#include "Client.hpp"

Client::Client() :
		clientFd(-1),
		requestBodyFd(-1),
		port(-1),
		contentLength(-1),
		state(READING_HEADERS),
		bodyReceived(0),
		chunkState(CHUNK_READING_SIZE),
		chunkBytesRemaining(0),
		chunkTrailerBytes(0),
		responseOffset(0),
		cgiOutFd(-1),
		cgiPid(-1),
		cgiBodyFd(-1),
		cgiBodyWriteFd(-1),
		cgiBytesReceived(0),
		cgiBodyRemaining(0),
		cgiBodyBufferOffset(0),
		cgiState(CGI_NONE),
		cgiSlotHeld(false)
{}

Client::~Client()
{
	if (cgiOutFd != -1)
		close(cgiOutFd);
	if (cgiBodyFd != -1)
		close(cgiBodyFd);
	if (cgiBodyWriteFd != -1)
		close(cgiBodyWriteFd);
}
