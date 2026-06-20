#include "ListenAndAcceptReqs.hpp"

ListenAndAcceptReqs::ListenAndAcceptReqs(const int socketFd)
{
	this->socketFd = socketFd;
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
	clients.clear();
	close(epollFd);
}

ListenAndAcceptReqs::ListenOrAcceptionError::ListenOrAcceptionError()
: _errno(errno) {}

const char* ListenAndAcceptReqs::ListenOrAcceptionError::what() const throw()
{
	return (std::strerror(_errno));
}

void	ListenAndAcceptReqs::waitReqs()
{
	epollEvents.resize(BUFFER_SIZE);

	int	timeout = TIME_OUT;
	int	tour = 0;
	std::map<int, int> fdTargetTour;
	std::vector<std::vector<int> > timerWheel(MAX_TOUR);

	while (true)
	{
		if (timeout == 0)
		{
			timeout = TIME_OUT;
			tour = (tour + 1) % MAX_TOUR;

			for (int i = 0; i < timerWheel[tour].size(); i++)
			{
				int	expired_fd = timerWheel[tour].at(i);
				if (expired_fd == -1)
					continue ;

				if (fdTargetTour[expired_fd] == tour)
				{
					epoll_ctl(epollFd, EPOLL_CTL_DEL, expired_fd, NULL);
					close(expired_fd);
					fdTargetTour.erase(expired_fd);
					clients.erase(expired_fd);
				}
			}
			timerWheel[tour].clear(); 
		}

		int	readyNum = epoll_wait(epollFd, &epollEvents[0], BUFFER_SIZE, timeout);
		if (readyNum == -1)
		{
			if (errno == EINTR)
				continue ;

			throw (ListenAndAcceptReqs::ListenOrAcceptionError());
		}
		else if (readyNum > 0)
		{
			for (int i = 0; i < readyNum; i++)
			{
				int	currentFd = epollEvents.at(i).data.fd;

				if (currentFd == socketFd)
				{
					int	clientSocket = accept(socketFd, NULL, NULL);
					if (clientSocket == -1)
					{
						std::cerr << "One connecttion cannot accept."
							<< std::endl;
						continue ;
					}

					int flags = fcntl(clientSocket, F_GETFL, 0);
					if (flags == -1 || 
						fcntl(clientSocket, F_SETFL, flags | O_NONBLOCK) == -1)
					{
						std::cerr << "One connecttion cannot adjust as NONBLOCK."
							<< std::endl;
						close(clientSocket);
						continue ;
					}

					struct epoll_event event;
					event.data.fd = clientSocket;
					event.events = EPOLLIN;
					if (epoll_ctl(epollFd, EPOLL_CTL_ADD, clientSocket, &event) == -1)
					{
						std::cerr << "One connection cannot add to epoll."
							<< std::endl;
						close(clientSocket);
						continue ;
					}
					
					struct Client client;
					client.clientFd = clientSocket;
					clients[clientSocket] = client;

					fdTargetTour[clientSocket] = (tour + 2) % MAX_TOUR;
					timerWheel[(tour + 2) % MAX_TOUR].push_back(clientSocket);
				}
				else
				{
					int	newTourNum = (tour + 2) % MAX_TOUR;
					if (fdTargetTour[currentFd] != newTourNum)
					{
						timerWheel[newTourNum].push_back(currentFd);
						fdTargetTour[currentFd] = newTourNum;
					}

					// PROGRAM MUST RUN A CLASS THAT CLASS WILL DO JOB FOR REQ.
				}
				// Program yeni gelen baglantiyi aliyor.
				// Eskisi cok eskirse cikariyor.
				// Artık parçalanan paketlere bakmalı.
				// İstekleri irdelemeli.
			}
		}
	}
}
