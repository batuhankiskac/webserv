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

	static size_t _parseSizeWithUnit(const std::string& value, const std::string& name);

	void _parseListen(const std::vector<std::string>& tokens, size_t& i);
	void _parseServerName(const std::vector<std::string>& tokens, size_t& i);
	void _parseErrorPage(const std::vector<std::string>& tokens, size_t& i);
	void _parseClientMaxBodySize(const std::vector<std::string>& tokens, size_t& i);
	void _parseLocation(const std::vector<std::string>& tokens, size_t& i);

public:
	ServerBlock();

	void parseServerBlock(const std::vector<std::string>& tokens, size_t& i);

	int getPort() const;
	const std::string& getIp() const;
	size_t getClientMaxBodySize() const;
	const std::vector<std::string>& getServerNames() const;
	const std::map<std::string, std::string>& getErrorPages() const;
	const std::vector<LocationBlock>& getLocations() const;

};

#endif
