#include "ListenAndAcceptReqs.hpp"
#include "RequestHandler.hpp"
#include "Response.hpp"
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <csignal>
#include <sstream>
#include <cctype>
#include <string>

static const std::size_t MAX_CGI_RESPONSE_SIZE = 128 * 1024 * 1024;
static const std::size_t MAX_CGI_HEADER_SIZE = 32 * 1024;
static const std::size_t CGI_IO_BUFFER_SIZE = 64 * 1024;

static std::string	buildCgiHttpResponseHeader(
		const std::string& headerBlock, std::size_t bodySize) {
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

		const std::size_t	colon = line.find(':');
		if (colon == std::string::npos || colon == 0)
			continue;
		std::string	key = line.substr(0, colon);
		while (!key.empty() && (key[key.size() - 1] == ' '
			|| key[key.size() - 1] == '\t'))
			key.erase(key.size() - 1);
		for (std::size_t i = 0; i < key.size(); ++i)
			key[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(key[i])));

		if (key == "status") {
			std::stringstream	ss(line.substr(colon + 1));
			ss >> status >> std::ws;
			std::getline(ss, reason);
			if (reason.empty())
				reason = "CGI Response";
		} else if (key != "content-length" && key != "transfer-encoding"
			&& key != "connection") {
			outHeaders += line;
			outHeaders += "\r\n";
		}
	}

	std::stringstream	resp;
	resp << "HTTP/1.1 " << status << " " << reason << "\r\n";
	resp << outHeaders;
	resp << "Content-Length: "
		<< static_cast<long>(bodySize) << "\r\n";
	resp << "Connection: close\r\n\r\n";
	return (resp.str());
}

static void	closeCgiSpool(CgiSpoolState& spool) {
	if (spool.readFd != -1)
		close(spool.readFd);
	if (spool.writeFd != -1)
		close(spool.writeFd);
	spool.readFd = -1;
	spool.writeFd = -1;
	spool.bytesReceived = 0;
	spool.bodyRemaining = 0;
}

static bool	writeAllToFile(int fd, const char* data, std::size_t size) {
	std::size_t	written = 0;
	while (written < size) {
		const ssize_t	count = write(fd, data + written, size - written);
		if (count == -1 && errno == EINTR)
			continue;
		if (count <= 0)
			return (false);
		written += static_cast<std::size_t>(count);
	}
	return (true);
}

static bool	prepareCgiResponse(Client& client) {
	if (client.cgiSpool.readFd == -1 || client.cgiSpool.bytesReceived == 0)
		return (false);

	std::size_t	prefixSize = client.cgiSpool.bytesReceived;
	if (prefixSize > MAX_CGI_HEADER_SIZE + 4)
		prefixSize = MAX_CGI_HEADER_SIZE + 4;

	std::string	prefix;
	char	buffer[4096];
	while (prefix.size() < prefixSize) {
		std::size_t	toRead = prefixSize - prefix.size();
		if (toRead > sizeof(buffer))
			toRead = sizeof(buffer);
		const ssize_t	count = read(client.cgiSpool.readFd, buffer, toRead);
		if (count == -1 && errno == EINTR)
			continue;
		if (count <= 0)
			return (false);
		prefix.append(buffer, static_cast<std::size_t>(count));
	}

	std::size_t	separator = prefix.find("\r\n\r\n");
	std::size_t	separatorLength = 4;
	const std::size_t	lfSeparator = prefix.find("\n\n");
	if (separator == std::string::npos
		|| (lfSeparator != std::string::npos && lfSeparator < separator)) {
		separator = lfSeparator;
		separatorLength = 2;
	}

	std::size_t	bodyOffset = 0;
	std::string	headerBlock;
	if (separator != std::string::npos && separator <= MAX_CGI_HEADER_SIZE) {
		headerBlock = prefix.substr(0, separator);
		bodyOffset = separator + separatorLength;
	}
	if (bodyOffset > client.cgiSpool.bytesReceived || bodyOffset > prefix.size())
		return (false);

	client.response = buildCgiHttpResponseHeader(headerBlock,
		client.cgiSpool.bytesReceived - bodyOffset);
	client.response.append(prefix, bodyOffset, prefix.size() - bodyOffset);
	client.responseOffset = 0;
	client.cgiSpool.bodyRemaining = client.cgiSpool.bytesReceived - prefix.size();
	return (true);
}

