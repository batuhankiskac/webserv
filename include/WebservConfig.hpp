#ifndef WEBSERV_CONFIG_HPP
#define WEBSERV_CONFIG_HPP

#include <string>
#include <vector>
#include "ServerBlock.hpp"

class WebservConfig {
private:
	std::vector<ServerBlock> _servers;
	std::vector<std::string> _tokens;
	std::string			   _raw;

	void _extractFromFile(const std::string& filename);
	void _tokenize();
	void _parse();

public:
	WebservConfig(const std::string& filename);
	~WebservConfig();

	const std::vector<ServerBlock>& getServers() const;
};

#endif
