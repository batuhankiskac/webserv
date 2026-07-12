#include "Server.hpp"

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

	struct sockaddr_in socketAddress;
	socketAddress.sin_family = AF_INET;
	socketAddress.sin_port = htons(port);
	if (ip.empty())
		socketAddress.sin_addr.s_addr = INADDR_ANY;
	else
	{
		struct addrinfo	hints;
		hints.ai_flags = 0;
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = 0;
		hints.ai_addrlen = 0;
		hints.ai_addr = NULL;
		hints.ai_canonname = NULL;
		hints.ai_next = NULL;
		struct addrinfo*	res = NULL;
		if (getaddrinfo(ip.c_str(), NULL, &hints, &res) != 0)
		{
			close(socketFd);
			throw Server::SocketCreationError();
		}
		socketAddress.sin_addr = reinterpret_cast<struct sockaddr_in*>(res->ai_addr)->sin_addr;
		freeaddrinfo(res);
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
