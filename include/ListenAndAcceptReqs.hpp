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
#include <set>
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
		std::map<int, std::size_t> listenFdToServerIndex;
		std::vector<struct epoll_event> epollEvents;
		std::map<int, Client> clients;
		std::map<int, time_t> blockedListeners;

		std::map<int, int> cgiReadFdToClientFd;
		std::set<pid_t> pendingCgiChildren;

		File& file;
		const WebservConfig& config;

		size_t	_getMaxBodySize(std::size_t serverBlockIndex) const;
		void	_queueChildForReap(pid_t pid, bool terminate);
		void	_reapChildrenNonBlocking();
		void	_releaseClientResources(Client& client);
		void	cleanupClient(int fd, std::map<int, int>& fdTargetTour);
		bool	_sendErrorAndMod(int fd, Client& client, int code);
		void	_handleCgiRead(int cgiFd, std::map<int, int>& fdTargetTour);
		void	_handleCgiReadSafely(int cgiFd, std::map<int, int>& fdTargetTour);

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
