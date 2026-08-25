#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <sys/socket.h>
#include <cerrno>
#include <cctype>
#include <string>
#include <sstream>
#include <algorithm>
#include <sstream>
#include <fcntl.h>

#include "File.hpp"
#include "HttpConstants.hpp"

#define MAX_CHUNK_LINE_SIZE	256
#define MAX_REQUEST_LINE	8192 // Maximum request-line size, excluding CRLF
#define MAX_HEADERS_SIZE	32768
#define MAX_IN_MEMORY_BODY_SIZE	8192

class Request
{
	private:
		static int	_errno;

	public:
		static int	readFd(struct Client &client, File& file, size_t maxBodySize);
		static int	getErrno();
};

#endif // REQUEST_HPP
