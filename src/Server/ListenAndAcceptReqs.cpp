#include "ListenAndAcceptReqs.hpp"
#include "RequestHandler.hpp"
#include "Response.hpp"
#include <sys/wait.h>
#include <csignal>
#include <sstream>
#include <cctype>
#include <string>

static std::string	buildCgiHttpResponse(const std::string& cgiOutput) {
	std::size_t	sep = cgiOutput.find("\r\n\r\n");
	std::size_t	sepLen = 4;
	if (sep == std::string::npos) {
		sep = cgiOutput.find("\n\n");
		sepLen = 2;
	}

	std::string	headerBlock;
	std::string	body;
	if (sep != std::string::npos) {
		headerBlock = cgiOutput.substr(0, sep);
		body = cgiOutput.substr(sep + sepLen);
	} else
		body = cgiOutput;

	int	status = 200;
	std::string	reason = "OK";
	std::string	outHeaders;

	std::stringstream	hs(headerBlock);
	std::string	line;
	while (std::getline(hs, line)) {
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);
		if (line.empty())
			continue;

		std::string	lower = line;
		for (std::size_t i = 0; i < lower.size(); ++i)
			lower[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lower[i])));

		if (lower.size() >= 7 && lower.substr(0, 7) == "status:") {
			std::stringstream	ss(line.substr(7));
			ss >> status >> std::ws;
			std::getline(ss, reason);
		} else {
			outHeaders += line;
			outHeaders += "\r\n";
		}
	}

	std::stringstream	resp;
	resp << "HTTP/1.1 " << status << " " << reason << "\r\n";
	resp << outHeaders;
	resp << "Content-Length: " << static_cast<long>(body.size()) << "\r\n";
	resp << "Connection: close\r\n\r\n" << body;
	return (resp.str());
}

size_t	ListenAndAcceptReqs::_getMaxBodySize(int port) const
{
	const std::vector<ServerBlock>&	servers = config.getServers();
	for (std::size_t i = 0; i < servers.size(); ++i)
	{
		if (servers[i].getPort() == port)
			return (servers[i].getClientMaxBodySize());
	}
	return (servers[0].getClientMaxBodySize());
}

ListenAndAcceptReqs::ListenAndAcceptReqs(const std::vector<Server*>& servers,
		File& file, const WebservConfig& config)
	: epollFd(-1), file(file), config(config)
{
	if (servers.empty())
		throw (ListenAndAcceptReqs::ListenOrAcceptionError());

	epollFd = epoll_create(1);
	if (epollFd == -1)
		throw (ListenAndAcceptReqs::ListenOrAcceptionError());
	fcntl(epollFd, F_SETFD, FD_CLOEXEC);

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
	while (!clients.empty())
	{
		std::map<int, Client>::iterator	it = clients.begin();
		const int	clientFd = it->first;

		_releaseClientResources(it->second, true);
		if (epollFd != -1)
			epoll_ctl(epollFd, EPOLL_CTL_DEL, clientFd, NULL);
		close(clientFd);
		clients.erase(it);
	}

	while (!cgiReadFdToClientFd.empty())
	{
		const int	cgiFd = cgiReadFdToClientFd.begin()->first;
		if (epollFd != -1)
			epoll_ctl(epollFd, EPOLL_CTL_DEL, cgiFd, NULL);
		close(cgiFd);
		cgiReadFdToClientFd.erase(cgiReadFdToClientFd.begin());
	}

	while (waitpid(-1, NULL, WNOHANG) > 0)
		;

	if (epollFd != -1)
	{
		close(epollFd);
		epollFd = -1;
	}
}

ListenAndAcceptReqs::ListenOrAcceptionError::ListenOrAcceptionError()
: _errno(errno) {}

const char* ListenAndAcceptReqs::ListenOrAcceptionError::what() const throw()
{
	return (std::strerror(_errno));
}

void	ListenAndAcceptReqs::_releaseClientResources(Client& client, bool waitForCgi)
{
	if (client.cgiOutFd != -1)
	{
		if (epollFd != -1)
			epoll_ctl(epollFd, EPOLL_CTL_DEL, client.cgiOutFd, NULL);
		cgiReadFdToClientFd.erase(client.cgiOutFd);
		close(client.cgiOutFd);
		client.cgiOutFd = -1;
	}

	if (client.cgiPid > 0)
	{
		kill(client.cgiPid, SIGKILL);
		if (waitForCgi)
		{
			while (waitpid(client.cgiPid, NULL, 0) == -1 && errno == EINTR)
				;
		}
		else
			waitpid(client.cgiPid, NULL, WNOHANG);
		client.cgiPid = -1;
	}

	file.closeFile(client.clientFd);
	client.requestBodyFd = -1;
	client.cgiActive = false;
}

