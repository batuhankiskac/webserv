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
static const std::size_t CGI_IO_BUFFER_SIZE = 32 * 1024;
static const std::size_t MAX_ACTIVE_CGI = 4;

enum SendResult {
	SEND_FAILED = -1,
	SEND_PENDING = 0,
	SEND_COMPLETE = 1,
	SEND_PROGRESS = 2
};

static std::string	_toLower(const std::string& value) {
	std::string	lower = value;

	for (std::size_t i = 0; i < lower.size(); ++i)
		lower[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lower[i])));
	return (lower);
}

static std::string	_defaultCgiReason(int status) {
	switch (status) {
		case HTTP_OK: return ("OK");
		case HTTP_CREATED: return ("Created");
		case HTTP_NO_CONTENT: return ("No Content");
		case HTTP_FOUND: return ("Found");
		case HTTP_BAD_REQUEST: return ("Bad Request");
		case HTTP_FORBIDDEN: return ("Forbidden");
		case HTTP_NOT_FOUND: return ("Not Found");
		case HTTP_INTERNAL_SERVER_ERROR: return ("Internal Server Error");
		default: return ("CGI Response");
	}
}

static std::string	buildCgiHttpResponseHeader(const Client& client,
		const std::string& headerBlock, std::size_t bodySize) {
	int	status = HTTP_OK;
	std::string	reason = _defaultCgiReason(status);
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
		while (!key.empty() && (key[key.size() - 1] == ' ' || key[key.size() - 1] == '\t'))
			key.erase(key.size() - 1);
		key = _toLower(key);

		if (key == "status") {
			int	parsedStatus = 0;
			std::string	parsedReason;
			std::stringstream	ss(line.substr(colon + 1));
			ss >> parsedStatus;
			if (parsedStatus >= 100 && parsedStatus <= 599) {
				status = parsedStatus;
				ss >> std::ws;
				std::getline(ss, parsedReason);
				reason = parsedReason.empty() ? _defaultCgiReason(status) : parsedReason;
			}
		} else if (key != "content-length" && key != "transfer-encoding"
			&& key != "connection") {
			outHeaders += line;
			outHeaders += "\r\n";
		}
	}

	std::string	version = client.request.getHttpVersion();
	if (version != "HTTP/1.0" && version != "HTTP/1.1")
		version = "HTTP/1.1";
	std::stringstream	resp;
	resp << version << " " << status << " " << reason << "\r\n";
	resp << outHeaders;
	resp << "Content-Length: " << bodySize << "\r\n";
	resp << "Connection: close\r\n\r\n";
	return (resp.str());
}

static bool	_writeAllToFd(int fd, const char* data, std::size_t size) {
	std::size_t	written = 0;

	while (written < size) {
		const ssize_t	result = write(fd, data + written, size - written);
		if (result > 0) {
			written += static_cast<std::size_t>(result);
			continue;
		}
		if (result == -1 && errno == EINTR)
			continue;
		return (false);
	}
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
	: epollFd(-1), activeCgiCount(0), file(file), config(config)
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

void	ListenAndAcceptReqs::_releaseCgiSlot(Client& client)
{
	if (!client.cgiSlotHeld)
		return;
	client.cgiSlotHeld = false;
	if (activeCgiCount > 0)
		--activeCgiCount;
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
		const pid_t	pid = client.cgiPid;
		cgiPidToClientFd.erase(pid);
		kill(pid, SIGKILL);
		while (waitpid(pid, NULL, 0) == -1 && errno == EINTR)
			;
		client.cgiPid = -1;
	}
	_releaseCgiSlot(client);

	if (client.cgiBodyFd != -1)
	{
		close(client.cgiBodyFd);
		client.cgiBodyFd = -1;
	}
	if (client.cgiBodyWriteFd != -1)
	{
		close(client.cgiBodyWriteFd);
		client.cgiBodyWriteFd = -1;
	}

	file.closeFile(client.clientFd);
	client.requestBodyFd = -1;
	client.requestBodyPath.clear();
	client.cgiBytesReceived = 0;
	client.cgiBodyRemaining = 0;
	std::string().swap(client.cgiBodyBuffer);
	client.cgiBodyBufferOffset = 0;
	client.cgiState = CGI_NONE;
}

