#ifndef WEBSERV_CONFIG_HPP
#define WEBSERV_CONFIG_HPP

#include <string>
#include <vector>
#include "ServerBlock.hpp"

class WebservConfig {
private:
	std::vector<ServerBlock> _servers;

	void _extractFromFile(const std::string& filename, std::string& raw);
	void _tokenize(const std::string& raw, std::vector<std::string>& tokens);
	void _parse(const std::vector<std::string>& tokens);

public:
	WebservConfig(const std::string& filename);

	const std::vector<ServerBlock>& getServers() const;
};

#endif
