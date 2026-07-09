#include "ServerBlock.hpp"
#include <cctype>
#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

ServerBlock::ServerBlock() : _port(80), _clientMaxBodySize(1048576), _ip("0.0.0.0") {
	_setupDirectives();
}

void ServerBlock::_setupDirectives() {
	_directives["listen"] = &ServerBlock::_parseListen;
	_directives["server_name"] = &ServerBlock::_parseServerName;
	_directives["error_page"] = &ServerBlock::_parseErrorPage;
	_directives["client_max_body_size"] = &ServerBlock::_parseClientMaxBodySize;
	_directives["location"] = &ServerBlock::_parseLocation;
}

size_t ServerBlock::_parseSizeWithUnit(const std::string& value, const std::string& name) {
	if (value.empty()) {
		throw std::runtime_error("Invalid " + name + " directive");
	}

	size_t	multiplier = 1;
	std::string	numStr = value;
	char	unit = value[value.size() - 1];

	switch (unit) {
		case 'k':
		case 'K':
			multiplier = 1024;
			numStr = value.substr(0, value.size() - 1);
			break;
		case 'm':
		case 'M':
			multiplier = 1024 * 1024;
			numStr = value.substr(0, value.size() - 1);
			break;
		case 'g':
		case 'G':
			multiplier = 1024 * 1024 * 1024;
			numStr = value.substr(0, value.size() - 1);
			break;
		default:
			if (!std::isdigit(unit)) {
				throw std::runtime_error("Invalid " + name + " directive");
			}
			break;
	}

	std::stringstream	ss(numStr);
	size_t	size;
	ss >> size;
	if (ss.fail()) {
		throw std::runtime_error("Invalid " + name + " directive");
	}

	if (size > (size_t)-1 / multiplier) {
		throw std::runtime_error("Invalid " + name + " directive");
	}

	return size * multiplier;
}

void ServerBlock::parseServerBlock(const std::vector<std::string>& _tokens, size_t& i) {
	i++;

	if (i >= _tokens.size() || _tokens[i] != "{") {
		throw std::runtime_error("Invalid server block");
	}
	i++;

	while (i < _tokens.size() && _tokens[i] != "}") {
		std::map<std::string, func>::iterator	it = _directives.find(_tokens[i]);
		if (it != _directives.end()) {
			(this->*it->second)(_tokens, i);
		} else {
			throw std::runtime_error("Invalid token: " + _tokens[i]);
		}
	}

	if (i >= _tokens.size() || _tokens[i] != "}") {
		throw std::runtime_error("Invalid server block");
	}
	i++;
}

void ServerBlock::_parseListen(const std::vector<std::string>& _tokens, size_t& i) {
	i++;

	if (i >= _tokens.size() || _tokens[i] == "}") {
		throw std::runtime_error("Invalid listen directive");
	}

	std::string	value = _tokens[i++];

	std::string	portStr;
	size_t	colon = value.find(':');
	if (colon == std::string::npos) {
		portStr = value;
	} else {
		_ip = value.substr(0, colon);
		portStr = value.substr(colon + 1);
	}

	std::stringstream	ss(portStr);
	ss >> _port;
	if (ss.fail()) {
		throw std::runtime_error("Invalid listen directive");
	}

	if (i >= _tokens.size() || _tokens[i] != ";") {
		throw std::runtime_error("Invalid listen directive");
	}
	i++;
}

void ServerBlock::_parseServerName(const std::vector<std::string>& _tokens, size_t& i) {
	i++;

	while (i < _tokens.size() && _tokens[i] != ";") {
		_serverNames.push_back(_tokens[i++]);
	}

	if (i >= _tokens.size() || _tokens[i] != ";") {
		throw std::runtime_error("Invalid server_name directive");
	}
	i++;
}

void ServerBlock::_parseErrorPage(const std::vector<std::string>& _tokens, size_t& i) {
	i++;

	std::vector<std::string>	values;
	while (i < _tokens.size() && _tokens[i] != ";") {
		values.push_back(_tokens[i++]);
	}

	if (i >= _tokens.size() || _tokens[i] != ";") {
		throw std::runtime_error("Invalid error_page directive");
	}
	i++;

	if (values.size() < 2) {
		throw std::runtime_error("Invalid error_page directive");
	}

	std::string	path = values.back();
	values.pop_back();
	for (size_t j = 0; j < values.size(); j++) {
		_errorPages[values[j]] = path;
	}
}

void ServerBlock::_parseClientMaxBodySize(const std::vector<std::string>& _tokens, size_t& i) {
	i++;

	if (i >= _tokens.size() || _tokens[i] == "}") {
		throw std::runtime_error("Invalid client_max_body_size directive");
	}

	std::string	value = _tokens[i++];
	_clientMaxBodySize = _parseSizeWithUnit(value, "client_max_body_size");

	if (i >= _tokens.size() || _tokens[i] != ";") {
		throw std::runtime_error("Invalid client_max_body_size directive");
	}
	i++;
}

void ServerBlock::_parseLocation(const std::vector<std::string>& _tokens, size_t& i) {
	i++;

	if (i >= _tokens.size() || _tokens[i] == "}") {
		throw std::runtime_error("Invalid location directive");
	}

	std::string	path = _tokens[i++];

	LocationBlock	location(path);
	location.parseLocationBlock(_tokens, i);
	_locations.push_back(location);
}

int ServerBlock::getPort() const {
	return _port;
}

const std::string& ServerBlock::getIp() const {
	return _ip;
}

size_t ServerBlock::getClientMaxBodySize() const {
	return _clientMaxBodySize;
}

const std::vector<std::string>& ServerBlock::getServerNames() const {
	return _serverNames;
}

const std::map<std::string, std::string>& ServerBlock::getErrorPages() const {
	return _errorPages;
}

const std::vector<LocationBlock>& ServerBlock::getLocations() const {
	return _locations;
}
