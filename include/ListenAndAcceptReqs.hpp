#ifndef LISTEN_AND_ACCEPT_REQS
#define LISTEN_AND_ACCEPT_REQS

#include <exception>
#include <cstring>
#include <cerrno>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <ctime>

#include "Client.hpp"
#include "Request.hpp"
#include "File.hpp"

#define BUFFER_SIZE 64
#define TIME_OUT	30
#define MAX_TOUR	50

class ListenAndAcceptReqs
{
	public:
		ListenAndAcceptReqs(const int socketFd, File& file);
		~ListenAndAcceptReqs();
		void	waitReqs();

	private:
		int	epollFd;
		int	socketFd;
		std::vector<struct epoll_event> epollEvents;
		std::map<int, Client> clients;

		File& file;

		void	cleanupClient(int fd, std::map<int, int>& fdTargetTour);

		class ListenOrAcceptionError : public std::exception
		{
			private:
				int	_errno;

			public:
				ListenOrAcceptionError();
				virtual const char* what() const throw();
		};
};

#endif //LISTEN_AND_ACCEPT_REQS