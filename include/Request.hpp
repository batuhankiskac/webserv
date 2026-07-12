#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <sys/socket.h>
#include <errno.h>
#include <cctype>
#include <string>
#include <sstream>
#include <algorithm>
#include <sstream>
#include <fcntl.h>

#include "File.hpp"
#include "HttpConstants.hpp"

#define CHUNK_SEMI_SIZE		256
#define MAX_REQUEST_LINE	8192
#define MAX_HEADERS_SIZE	32768

class Request
{
	private:
		static int	_errno;

	public:
		static int	readFd(struct Client &client, File& file, size_t maxBodySize);
		static int	getErrno();
};

#endif // REQUEST_HPP
