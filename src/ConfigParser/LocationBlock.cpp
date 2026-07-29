#include "LocationBlock.hpp"
#include <cctype>
#include <sstream>
#include <stdexcept>

LocationBlock::LocationBlock(const std::string& path) : _path(path), _clientMaxBodySize(0),
	_hasClientMaxBodySize(false), _uploadEnable(false), _autoIndex(false), _returnCode(200) {
}

void LocationBlock::parseLocationBlock(const std::vector<std::string>& tokens, size_t& i) {
	if (i >= tokens.size() || tokens[i] != "{") {
		throw std::runtime_error("Invalid location block");
	}
	i++;

	while (i < tokens.size() && tokens[i] != "}") {
		const std::string& tok = tokens[i];
		if      (tok == "root")           _parseSingleValue(tokens, i, _root, "root");
		else if (tok == "path")           _parseSingleValue(tokens, i, _path, "path");
		else if (tok == "upload_store")   _parseSingleValue(tokens, i, _uploadStore, "upload_store");
		else if (tok == "cgi_path")       _parseSingleValue(tokens, i, _cgiPath, "cgi_path");
		else if (tok == "cgi_ext")        _parseSingleValue(tokens, i, _cgiExt, "cgi_ext");
		else if (tok == "upload_enable")  _parseOnOff(tokens, i, _uploadEnable, "upload_enable");
		else if (tok == "autoindex")      _parseOnOff(tokens, i, _autoIndex, "autoindex");
		else if (tok == "index")          _parseIndex(tokens, i);
		else if (tok == "allow_methods")  _parseAllowMethods(tokens, i);
		else if (tok == "return")         _parseReturn(tokens, i);
		else if (tok == "client_max_body_size") _parseClientMaxBodySize(tokens, i);
		else throw std::runtime_error("Invalid token: " + tok);
	}

	if (i >= tokens.size() || tokens[i] != "}") {
		throw std::runtime_error("Invalid location block");
	}
	i++;
}

void LocationBlock::_parseClientMaxBodySize(const std::vector<std::string>& tokens, size_t& i) {
	i++;
	if (i >= tokens.size() || tokens[i] == ";") {
		throw std::runtime_error("Invalid client_max_body_size directive");
	}

	const std::string	value = tokens[i++];
	std::string			number = value;
	size_t				multiplier = 1;
	const char			unit = value.empty() ? '\0' : value[value.size() - 1];

	if (unit == 'k' || unit == 'K') {
		multiplier = 1024;
		number = value.substr(0, value.size() - 1);
	} else if (unit == 'm' || unit == 'M') {
		multiplier = 1024 * 1024;
		number = value.substr(0, value.size() - 1);
	} else if (unit == 'g' || unit == 'G') {
		multiplier = 1024 * 1024 * 1024;
		number = value.substr(0, value.size() - 1);
	} else if (!std::isdigit(static_cast<unsigned char>(unit))) {
		throw std::runtime_error("Invalid client_max_body_size directive");
	}

	if (number.empty()) {
		throw std::runtime_error("Invalid client_max_body_size directive");
	}
	for (size_t j = 0; j < number.size(); ++j) {
		if (!std::isdigit(static_cast<unsigned char>(number[j]))) {
			throw std::runtime_error("Invalid client_max_body_size directive");
		}
	}

	std::stringstream	ss(number);
	size_t				size = 0;
	ss >> size;
	if (ss.fail() || !ss.eof() || size > static_cast<size_t>(-1) / multiplier) {
		throw std::runtime_error("Invalid client_max_body_size directive");
	}
	if (i >= tokens.size() || tokens[i] != ";") {
		throw std::runtime_error("Invalid client_max_body_size directive");
	}
	i++;
	_clientMaxBodySize = size * multiplier;
	_hasClientMaxBodySize = true;
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

const std::string& LocationBlock::getPath() const {
	return _path;
}

const std::string& LocationBlock::getRoot() const {
	return _root;
}

const std::vector<std::string>& LocationBlock::getIndex() const {
	return _index;
}

const std::vector<std::string>& LocationBlock::getAllowMethods() const {
	return _allowMethods;
}

const std::string& LocationBlock::getUploadStore() const {
	return _uploadStore;
}

const std::string& LocationBlock::getCgiPath() const {
	return _cgiPath;
}

const std::string& LocationBlock::getCgiExt() const {
	return _cgiExt;
}

const std::string& LocationBlock::getReturnUrl() const {
	return _returnUrl;
}

size_t LocationBlock::getClientMaxBodySize() const {
	return _clientMaxBodySize;
}

bool LocationBlock::hasClientMaxBodySize() const {
	return _hasClientMaxBodySize;
}

int LocationBlock::getReturnCode() const {
	return _returnCode;
}

bool LocationBlock::getUploadEnable() const {
	return _uploadEnable;
}

bool LocationBlock::getAutoIndex() const {
	return _autoIndex;
}
