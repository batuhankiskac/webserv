#include "Client.hpp"

Client::Client() :
		clientFd(-1),
			requestBodyFd(-1),
			port(-1),
			serverBlockIndex(0),
			contentLength(-1),
			state(READING_HEADERS),
			bodyReceived(0),
			chunkState(CHUNK_READING_SIZE),
			chunkBytesRemaining(0),
			chunkMetadataBytes(0),
			responseOffset(0),
			responseFileFd(-1),
			responseFileRemaining(0),
			cgiOutFd(-1),
		cgiPid(-1),
		cgiActive(false)
{}

Client::~Client()
{
	if (cgiOutFd != -1)
		close(cgiOutFd);
}