void	ListenAndAcceptReqs::cleanupClient(int fd, std::map<int, int>& fdTargetTour)
{
	std::map<int, Client>::iterator	it = clients.find(fd);
	if (it != clients.end())
		_releaseClientResources(it->second, false);

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

void	ListenAndAcceptReqs::_handleCgiRead(int cgiFd, std::map<int, int>& fdTargetTour)
{
	std::map<int, int>::iterator	it = cgiReadFdToClientFd.find(cgiFd);
	if (it == cgiReadFdToClientFd.end())
		return;

	int	clientFd = it->second;
	std::map<int, Client>::iterator	cit = clients.find(clientFd);
	if (cit == clients.end())
	{
		epoll_ctl(epollFd, EPOLL_CTL_DEL, cgiFd, NULL);
		cgiReadFdToClientFd.erase(cgiFd);
		close(cgiFd);
		return;
	}

	Client&	client = cit->second;

	char	buf[4096];
	ssize_t	n = read(cgiFd, buf, sizeof(buf));
	if (n == -1)
	{
		epoll_ctl(epollFd, EPOLL_CTL_DEL, cgiFd, NULL);
		cgiReadFdToClientFd.erase(cgiFd);
		close(cgiFd);
		client.cgiOutFd = -1;
		if (client.cgiPid > 0)
		{
			kill(client.cgiPid, SIGKILL);
			waitpid(client.cgiPid, NULL, WNOHANG);
			client.cgiPid = -1;
		}
		client.cgiActive = false;
		_sendErrorAndMod(clientFd, client, HTTP_BAD_GATEWAY);
		return;
	}

	if (n > 0)
	{
		if (client.cgiResponse.size() + static_cast<std::size_t>(n) > 8 * 1024 * 1024) {
			epoll_ctl(epollFd, EPOLL_CTL_DEL, cgiFd, NULL);
			cgiReadFdToClientFd.erase(cgiFd);
			close(cgiFd);
			client.cgiOutFd = -1;
			if (client.cgiPid > 0) { kill(client.cgiPid, SIGKILL); waitpid(client.cgiPid, NULL, WNOHANG); client.cgiPid = -1; }
			client.cgiActive = false;
			_sendErrorAndMod(clientFd, client, HTTP_BAD_GATEWAY);
			return;
		}
		client.cgiResponse.append(buf, static_cast<std::size_t>(n));
		return;
	}

	epoll_ctl(epollFd, EPOLL_CTL_DEL, cgiFd, NULL);
	cgiReadFdToClientFd.erase(cgiFd);
	close(cgiFd);
	client.cgiOutFd = -1;

	if (client.cgiPid > 0)
		waitpid(client.cgiPid, NULL, WNOHANG);

	if (client.cgiResponse.empty())
	{
		client.cgiActive = false;
		_sendErrorAndMod(clientFd, client, HTTP_BAD_GATEWAY);
		return;
	}

	client.response = buildCgiHttpResponse(client.cgiResponse);
	client.cgiActive = false;
	client.cgiResponse.clear();

	struct epoll_event	modEvent;
	modEvent.data.fd = clientFd;
	modEvent.events = EPOLLOUT;
	if (epoll_ctl(epollFd, EPOLL_CTL_MOD, clientFd, &modEvent) == -1)
		cleanupClient(clientFd, fdTargetTour);
}

void	ListenAndAcceptReqs::waitReqs(const volatile sig_atomic_t& shutdownRequested)
{
	const int TICK_RATE = 1;
	epollEvents.resize(BUFFER_SIZE);

	time_t	lastTick = std::time(NULL);
	int	tour = 0;
	std::map<int, int> fdTargetTour;
	std::vector<std::vector<int> > timerWheel(MAX_TOUR);

	while (!shutdownRequested)
	{
		const time_t now = std::time(NULL);

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
					cleanupClient(expired_fd, fdTargetTour);
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
					if (cgiReadFdToClientFd.count(currentFd))
					{
						_handleCgiRead(currentFd, fdTargetTour);
						continue ;
					}
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

					if (fcntl(clientSocket, F_SETFL, O_NONBLOCK) == -1)
					{
						std::cerr << "One connection cannot adjust as NONBLOCK."
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
					fcntl(clientSocket, F_SETFD, FD_CLOEXEC);

					fdTargetTour[clientSocket] = (tour + TIME_OUT) % MAX_TOUR;
					timerWheel[(tour + TIME_OUT) % MAX_TOUR].push_back(clientSocket);
					continue ;
				}

				if (cgiReadFdToClientFd.count(currentFd))
				{
					if (epollEvents.at(i).events & EPOLLIN)
						_handleCgiRead(currentFd, fdTargetTour);
					continue ;
				}

				if (epollEvents.at(i).events & EPOLLIN)
				{
					int	update = 0;
					bool disconnect = false;

				const int result = Request::readFd(clients[currentFd], file, _getMaxBodySize(clients[currentFd].port));
				if (!result && !Request::getErrno())
				{
					update = 1;
					RequestHandler::handle(clients[currentFd], config, clients[currentFd].port);

					if (clients[currentFd].cgiActive)
					{
						struct epoll_event cgiEvent;
						cgiEvent.events = EPOLLIN;
						cgiEvent.data.fd = clients[currentFd].cgiOutFd;
						epoll_ctl(epollFd, EPOLL_CTL_ADD, clients[currentFd].cgiOutFd, &cgiEvent);
						cgiReadFdToClientFd[clients[currentFd].cgiOutFd] = currentFd;
					}
					else
					{
						struct epoll_event modEvent;
						modEvent.data.fd = currentFd;
						modEvent.events = EPOLLOUT;
						if (epoll_ctl(epollFd, EPOLL_CTL_MOD, currentFd, &modEvent) == -1)
						{
							disconnect = true;
						}
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
							if (!_sendErrorAndMod(currentFd, clients[currentFd], HTTP_INTERNAL_SERVER_ERROR))
								disconnect = true;
							else
								update = 1;
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
							cleanupClient(currentFd, fdTargetTour);
							continue ;
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
