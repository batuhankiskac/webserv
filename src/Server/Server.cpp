#include "Server.hpp"
#include <netdb.h>

Server::Server(std::string ip, int port) : port(port)
{
	socketFd = socket(AF_INET, SOCK_STREAM, 0);
	if (socketFd < 0)
	{
		throw Server::SocketCreationError();
	}

	const int	reuse = 1;
	if (setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
	{
		close(socketFd);
		throw Server::SocketCreationError();
	}
	if (fcntl(socketFd, F_SETFL, O_NONBLOCK) < 0)
	{
		close(socketFd);
		throw Server::SocketCreationError();
	}
	fcntl(socketFd, F_SETFD, FD_CLOEXEC);

	struct sockaddr_in socketAddress;
	socketAddress.sin_family = AF_INET;
	socketAddress.sin_port = htons(port);
	if (ip.empty())
		socketAddress.sin_addr.s_addr = INADDR_ANY;
	else
	{
		struct addrinfo hints;
		struct addrinfo* result = NULL;
		std::memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_NUMERICHOST;
		if (getaddrinfo(ip.c_str(), NULL, &hints, &result) != 0 || result == NULL)
		{
			if (result != NULL) freeaddrinfo(result);
			close(socketFd);
			throw Server::SocketCreationError();
		}
		socketAddress.sin_addr = reinterpret_cast<struct sockaddr_in*>(result->ai_addr)->sin_addr;
		freeaddrinfo(result);
	}

	if (bind(socketFd, (struct sockaddr*)&socketAddress, sizeof(socketAddress)) < 0)
	{
		close(socketFd);
		throw Server::SocketCreationError();
	}
}

Server::~Server()
{
	close(socketFd);
}

int Server::getSocketFd( void ) const
{
	return (socketFd);
}

int Server::getPort( void ) const
{
	return (port);
}

Server::SocketCreationError::SocketCreationError() : _errno(errno) {}

const char* Server::SocketCreationError::what() const throw()
{
	return (std::strerror(_errno));
}