void	ListenAndAcceptReqs::_reapCgiChildren()
{
	while (true)
	{
		const pid_t	pid = waitpid(-1, NULL, WNOHANG);
		if (pid > 0)
		{
			std::map<pid_t, int>::iterator	pit = cgiPidToClientFd.find(pid);
			if (pit == cgiPidToClientFd.end())
				continue;
			std::map<int, Client>::iterator	cit = clients.find(pit->second);
			if (cit != clients.end() && cit->second.cgiPid == pid)
			{
				cit->second.cgiPid = -1;
				_releaseCgiSlot(cit->second);
			}
			cgiPidToClientFd.erase(pit);
			continue;
		}
		if (pid == -1 && errno == EINTR)
			continue;
		break;
	}
}

void	ListenAndAcceptReqs::_refreshTimeout(int fd, int tour,
		std::map<int, int>& fdTargetTour,
		std::vector<std::vector<int> >& timerWheel)
{
	if (clients.find(fd) == clients.end())
		return;
	const int	newTour = (tour + TIME_OUT) % MAX_TOUR;
	std::map<int, int>::iterator	it = fdTargetTour.find(fd);
	if (it == fdTargetTour.end() || it->second != newTour)
	{
		timerWheel[newTour].push_back(fd);
		fdTargetTour[fd] = newTour;
	}
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
	resp.setHttpVersion(client.request.getHttpVersion());
	client.response = resp.serialize();
	client.responseOffset = 0;
	client.cgiState = CGI_NONE;

	struct epoll_event	modEvent;
	modEvent.data.fd = fd;
	modEvent.events = EPOLLOUT;
	if (epoll_ctl(epollFd, EPOLL_CTL_MOD, fd, &modEvent) == -1)
		return (false);
	return (true);
}

void	ListenAndAcceptReqs::_startQueuedCgis(int tour,
		std::map<int, int>& fdTargetTour,
		std::vector<std::vector<int> >& timerWheel)
{
	while (activeCgiCount < MAX_ACTIVE_CGI && !pendingCgiClients.empty())
	{
		const int	clientFd = pendingCgiClients.front();
		pendingCgiClients.pop_front();
		std::map<int, Client>::iterator	cit = clients.find(clientFd);
		if (cit == clients.end() || cit->second.cgiState != CGI_QUEUED)
			continue;

		Client&	client = cit->second;
		RequestHandler::startCgi(client, config, client.port);
		if (client.cgiState != CGI_RUNNING)
		{
			if (!_sendErrorAndMod(clientFd, client, HTTP_INTERNAL_SERVER_ERROR))
				cleanupClient(clientFd, fdTargetTour);
			else
				_refreshTimeout(clientFd, tour, fdTargetTour, timerWheel);
			continue;
		}

		client.cgiSlotHeld = true;
		++activeCgiCount;
		cgiPidToClientFd[client.cgiPid] = clientFd;

		struct epoll_event	cgiEvent;
		cgiEvent.events = EPOLLIN;
		cgiEvent.data.fd = client.cgiOutFd;
		if (epoll_ctl(epollFd, EPOLL_CTL_ADD, client.cgiOutFd, &cgiEvent) == -1)
		{
			_releaseClientResources(client);
			if (!_sendErrorAndMod(clientFd, client, HTTP_INTERNAL_SERVER_ERROR))
				cleanupClient(clientFd, fdTargetTour);
			else
				_refreshTimeout(clientFd, tour, fdTargetTour, timerWheel);
			continue;
		}
		cgiReadFdToClientFd[client.cgiOutFd] = clientFd;

		file.closeFile(clientFd);
		client.requestBodyFd = -1;
		client.requestBodyPath.clear();
		std::string().swap(client.requestBody);

		struct epoll_event	waitEvent;
		waitEvent.events = 0;
		waitEvent.data.fd = clientFd;
		if (epoll_ctl(epollFd, EPOLL_CTL_MOD, clientFd, &waitEvent) == -1)
		{
			cleanupClient(clientFd, fdTargetTour);
			continue;
		}
		_refreshTimeout(clientFd, tour, fdTargetTour, timerWheel);
	}
}

