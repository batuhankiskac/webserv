#include "ServerBlock.hpp"

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

void ServerBlock::_parseListen(const std::vector<std::string>& _tokens, size_t& i) {

}

void ServerBlock::_parseServerName(const std::vector<std::string>& _tokens, size_t& i) {

}

void ServerBlock::_parseErrorPage(const std::vector<std::string>& _tokens, size_t& i) {

}

void ServerBlock::_parseClientMaxBodySize(const std::vector<std::string>& _tokens, size_t& i) {

}

void ServerBlock::_parseLocation(const std::vector<std::string>& _tokens, size_t& i) {

}
