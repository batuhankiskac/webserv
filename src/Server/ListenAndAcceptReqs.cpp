#include "ListenAndAcceptReqs.hpp"
#include "RequestHandler.hpp"
#include "Response.hpp"

ListenAndAcceptReqs::ListenAndAcceptReqs(const std::vector<Server*>& servers,
		File& file, const WebservConfig& config)
	: epollFd(-1), file(file), config(config)
{
	if (servers.empty())
		throw (ListenAndAcceptReqs::ListenOrAcceptionError());

	epollFd = epoll_create(1);
	if (epollFd == -1)
		throw (ListenAndAcceptReqs::ListenOrAcceptionError());

	for (std::size_t i = 0; i < servers.size(); ++i)
	{
		int	listenFd = servers[i]->getSocketFd();
		int	port = servers[i]->getPort();

		if (listen(listenFd, SOMAXCONN) < 0)
		{
			close(epollFd);
			throw (ListenAndAcceptReqs::ListenOrAcceptionError());
		}

		struct epoll_event event;
		event.events = EPOLLIN;
		event.data.fd = listenFd;

		if (epoll_ctl(epollFd, EPOLL_CTL_ADD, listenFd, &event) == -1)
		{
			close(epollFd);
			throw (ListenAndAcceptReqs::ListenOrAcceptionError());
		}

		listenFdToPort[listenFd] = port;
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

void	ListenAndAcceptReqs::cleanupClient(int fd, std::map<int, int>& fdTargetTour)
{
	epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, NULL);
	close(fd);
	fdTargetTour.erase(fd);
	clients.erase(fd);
}

bool	ListenAndAcceptReqs::_sendErrorAndMod(int fd, Client& client, int code)
{
	const std::vector<ServerBlock>&	servers = config.getServers();
	const ServerBlock*	defServer = &servers[0];
	for (std::size_t i = 0; i < servers.size(); ++i)
	{
		if (servers[i].getPort() == client.port)
		{
			defServer = &servers[i];
			break;
		}
	}

	Response	resp = Response::error(code, *defServer);
	client.response = resp.serialize();

	struct epoll_event	modEvent;
	modEvent.data.fd = fd;
	modEvent.events = EPOLLOUT;
	if (epoll_ctl(epollFd, EPOLL_CTL_MOD, fd, &modEvent) == -1)
		return (false);
	return (true);
}

void	ListenAndAcceptReqs::waitReqs()
{
	const int TICK_RATE = 1;
	epollEvents.resize(BUFFER_SIZE);

	time_t	lastTick = time(NULL);
	int	tour = 0;
	std::map<int, int> fdTargetTour;
	std::vector<std::vector<int> > timerWheel(MAX_TOUR);

	while (true)
	{
		const time_t now = time(NULL);

		while (now - lastTick >= TICK_RATE)
		{
			lastTick += TICK_RATE;
			tour = (tour + 1) % MAX_TOUR;

			for (std::size_t i = 0; i < timerWheel[tour].size(); i++)
			{
				int	expired_fd = timerWheel[tour].at(i);
				if (expired_fd == -1)
					continue ;

				if (fdTargetTour.count(expired_fd) && fdTargetTour[expired_fd] == tour)
				{
					epoll_ctl(epollFd, EPOLL_CTL_DEL, expired_fd, NULL);
					close(expired_fd);
					fdTargetTour.erase(expired_fd);
					clients.erase(expired_fd);
				}
			}
			timerWheel[tour].clear(); 
		}

		int	readyNum = epoll_wait(epollFd, &epollEvents[0], BUFFER_SIZE, 1000);
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

				if (epollEvents.at(i).events & (EPOLLERR | EPOLLHUP))
				{
					if (listenFdToPort.count(currentFd))
						continue ;
					cleanupClient(currentFd, fdTargetTour);
					continue ;
				}

				if (listenFdToPort.count(currentFd))
				{
					if (!(epollEvents.at(i).events & EPOLLIN))
						continue ;

					int	clientSocket = accept(currentFd, NULL, NULL);
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
					client.port = listenFdToPort[currentFd];
					clients[clientSocket] = client;

					fdTargetTour[clientSocket] = (tour + TIME_OUT) % MAX_TOUR;
					timerWheel[(tour + TIME_OUT) % MAX_TOUR].push_back(clientSocket);
					continue ;
				}

				if (epollEvents.at(i).events & EPOLLIN)
				{
					int	update = 0;
					bool disconnect = false;

				const int result = Request::readFd(clients[currentFd], file);
				if (!result && !Request::getErrno())
				{
					update = 1;
					RequestHandler::handle(clients[currentFd], config, clients[currentFd].port);

					struct epoll_event modEvent;
						modEvent.data.fd = currentFd;
						modEvent.events = EPOLLOUT;
						if (epoll_ctl(epollFd, EPOLL_CTL_MOD, currentFd, &modEvent) == -1)
						{
							disconnect = true;
						}
					}
					else if (!result && Request::getErrno() == -1)
					{
						disconnect = true;
					}
					else if (result > 0)
					{
						update = 1;
					}
					else
					{
						const int	_errno = Request::getErrno();
						if (_errno == -1)
						{
							// Does nothing. 
						}
						else if (_errno > 0)
						{
							if (_errno == ECONNRESET || _errno == EPIPE || _errno == ETIMEDOUT)
							{
								disconnect = true;
							}
							else
							{
								if (!_sendErrorAndMod(currentFd, clients[currentFd], HTTP_INTERNAL_SERVER_ERROR))
									disconnect = true;
								else
									update = 1;
							}
						}
						else if (_errno == -2)
						{
							const int err = this->file.getErr();
							int code;
							if (err == EMFILE || err == ENFILE)
								code = HTTP_SERVICE_UNAVAILABLE;
							else
								code = HTTP_INTERNAL_SERVER_ERROR;
							if (!_sendErrorAndMod(currentFd, clients[currentFd], code))
								disconnect = true;
							else
								update = 1;
						}
						else
						{
							if (!_sendErrorAndMod(currentFd, clients[currentFd], -_errno))
								disconnect = true;
							else
								update = 1;
						}
					}

					if (disconnect)
					{
						cleanupClient(currentFd, fdTargetTour);
						continue ;
					}
					
					int	newTourNum = (tour + TIME_OUT) % MAX_TOUR;
					if (update && 
						fdTargetTour[currentFd] != newTourNum)
					{
						timerWheel[newTourNum].push_back(currentFd);
						fdTargetTour[currentFd] = newTourNum;
					}
				}
				else if (epollEvents.at(i).events & EPOLLOUT)
				{
					std::string& resp = clients[currentFd].response;
					if (resp.empty())
					{
						cleanupClient(currentFd, fdTargetTour);
						continue ;
					}

					ssize_t sent = send(currentFd, resp.c_str(), resp.size(), 0);
					if (sent == -1)
					{
						if (errno == EAGAIN || errno == EWOULDBLOCK)
						{
							// Yazma tamponu dolu, bir sonraki EPOLLOUT beklenir.
						}
						else
						{
							cleanupClient(currentFd, fdTargetTour);
							continue ;
						}
					}
					else if (sent == 0)
					{
						cleanupClient(currentFd, fdTargetTour);
						continue ;
					}
					else if (sent == static_cast<ssize_t>(resp.size()))
					{
						cleanupClient(currentFd, fdTargetTour);
						continue ;
					}
					else
					{
						resp.erase(0, sent);
					}

					int	newTourNum = (tour + TIME_OUT) % MAX_TOUR;
					if (fdTargetTour[currentFd] != newTourNum)
					{
						timerWheel[newTourNum].push_back(currentFd);
						fdTargetTour[currentFd] = newTourNum;
					}
				}
			}
		}
	}
}
