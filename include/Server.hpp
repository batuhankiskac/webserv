#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <cstring>

class Server
{
	public:
		Server(std::string ip, int port);
		~Server();

		int getSocketFd( void ) const;
		int getPort( void ) const;

	private:
		int	socketFd;
		const int	port;

		class SocketCreationError : public std::exception
		{
			private:
				int	_errno;
			
			public:
				SocketCreationError();
				virtual const char* what() const throw();
		};
};

#endif // SERVER_HPP