#ifndef REQUEST_READER_HPP
#define REQUEST_READER_HPP

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
#define MAX_BODY_SIZE		1048576

class RequestReader
{
	private:
		static int	_errno;

	public:
		static int	readFd(struct Client &client, File& file);
		static int	getErrno();
};

#endif // REQUEST_READER_HPP