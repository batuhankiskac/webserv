#include "include/ListenAndAcceptReqs.hpp"

ListenAndAcceptReqs::ListenAndAcceptReqs(const int socketFd)
{
	if (listen(socketFd, SOMAXCONN) < 0)
	{
		throw ListenAndAcceptReqs::ListenOrAcceptionError();
	}

	epollFd = epoll_create(1);
	if (epollFd == -1)
	{
		throw (ListenAndAcceptReqs::ListenOrAcceptionError());
	}

	struct epoll_event event;
	event.events = EPOLLIN;
	event.data.fd = socketFd;

	if (epoll_ctl(epollFd, EPOLL_CTL_ADD, socketFd, &event) == -1)
	{
		close(epollFd);
		throw (ListenAndAcceptReqs::ListenOrAcceptionError());
	}
}

ListenAndAcceptReqs::~ListenAndAcceptReqs()
{
	close(epollFd);
}

ListenAndAcceptReqs::ListenOrAcceptionError::ListenOrAcceptionError()
: _errno(errno) {}

const char* ListenAndAcceptReqs::ListenOrAcceptionError::what() const throw()
{
	return (std::strerror(_errno));
}

// HENÜZ TAMAMLANMADI.
