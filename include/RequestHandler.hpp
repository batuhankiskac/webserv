#ifndef REQUEST_HANDLER_HPP
#define REQUEST_HANDLER_HPP

#include "Client.hpp"
#include "WebservConfig.hpp"

class RequestHandler {
private:
	RequestHandler();
	static std::string _extractHost(const Client& client);
	static const ServerBlock& _selectServerBlock(const WebservConfig& config, int port, const std::string& host);
	static const LocationBlock* _selectLocationBlock(const ServerBlock& server, const std::string& path);
	static void _serveError(Client& client, int code, const ServerBlock& server);
	static void _handleGet(Client& client, const ServerBlock& server, const LocationBlock& loc, const std::string& reqPath);
	static void _handlePost(Client& client, const ServerBlock& server, const LocationBlock& loc, const std::string& reqPath);
	static void _handleDelete(Client& client, const ServerBlock& server, const LocationBlock& loc, const std::string& reqPath);
	static std::string _generateAutoindex(const std::string& filePath, const std::string& reqPath);

public:
	static void handle(Client& client, const WebservConfig& config, int port);
};

#endif
