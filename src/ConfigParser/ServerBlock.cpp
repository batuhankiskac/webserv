#include "ServerBlock.hpp"
#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

ServerBlock::ServerBlock() : _port(80), _clientMaxBodySize(1048576), _ip("0.0.0.0") {
	_setupDirectives();
}

ServerBlock::~ServerBlock() { }

void ServerBlock::_setupDirectives() {
	_directives["listen"] = &ServerBlock::_parseListen;
	_directives["server_name"] = &ServerBlock::_parseServerName;
	_directives["error_page"] = &ServerBlock::_parseErrorPage;
	_directives["client_max_body_size"] = &ServerBlock::_parseClientMaxBodySize;
	_directives["location"] = &ServerBlock::_parseLocation;
}

void ServerBlock::parseServerBlock(const std::vector<std::string>& _tokens, size_t& i) {
	if (i + 1 >= _tokens.size() || _tokens[i + 1] != "{") {
		throw std::runtime_error("Invalid server block");
	}
	i++;

	while (i < _tokens.size() && _tokens[i] != "}") {
		std::map<std::string, func>::iterator it = _directives.find(_tokens[i]);
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

	std::string value = _tokens[i++];

	size_t colon = value.find(':');
	if (colon == std::string::npos) {
		std::stringstream ss(value);
		ss >> _port;
		if (ss.fail()) {
			throw std::runtime_error("Invalid listen directive");
		}
	} else {
		_ip = value.substr(0, colon);
		std::stringstream ss(value.substr(colon + 1));
		ss >> _port;
		if (ss.fail()) {
			throw std::runtime_error("Invalid listen directive");
		}
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

	std::vector<std::string> values;
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

	std::string path = values.back();
	values.pop_back();
	for (size_t j = 0; j < values.size() - 1; j++) {
		_errorPages[values[j]] = path;
	}
	i++;
}

void ServerBlock::_parseClientMaxBodySize(const std::vector<std::string>& _tokens, size_t& i) {
	i++;

	if (i >= _tokens.size() || _tokens[i] == "}") {
		throw std::runtime_error("Invalid client_max_body_size directive");
	}

	std::string value = _tokens[i++];
	size_t multiplier = 1;
	char unit = value[value.size() - 1];

	switch (unit) {
		case 'k':
		case 'K':
			multiplier = 1024;
			value = value.substr(0, value.size() - 1);
			break;
		case 'm':
		case 'M':
			multiplier = 1024 * 1024;
			value = value.substr(0, value.size() - 1);
			break;
		case 'g':
		case 'G':
			multiplier = 1024 * 1024 * 1024;
			value = value.substr(0, value.size() - 1);
			break;
		default:
			throw std::runtime_error("Invalid client_max_body_size directive");
	}

	std::stringstream ss(value);
	size_t size;
	ss >> size;
	if (ss.fail()) {
		throw std::runtime_error("Invalid client_max_body_size directive");
	}

	_clientMaxBodySize = size * multiplier;

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

	std::string path = _tokens[i++];

	LocationBlock location(path);
	location.parseLocationBlock(_tokens, i);
	_locations.push_back(location);
}
