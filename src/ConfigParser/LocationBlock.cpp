#include "LocationBlock.hpp"
#include <sstream>
#include <stdexcept>

LocationBlock::LocationBlock(const std::string& path) : _path(path), _uploadEnable(false), _autoIndex(false), _returnCode(200) {
	_setupDirectives();
}

LocationBlock::~LocationBlock() { }

void LocationBlock::_setupDirectives() {
	_directives["index"] = &LocationBlock::_parseIndex;
	_directives["allow_methods"] = &LocationBlock::_parseAllowMethods;
	_directives["path"] = &LocationBlock::_parsePath;
	_directives["root"] = &LocationBlock::_parseRoot;
	_directives["upload_store"] = &LocationBlock::_parseUploadStore;
	_directives["upload_enable"] = &LocationBlock::_parseUploadEnable;
	_directives["cgi_path"] = &LocationBlock::_parseCgiPath;
	_directives["cgi_ext"] = &LocationBlock::_parseCgiExt;
	_directives["return"] = &LocationBlock::_parseReturn;
	_directives["autoindex"] = &LocationBlock::_parseAutoIndex;
}

void LocationBlock::parseLocationBlock(const std::vector<std::string>& _tokens, size_t& i) {
	if (i >= _tokens.size() || _tokens[i] != "{") {
		throw std::runtime_error("Invalid location block");
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
		throw std::runtime_error("Invalid location block");
	}
	i++;
}

void LocationBlock::_parseSingleValue(const std::vector<std::string>& tokens, size_t& i,
                                      std::string& dest, const std::string& name) {
	i++;
	if (i >= tokens.size() || tokens[i] == ";") {
		throw std::runtime_error("Invalid " + name + " directive");
	}
	dest = tokens[i++];
	if (i >= tokens.size() || tokens[i] != ";") {
		throw std::runtime_error("Invalid " + name + " directive");
	}
	i++;
}

void LocationBlock::_parseOnOff(const std::vector<std::string>& tokens, size_t& i,
                                bool& dest, const std::string& name) {
	i++;
	if (i >= tokens.size() || tokens[i] == ";") {
		throw std::runtime_error("Invalid " + name + " directive");
	}
	std::string value = tokens[i++];
	if (value != "on" && value != "off") {
		throw std::runtime_error("Invalid " + name + " directive");
	}
	dest = (value == "on");
	if (i >= tokens.size() || tokens[i] != ";") {
		throw std::runtime_error("Invalid " + name + " directive");
	}
	i++;
}

void LocationBlock::_parseIndex(const std::vector<std::string>& _tokens, size_t& i) {
	i++;

	while (i < _tokens.size() && _tokens[i] != ";") {
		_index.push_back(_tokens[i++]);
	}

	if (i >= _tokens.size() || _tokens[i] != ";") {
		throw std::runtime_error("Invalid index directive");
	}
	i++;
}

void LocationBlock::_parseAllowMethods(const std::vector<std::string>& _tokens, size_t& i) {
	i++;

	while (i < _tokens.size() && _tokens[i] != ";") {
		std::string method = _tokens[i++];
		if (method != "GET" && method != "POST" && method != "DELETE") {
			throw std::runtime_error("Invalid allow_methods directive");
		}
		_allowMethods.push_back(method);
	}

	if (i >= _tokens.size() || _tokens[i] != ";") {
		throw std::runtime_error("Invalid allow_methods directive");
	}
	i++;
}


void LocationBlock::_parseReturn(const std::vector<std::string>& _tokens, size_t& i) {
	i++;

	if (i >= _tokens.size() || _tokens[i] == ";") {
		throw std::runtime_error("Invalid return directive");
	}

	std::stringstream ss(_tokens[i++]);
	ss >> _returnCode;
	if (ss.fail()) {
		throw std::runtime_error("Invalid return directive");
	}

	if (i < _tokens.size() && _tokens[i] != ";") {
		_returnUrl = _tokens[i++];
	}
	if (i >= _tokens.size() || _tokens[i] != ";") {
		throw std::runtime_error("Invalid return directive");
	}
	i++;
}
