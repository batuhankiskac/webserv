#include "Config.hpp"
#include <cstddef>
#include <fstream>
#include <stdexcept>
#include <cctype>

Config::Config(const std::string& filename) {
	_extractFromFile(filename);
	_tokenize();
	_parse();
}

Config::~Config() { }

void Config::_extractFromFile(const std::string& filename) {
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

void Config::_tokenize() {
	std::string current = "";

	for (size_t i = 0; i < _raw.length(); ++i) {
		if (std::isspace(_raw[i])) {
			if (current.empty()) {
				continue;
			}
			_tokens.push_back(current);
			current = "";
		} else if (_raw[i] == '{' || _raw[i] == '}') {
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

void Config::_parse() {
	// TODO: Implement parsing
}

const std::vector<ServerConfig>& Config::getServers() const {
	return _servers;
}

