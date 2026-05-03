#ifndef LISTEN_AND_ACCEPT_REQS
#define LISTEN_AND_ACCEPT_REQS

#include <exception>
#include <cstring>
#include <cerrno>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <vector>
#include <unistd.h>

class ListenAndAcceptReqs
{
	public:
		ListenAndAcceptReqs(const int socketFd);
		~ListenAndAcceptReqs();

	private:
		int	epollFd;
		std::vector<struct epoll_event> epollEvents;

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