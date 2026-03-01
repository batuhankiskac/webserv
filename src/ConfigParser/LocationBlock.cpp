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

void LocationBlock::_parsePath(const std::vector<std::string>& _tokens, size_t& i) {
	i++;

	if (i >= _tokens.size() || _tokens[i] == ";") {
		throw std::runtime_error("Invalid path directive");
	}
	_path = _tokens[i++];
	if (i >= _tokens.size() || _tokens[i] != ";") {
		throw std::runtime_error("Invalid path directive");
	}
	i++;
}

void LocationBlock::_parseRoot(const std::vector<std::string>& _tokens, size_t& i) {
	i++;

	if (i >= _tokens.size() || _tokens[i] == ";") {
		throw std::runtime_error("Invalid root directive");
	}
	_root = _tokens[i++];
	if (i >= _tokens.size() || _tokens[i] != ";") {
		throw std::runtime_error("Invalid root directive");
	}
	i++;
}

void LocationBlock::_parseUploadStore(const std::vector<std::string>& _tokens, size_t& i) {
	i++;

	if (i >= _tokens.size() || _tokens[i] == ";") {
		throw std::runtime_error("Invalid upload_store directive");
	}
	_uploadStore = _tokens[i++];
	if (i >= _tokens.size() || _tokens[i] != ";") {
		throw std::runtime_error("Invalid upload_store directive");
	}
	i++;
}

void LocationBlock::_parseUploadEnable(const std::vector<std::string>& _tokens, size_t& i) {
	i++;

	if (i >= _tokens.size() || _tokens[i] == ";") {
		throw std::runtime_error("Invalid upload_enable directive");
	}

	std::string value = _tokens[i++];
	if (value != "on" && value != "off") {
		throw std::runtime_error("Invalid upload_enable directive");
	}
	_uploadEnable = (value == "on");
	if (i >= _tokens.size() || _tokens[i] != ";") {
		throw std::runtime_error("Invalid upload_enable directive");
	}
	i++;
}

void LocationBlock::_parseCgiPath(const std::vector<std::string>& _tokens, size_t& i) {
	i++;

	if (i >= _tokens.size() || _tokens[i] == ";") {
		throw std::runtime_error("Invalid cgi_path directive");
	}
	_cgiPath = _tokens[i++];
	if (i >= _tokens.size() || _tokens[i] != ";") {
		throw std::runtime_error("Invalid cgi_path directive");
	}
	i++;
}

void LocationBlock::_parseCgiExt(const std::vector<std::string>& _tokens, size_t& i) {
	i++;

	if (i >= _tokens.size() || _tokens[i] == ";") {
		throw std::runtime_error("Invalid cgi_ext directive");
	}
	_cgiExt = _tokens[i++];
	if (i >= _tokens.size() || _tokens[i] != ";") {
		throw std::runtime_error("Invalid cgi_ext directive");
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

	if (i >= _tokens.size() || _tokens[i] == ";") {
		throw std::runtime_error("Invalid return directive");
	}
	_returnUrl = _tokens[i++];
	if (i >= _tokens.size() || _tokens[i] != ";") {
		throw std::runtime_error("Invalid return directive");
	}
	i++;
}

void LocationBlock::_parseAutoIndex(const std::vector<std::string>& _tokens, size_t& i) {
	i++;

	if (i >= _tokens.size() || _tokens[i] == ";") {
		throw std::runtime_error("Invalid autoindex directive");
	}

	std::string value = _tokens[i++];
	if (value != "on" && value != "off") {
		throw std::runtime_error("Invalid autoindex directive");
	}
	_autoIndex = (value == "on");
	if (i >= _tokens.size() || _tokens[i] != ";") {
		throw std::runtime_error("Invalid autoindex directive");
	}
	i++;
}