bool	ListenAndAcceptReqs::_prepareCgiResponse(Client& client)
{
	if (client.cgiBodyFd == -1 || client.cgiBytesReceived == 0)
		return (false);

	std::size_t	prefixSize = client.cgiBytesReceived;
	if (prefixSize > MAX_CGI_HEADER_SIZE + 4)
		prefixSize = MAX_CGI_HEADER_SIZE + 4;
	std::string	prefix;
	char	buffer[4096];
	while (prefix.size() < prefixSize)
	{
		std::size_t	toRead = prefixSize - prefix.size();
		if (toRead > sizeof(buffer))
			toRead = sizeof(buffer);
		const ssize_t	count = read(client.cgiBodyFd, buffer, toRead);
		if (count > 0)
		{
			prefix.append(buffer, static_cast<std::size_t>(count));
			continue;
		}
		if (count == -1 && errno == EINTR)
			continue;
		return (false);
	}

	std::size_t	separator = prefix.find("\r\n\r\n");
	std::size_t	separatorLength = 4;
	const std::size_t	lfSeparator = prefix.find("\n\n");
	if (separator == std::string::npos
		|| (lfSeparator != std::string::npos && lfSeparator < separator))
	{
		separator = lfSeparator;
		separatorLength = 2;
	}

	std::size_t	bodyOffset = 0;
	std::string	headerBlock;
	if (separator != std::string::npos && separator <= MAX_CGI_HEADER_SIZE)
	{
		headerBlock = prefix.substr(0, separator);
		bodyOffset = separator + separatorLength;
	}
	if (bodyOffset > client.cgiBytesReceived || bodyOffset > prefix.size())
		return (false);

	client.cgiBodyRemaining = client.cgiBytesReceived - bodyOffset;
	client.response = buildCgiHttpResponseHeader(client, headerBlock,
		client.cgiBodyRemaining);
	client.responseOffset = 0;
	client.cgiBodyBuffer = prefix.substr(bodyOffset);
	client.cgiBodyBufferOffset = 0;
	client.cgiState = CGI_SENDING_RESPONSE;
	return (true);
}

