#include "WebservConfig.hpp"
#include "Server.hpp"
#include "ListenAndAcceptReqs.hpp"
#include "File.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <set>
#include <utility>
#include <csignal>

static volatile sig_atomic_t	gShutdownRequested = 0;

static void	handleShutdownSignal(int signal) {
	(void)signal;
	gShutdownRequested = 1;
}

int	main(int argc, char** argv) {
	if (argc != 2) {
		std::cerr << "Usage: " << argv[0] << " <configuration_file>" << std::endl;
		return (1);
	}

	std::signal(SIGPIPE, SIG_IGN);
	if (std::signal(SIGINT, handleShutdownSignal) == SIG_ERR
		|| std::signal(SIGTERM, handleShutdownSignal) == SIG_ERR) {
		std::cerr << "webserv: failed to install signal handler" << std::endl;
		return (1);
	}

	std::vector<Server*>	servers;
	File	file;

	try {
		WebservConfig	config(argv[1]);

		file.setPath("/tmp/webserv_body_");

		std::set<std::pair<std::string, int> >	seen;
		const std::vector<ServerBlock>&	blocks = config.getServers();

		for (std::size_t i = 0; i < blocks.size(); ++i) {
			const std::string&	ip = blocks[i].getIp();
			int	port = blocks[i].getPort();
			std::pair<std::string, int>	key(ip, port);

			if (seen.count(key))
				continue ;
			seen.insert(key);

			Server*	srv = new Server(ip, port);
			servers.push_back(srv);
		}

		if (servers.empty())
			throw (std::runtime_error("No server sockets to listen"));

		ListenAndAcceptReqs	listener(servers, file, config);
		listener.waitReqs(gShutdownRequested);
	}
	catch (const std::exception& e) {
		std::cerr << "webserv: " << e.what() << std::endl;
		for (std::size_t i = 0; i < servers.size(); ++i)
			delete servers[i];
		return (1);
	}

	for (std::size_t i = 0; i < servers.size(); ++i)
		delete servers[i];

	return (0);
}
