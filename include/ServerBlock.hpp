#ifndef SERVER_BLOCK_HPP
#define SERVER_BLOCK_HPP

#include <string>
#include <vector>
#include <map>
#include "LocationBlock.hpp"

class ServerBlock {
private:
	int _port;
	size_t _clientMaxBodySize;
	std::string _ip;
	std::map<std::string, std::string> _errorPages;
	std::vector<LocationBlock> _locations;
	std::vector<std::string> _serverNames;

	typedef void (ServerBlock::*func)(const std::vector<std::string>&, size_t&);

	std::map<std::string, func> _directives;
	void _setupDirectives();

	void _parseListen(const std::vector<std::string>& tokens, size_t& i);
	void _parseServerName(const std::vector<std::string>& tokens, size_t& i);
	void _parseErrorPage(const std::vector<std::string>& tokens, size_t& i);
	void _parseClientMaxBodySize(const std::vector<std::string>& tokens, size_t& i);
	void _parseLocation(const std::vector<std::string>& tokens, size_t& i);

public:
	ServerBlock();
	~ServerBlock();

	void parseServerBlock(const std::vector<std::string>& tokens, size_t& i);

	int getPort() const { return _port; }
	size_t getClientMaxBodySize() const { return _clientMaxBodySize; }
	std::string getIp() const { return _ip; }
	std::map<std::string, std::string> getErrorPages() const { return _errorPages; }
	std::vector<LocationBlock> getLocations() const { return _locations; }
	std::vector<std::string> getServerNames() const { return _serverNames; }

	void setPort(int port) { _port = port; }
	void setClientMaxBodySize(size_t clientMaxBodySize) { _clientMaxBodySize = clientMaxBodySize; }
	void setIp(const std::string& ip) { _ip = ip; }
	void setErrorPages(const std::map<std::string, std::string>& errorPages) { _errorPages = errorPages; }
	void setLocations(const std::vector<LocationBlock>& locations) { _locations = locations; }
	void setServerNames(const std::vector<std::string>& serverNames) { _serverNames = serverNames; }
};

#endif
