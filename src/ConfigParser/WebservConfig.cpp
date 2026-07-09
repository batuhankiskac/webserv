#include "ServerBlock.hpp"
#include "WebservConfig.hpp"
#include <cstddef>
#include <fstream>
#include <stdexcept>
#include <cctype>

WebservConfig::WebservConfig(const std::string& filename) {
	std::string	raw;
	std::vector<std::string>	tokens;
	_extractFromFile(filename, raw);
	_tokenize(raw, tokens);
	_parse(tokens);
}

void WebservConfig::_extractFromFile(const std::string& filename, std::string& raw) {
	std::ifstream	file(filename.c_str());
	if (!file.is_open()) {
		throw std::runtime_error("Error opening file: " + filename);
	}

	std::string	line;
	while (std::getline(file, line)) {
		std::string::size_type	hashPos = line.find('#');
		if (hashPos != std::string::npos) {
			line = line.substr(0, hashPos);
		}

		if (line.find_first_not_of(" \t\r\n") != std::string::npos) {
			raw += line + "\n";
		}
	}
}

void WebservConfig::_tokenize(const std::string& raw, std::vector<std::string>& tokens) {
	std::string	current;

	for (size_t i = 0; i < raw.length(); ++i) {
		if (std::isspace(raw[i])) {
			if (current.empty()) {
				continue;
			}
			tokens.push_back(current);
			current.clear();
		} else if (raw[i] == '{' || raw[i] == '}' || raw[i] == ';') {
			if (!current.empty()) {
				tokens.push_back(current);
				current.clear();
			}
			tokens.push_back(std::string(1, raw[i]));
		} else {
			current += raw[i];
		}
	}

	if (!current.empty()) {
		tokens.push_back(current);
	}
}

void WebservConfig::_parse(const std::vector<std::string>& tokens) {
	size_t	i = 0;

	while (i < tokens.size()) {
		if (tokens[i] == "server") {

			if (i + 1 >= tokens.size() || tokens[i + 1] != "{") {
				throw std::runtime_error("Invalid server block");
			}

			ServerBlock	block;
			block.parseServerBlock(tokens, i);
			_servers.push_back(block);
		} else {
			throw std::runtime_error("Invalid token: " + tokens[i]);
		}
	}

	if (_servers.empty()) {
		throw std::runtime_error("No server blocks found");
	}
}

const std::vector<ServerBlock>& WebservConfig::getServers() const {
	return _servers;
}
