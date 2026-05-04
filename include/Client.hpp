#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>

struct Client
{
	int			clientFd;
	std::string	request;
	std::string	response;
};

#endif //CLIENT_HPP