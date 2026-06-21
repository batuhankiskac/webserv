#include "Client.hpp"

Client::Client() :
		clientFd(-1),
		requestBodyFd(-1),
		contentLength(-1),
		bodyReceived(0),
		state(READING_HEADERS)
{}

Client::~Client() {}
