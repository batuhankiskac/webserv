#include "Server.hpp"

Server::Server(std::string ip, int port)
{
	socketFd = socket(AF_INET, SOCK_STREAM, 0);
	if (socketFd < 0)
	{
		throw Server::SocketCreationError();
	}

	const int	open = 1;
	if (setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &open, sizeof(open)) < 0)
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
		socketAddress.sin_addr.s_addr = inet_addr(ip.c_str());

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

Server::SocketCreationError::SocketCreationError() : _errno(errno) {}

const char* Server::SocketCreationError::what() const throw()
{
	return (std::strerror(_errno));
}