int	ListenAndAcceptReqs::_sendClientData(Client& client)
{
	bool	progress = false;

	if (client.responseOffset < client.response.size())
	{
		ssize_t	sent;
		do
		{
			sent = send(client.clientFd,
				client.response.data() + client.responseOffset,
				client.response.size() - client.responseOffset, 0);
		}
		while (sent == -1 && errno == EINTR);
		if (sent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
			return (SEND_PENDING);
		if (sent <= 0)
			return (SEND_FAILED);
		client.responseOffset += static_cast<std::size_t>(sent);
		progress = true;
		if (client.responseOffset < client.response.size())
			return (SEND_PROGRESS);
	}

	if (client.cgiState != CGI_SENDING_RESPONSE)
		return (SEND_COMPLETE);
	if (client.cgiBodyRemaining == 0)
		return (SEND_COMPLETE);

	if (client.cgiBodyBufferOffset >= client.cgiBodyBuffer.size())
	{
		std::size_t	toRead = client.cgiBodyRemaining;
		if (toRead > CGI_IO_BUFFER_SIZE)
			toRead = CGI_IO_BUFFER_SIZE;
		char	buffer[CGI_IO_BUFFER_SIZE];
		ssize_t	count;
		do
		{
			count = read(client.cgiBodyFd, buffer, toRead);
		}
		while (count == -1 && errno == EINTR);
		if (count <= 0)
			return (SEND_FAILED);
		client.cgiBodyBuffer.assign(buffer, static_cast<std::size_t>(count));
		client.cgiBodyBufferOffset = 0;
	}

	ssize_t	sent;
	do
	{
		sent = send(client.clientFd,
			client.cgiBodyBuffer.data() + client.cgiBodyBufferOffset,
			client.cgiBodyBuffer.size() - client.cgiBodyBufferOffset, 0);
	}
	while (sent == -1 && errno == EINTR);
	if (sent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
		return (progress ? SEND_PROGRESS : SEND_PENDING);
	if (sent <= 0)
		return (SEND_FAILED);
	client.cgiBodyBufferOffset += static_cast<std::size_t>(sent);
	if (client.cgiBodyBufferOffset < client.cgiBodyBuffer.size())
		return (SEND_PROGRESS);
	if (client.cgiBodyBuffer.size() > client.cgiBodyRemaining)
		return (SEND_FAILED);
	client.cgiBodyRemaining -= client.cgiBodyBuffer.size();
	client.cgiBodyBuffer.clear();
	client.cgiBodyBufferOffset = 0;
	return (client.cgiBodyRemaining == 0 ? SEND_COMPLETE : SEND_PROGRESS);
}

void	ListenAndAcceptReqs::_handleCgiRead(int cgiFd, int tour,
		std::map<int, int>& fdTargetTour,
		std::vector<std::vector<int> >& timerWheel)
{
	std::map<int, int>::iterator	it = cgiReadFdToClientFd.find(cgiFd);
	if (it == cgiReadFdToClientFd.end())
		return;

	const int	clientFd = it->second;
	std::map<int, Client>::iterator	cit = clients.find(clientFd);
	if (cit == clients.end())
	{
		epoll_ctl(epollFd, EPOLL_CTL_DEL, cgiFd, NULL);
		cgiReadFdToClientFd.erase(cgiFd);
		close(cgiFd);
		return;
	}
	Client&	client = cit->second;
	char	buffer[CGI_IO_BUFFER_SIZE];

	while (true)
	{
		const ssize_t	count = read(cgiFd, buffer, sizeof(buffer));
		if (count > 0)
		{
			const std::size_t	amount = static_cast<std::size_t>(count);
			if (client.cgiBytesReceived > MAX_CGI_RESPONSE_SIZE
				|| amount > MAX_CGI_RESPONSE_SIZE - client.cgiBytesReceived
				|| !_writeAllToFd(client.cgiBodyWriteFd, buffer, amount))
			{
				_releaseClientResources(client);
				if (!_sendErrorAndMod(clientFd, client, HTTP_BAD_GATEWAY))
					cleanupClient(clientFd, fdTargetTour);
				else
					_refreshTimeout(clientFd, tour, fdTargetTour, timerWheel);
				return;
			}
			client.cgiBytesReceived += amount;
			_refreshTimeout(clientFd, tour, fdTargetTour, timerWheel);
			continue;
		}
		if (count == -1 && errno == EINTR)
			continue;
		if (count == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
			return;
		if (count == -1)
		{
			_releaseClientResources(client);
			if (!_sendErrorAndMod(clientFd, client, HTTP_BAD_GATEWAY))
				cleanupClient(clientFd, fdTargetTour);
			else
				_refreshTimeout(clientFd, tour, fdTargetTour, timerWheel);
			return;
		}
		break;
	}

	epoll_ctl(epollFd, EPOLL_CTL_DEL, cgiFd, NULL);
	cgiReadFdToClientFd.erase(cgiFd);
	close(cgiFd);
	client.cgiOutFd = -1;
	if (client.cgiBodyWriteFd != -1)
	{
		close(client.cgiBodyWriteFd);
		client.cgiBodyWriteFd = -1;
	}

	if (!_prepareCgiResponse(client))
	{
		_releaseClientResources(client);
		if (!_sendErrorAndMod(clientFd, client, HTTP_BAD_GATEWAY))
			cleanupClient(clientFd, fdTargetTour);
		else
			_refreshTimeout(clientFd, tour, fdTargetTour, timerWheel);
		return;
	}

	struct epoll_event	writeEvent;
	writeEvent.data.fd = clientFd;
	writeEvent.events = EPOLLOUT;
	if (epoll_ctl(epollFd, EPOLL_CTL_MOD, clientFd, &writeEvent) == -1)
		cleanupClient(clientFd, fdTargetTour);
	else
		_refreshTimeout(clientFd, tour, fdTargetTour, timerWheel);
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
		_reapCgiChildren();
		_startQueuedCgis(tour, fdTargetTour, timerWheel);

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
						_handleCgiRead(currentFd, tour, fdTargetTour, timerWheel);
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
						_handleCgiRead(currentFd, tour, fdTargetTour, timerWheel);
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

						struct epoll_event modEvent;
						modEvent.data.fd = currentFd;
						if (clients[currentFd].cgiState == CGI_QUEUED)
						{
							pendingCgiClients.push_back(currentFd);
							fdTargetTour.erase(currentFd);
							modEvent.events = 0;
							update = 0;
						}
						else
							modEvent.events = EPOLLOUT;
						if (epoll_ctl(epollFd, EPOLL_CTL_MOD, currentFd, &modEvent) == -1)
							disconnect = true;
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

					if (update)
						_refreshTimeout(currentFd, tour, fdTargetTour, timerWheel);
				}
				else if (epollEvents.at(i).events & EPOLLOUT)
				{
					const int	sendResult = _sendClientData(clients[currentFd]);
					if (sendResult == SEND_FAILED || sendResult == SEND_COMPLETE)
					{
						cleanupClient(currentFd, fdTargetTour);
						continue ;
					}
					if (sendResult == SEND_PROGRESS)
						_refreshTimeout(currentFd, tour, fdTargetTour, timerWheel);
				}
			}
		}
	}
}
