#include "ServerBlock.hpp"
#include "WebservConfig.hpp"
#include <cstddef>
#include <fstream>
#include <stdexcept>
#include <cctype>

WebservConfig::WebservConfig(const std::string& filename) {
	_extractFromFile(filename);
	_tokenize();
	_parse();
}

WebservConfig::~WebservConfig() { }

void WebservConfig::_extractFromFile(const std::string& filename) {
	std::ifstream file(filename.c_str());
	if (!file.is_open()) {
		throw std::runtime_error("Error opening file: " + filename);
	}

	std::string line;
	while (std::getline(file, line)) {
		std::string::size_type hashPos = line.find('#');
		if (hashPos != std::string::npos) {
			line = line.substr(0, hashPos);
		}

		bool isEmpty = true;
		for (std::string::size_type i = 0; i < line.size(); i++) {
			if (!std::isspace(line[i])) {
				isEmpty = false;
				break;
			}
		}

		if (!isEmpty) {
			_raw += line + "\n";
		}
	}
	file.close();
}

void WebservConfig::_tokenize() {
	std::string current = "";

	for (size_t i = 0; i < _raw.length(); ++i) {
		if (std::isspace(_raw[i])) {
			if (current.empty()) {
				continue;
			}
			_tokens.push_back(current);
			current = "";
		} else if (_raw[i] == '{' || _raw[i] == '}' || _raw[i] == ';') {
			if (!current.empty()) {
				_tokens.push_back(current);
				current = "";
			}
			_tokens.push_back(std::string(1, _raw[i]));
		} else {
			current += _raw[i];
		}
	}

	if (!current.empty()) {
		_tokens.push_back(current);
	}
}

void WebservConfig::_parse() {
	size_t i = 0;

	while (i < _tokens.size()) {
		if (_tokens[i] == "server") {

			if (i + 1 >= _tokens.size() || _tokens[i + 1] != "{") {
				throw std::runtime_error("Invalid server block");
			}

			ServerBlock block;
			block.parseServerBlock(_tokens, i);
			_servers.push_back(block);
		} else {
			throw std::runtime_error("Invalid token: " + _tokens[i]);
		}
	}

	if (_servers.empty()) {
		throw std::runtime_error("No server blocks found");
	}
}

const std::vector<ServerBlock>& WebservConfig::getServers() const {
	return _servers;
}
