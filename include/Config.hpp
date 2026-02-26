#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>
#include "ServerConfig.hpp"

class Config {
private:
	std::vector<ServerConfig> _servers;
	std::vector<std::string>  _tokens;
	std::string 			  _raw;

	void _extractFromFile(const std::string& filename);
	void _tokenize();
	void _parse();

public:
	Config(const std::string& filename);
	~Config();

	const std::vector<ServerConfig>& getServers() const;
};

#endif
