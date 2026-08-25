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
#include <deque>
#include <ctime>
#include <csignal>

#include "Client.hpp"
#include "Request.hpp"
#include "File.hpp"
#include "Server.hpp"
#include "WebservConfig.hpp"

#define BUFFER_SIZE 64
#define TIME_OUT	30
#define MAX_TOUR	50

class ListenAndAcceptReqs
{
	public:
		ListenAndAcceptReqs(const std::vector<Server*>& servers, File& file, const WebservConfig& config);
		~ListenAndAcceptReqs();
		void	waitReqs(const volatile sig_atomic_t& shutdownRequested);

	private:
		int	epollFd;
		std::map<int, int> listenFdToPort;
		std::vector<struct epoll_event> epollEvents;
		std::map<int, Client> clients;
		std::map<int, time_t> blockedListeners;

		std::map<int, int> cgiReadFdToClientFd;
		std::map<pid_t, int> cgiPidToClientFd;
		std::deque<int> pendingCgiClients;
		std::size_t activeCgiCount;

		File& file;
		const WebservConfig& config;

		size_t	_getMaxBodySize(int port) const;
		void	_releaseCgiSlot(Client& client);
		void	_releaseClientResources(Client& client);
		void	_reapCgiChildren();
		void	_refreshTimeout(int fd, int tour, std::map<int, int>& fdTargetTour,
			std::vector<std::vector<int> >& timerWheel);
		void	_startQueuedCgis(int tour, std::map<int, int>& fdTargetTour,
			std::vector<std::vector<int> >& timerWheel);
		bool	_prepareCgiResponse(Client& client);
		int	_sendClientData(Client& client);
		void	cleanupClient(int fd, std::map<int, int>& fdTargetTour);
		bool	_sendErrorAndMod(int fd, Client& client, int code);
		void	_handleCgiRead(int cgiFd, int tour, std::map<int, int>& fdTargetTour,
			std::vector<std::vector<int> >& timerWheel);

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