static bool	loadNextCgiChunk(Client& client) {
	if (client.cgiSpool.readFd == -1 || client.cgiSpool.bodyRemaining == 0)
		return (false);

	std::size_t	toRead = client.cgiSpool.bodyRemaining;
	if (toRead > CGI_IO_BUFFER_SIZE)
		toRead = CGI_IO_BUFFER_SIZE;
	char	buffer[CGI_IO_BUFFER_SIZE];
	ssize_t	count;
	do {
		count = read(client.cgiSpool.readFd, buffer, toRead);
	} while (count == -1 && errno == EINTR);
	if (count <= 0 || static_cast<std::size_t>(count) > client.cgiSpool.bodyRemaining)
		return (false);

	client.response.assign(buffer, static_cast<std::size_t>(count));
	client.responseOffset = 0;
	client.cgiSpool.bodyRemaining -= static_cast<std::size_t>(count);
	return (true);
}

size_t	ListenAndAcceptReqs::_getMaxBodySize(int port) const
{
	const std::vector<ServerBlock>&	servers = config.getServers();
	size_t	maxBodySize = 0;
	for (std::size_t i = 0; i < servers.size(); ++i)
	{
		if (servers[i].getPort() != port)
			continue;
		if (servers[i].getClientMaxBodySize() == 0)
			return (static_cast<size_t>(-1));
		if (servers[i].getClientMaxBodySize() > maxBodySize)
			maxBodySize = servers[i].getClientMaxBodySize();
		const std::vector<LocationBlock>& locations = servers[i].getLocations();
		for (std::size_t j = 0; j < locations.size(); ++j)
		{
			if (!locations[j].hasClientMaxBodySize())
				continue;
			if (locations[j].getClientMaxBodySize() == 0)
				return (static_cast<size_t>(-1));
			if (locations[j].getClientMaxBodySize() > maxBodySize)
				maxBodySize = locations[j].getClientMaxBodySize();
		}
	}
	if (maxBodySize != 0)
		return (maxBodySize);
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

		_releaseClientResources(it->second);
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

void	ListenAndAcceptReqs::_releaseClientResources(Client& client)
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
		while (waitpid(client.cgiPid, NULL, 0) == -1 && errno == EINTR)
			;
		client.cgiPid = -1;
	}

	closeCgiSpool(client.cgiSpool);

	file.closeFile(client.clientFd);
	client.requestBodyFd = -1;
	client.cgiActive = false;
}

void	ListenAndAcceptReqs::cleanupClient(int fd, std::map<int, int>& fdTargetTour)
{
	std::map<int, Client>::iterator	it = clients.find(fd);
	if (it != clients.end())
		_releaseClientResources(it->second);

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
	client.responseOffset = 0;

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

	char	buf[CGI_IO_BUFFER_SIZE];
	ssize_t	n = read(cgiFd, buf, sizeof(buf));
	if (n == -1)
	{
		if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
			return;
		epoll_ctl(epollFd, EPOLL_CTL_DEL, cgiFd, NULL);
		cgiReadFdToClientFd.erase(cgiFd);
		close(cgiFd);
		client.cgiOutFd = -1;
		_releaseClientResources(client);
		if (!_sendErrorAndMod(clientFd, client, HTTP_BAD_GATEWAY))
			cleanupClient(clientFd, fdTargetTour);
		return;
	}

	if (n > 0)
	{
		const std::size_t	amount = static_cast<std::size_t>(n);
		if (client.cgiSpool.bytesReceived > MAX_CGI_RESPONSE_SIZE
			|| amount > MAX_CGI_RESPONSE_SIZE - client.cgiSpool.bytesReceived) {
			epoll_ctl(epollFd, EPOLL_CTL_DEL, cgiFd, NULL);
			cgiReadFdToClientFd.erase(cgiFd);
			close(cgiFd);
			client.cgiOutFd = -1;
			_releaseClientResources(client);
			if (!_sendErrorAndMod(clientFd, client, HTTP_BAD_GATEWAY))
				cleanupClient(clientFd, fdTargetTour);
			return;
		}
		if (!writeAllToFile(client.cgiSpool.writeFd, buf, amount)) {
			epoll_ctl(epollFd, EPOLL_CTL_DEL, cgiFd, NULL);
			cgiReadFdToClientFd.erase(cgiFd);
			close(cgiFd);
			client.cgiOutFd = -1;
			_releaseClientResources(client);
			if (!_sendErrorAndMod(clientFd, client, HTTP_INTERNAL_SERVER_ERROR))
				cleanupClient(clientFd, fdTargetTour);
			return;
		}
		client.cgiSpool.bytesReceived += amount;
		return;
	}

	epoll_ctl(epollFd, EPOLL_CTL_DEL, cgiFd, NULL);
	cgiReadFdToClientFd.erase(cgiFd);
	close(cgiFd);
	client.cgiOutFd = -1;
	if (client.cgiSpool.writeFd != -1) {
		close(client.cgiSpool.writeFd);
		client.cgiSpool.writeFd = -1;
	}

	if (client.cgiPid > 0
		&& waitpid(client.cgiPid, NULL, WNOHANG) == client.cgiPid)
		client.cgiPid = -1;

	if (client.cgiSpool.bytesReceived == 0)
	{
		_releaseClientResources(client);
		if (!_sendErrorAndMod(clientFd, client, HTTP_BAD_GATEWAY))
			cleanupClient(clientFd, fdTargetTour);
		return;
	}

	if (!prepareCgiResponse(client)) {
		_releaseClientResources(client);
		if (!_sendErrorAndMod(clientFd, client, HTTP_INTERNAL_SERVER_ERROR))
			cleanupClient(clientFd, fdTargetTour);
		return;
	}
	client.cgiActive = false;

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

		for (std::map<int, time_t>::iterator bit = blockedListeners.begin(); bit != blockedListeners.end(); )
		{
			if (now < bit->second)
			{
				++bit;
				continue;
			}

			struct epoll_event enableEvent;
			enableEvent.events = EPOLLIN;
			enableEvent.data.fd = bit->first;
			if (epoll_ctl(epollFd, EPOLL_CTL_MOD, bit->first, &enableEvent) != -1)
				blockedListeners.erase(bit++);
			else
				++bit;
		}

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

					struct sockaddr_in	addr;
					socklen_t			addrLen = sizeof(addr);
					std::memset(&addr, 0, sizeof(addr));
					int	clientSocket = accept(currentFd, reinterpret_cast<struct sockaddr*>(&addr), &addrLen);
					if (clientSocket == -1)
					{
						if (errno == EMFILE || errno == ENFILE)
						{
							struct epoll_event disableEvent;
							disableEvent.events = 0;
							disableEvent.data.fd = currentFd;
							epoll_ctl(epollFd, EPOLL_CTL_MOD, currentFd, &disableEvent);
							blockedListeners[currentFd] = now + 1;
						}
						std::cerr << "One connection cannot accept."
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
					unsigned long	ip = ntohl(addr.sin_addr.s_addr);
					std::stringstream	ipStream;
					ipStream << ((ip >> 24) & 0xFF) << '.' << ((ip >> 16) & 0xFF) << '.'
						<< ((ip >> 8) & 0xFF) << '.' << (ip & 0xFF);
					client.clientIp = ipStream.str();
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
				if (clients.find(currentFd) == clients.end())
					continue ;

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
						if (epoll_ctl(epollFd, EPOLL_CTL_ADD,
							clients[currentFd].cgiOutFd, &cgiEvent) == -1)
							disconnect = true;
						else
						{
							cgiReadFdToClientFd[clients[currentFd].cgiOutFd]
								= currentFd;
							file.closeFile(currentFd);
							clients[currentFd].requestBodyFd = -1;
							clients[currentFd].requestBodyPath.clear();
							std::string().swap(clients[currentFd].requestBody);

							struct epoll_event waitEvent;
							waitEvent.data.fd = currentFd;
							waitEvent.events = 0;
							if (epoll_ctl(epollFd, EPOLL_CTL_MOD, currentFd,
								&waitEvent) == -1)
								disconnect = true;
						}
					}
					else
					{
						struct epoll_event writeEvent;
						writeEvent.data.fd = currentFd;
						writeEvent.events = EPOLLOUT;
						if (epoll_ctl(epollFd, EPOLL_CTL_MOD, currentFd,
							&writeEvent) == -1)
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
					Client& client = clients[currentFd];
					std::string& resp = client.response;
					if (resp.empty() || client.responseOffset >= resp.size())
					{
						cleanupClient(currentFd, fdTargetTour);
						continue ;
					}

					ssize_t sent;
					do
					{
						sent = send(currentFd,
							resp.data() + client.responseOffset,
							resp.size() - client.responseOffset, 0);
					}
					while (sent == -1 && errno == EINTR);
					if (sent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
						continue ;
					if (sent <= 0)
					{
						cleanupClient(currentFd, fdTargetTour);
						continue ;
					}
					client.responseOffset += static_cast<std::size_t>(sent);
					if (client.responseOffset == resp.size())
					{
						if (client.cgiSpool.bodyRemaining == 0)
						{
							cleanupClient(currentFd, fdTargetTour);
							continue ;
						}
						if (!loadNextCgiChunk(client))
						{
							cleanupClient(currentFd, fdTargetTour);
							continue ;
						}
						int	newTourNum = (tour + TIME_OUT) % MAX_TOUR;
						if (fdTargetTour[currentFd] != newTourNum)
						{
							timerWheel[newTourNum].push_back(currentFd);
							fdTargetTour[currentFd] = newTourNum;
						}
						continue ;
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
