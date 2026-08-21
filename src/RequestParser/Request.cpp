#include "Request.hpp"
#include "Client.hpp"

int	Request::_errno = 0;

static int	unchunkBody(std::string& rawBuffer, std::size_t& bodyReceived, int fileFd, std::string& body, const size_t maxBodySize)
{
	while (!rawBuffer.empty())
	{
		std::size_t	crlf_pos = rawBuffer.find("\r\n");
		if (crlf_pos == std::string::npos)
			return (1);

		std::string	size_line = rawBuffer.substr(0, crlf_pos);

		std::size_t	semi_pos = size_line.find(';');
		std::string	size_str;

		if (semi_pos != std::string::npos) 
		{
			if (size_line.length() > CHUNK_SEMI_SIZE) 
				return (-HTTP_URI_TOO_LONG);

			size_str = size_line.substr(0, semi_pos);
		} 
		else 
		{
			size_str = size_line;
		}
		if (size_str.empty())
			return (-HTTP_BAD_REQUEST);
		
		char*	end_ptr;
		long	chunk_size = std::strtol(size_str.c_str(), &end_ptr, 16);

		if ((*end_ptr != '\0' && *end_ptr != ' ' && *end_ptr != '\t') || 
				(chunk_size < 0))
		{
			return (-HTTP_BAD_REQUEST);
		}

		if (chunk_size == 0)
		{
			if (rawBuffer.size() >= crlf_pos + 4 && rawBuffer.compare(crlf_pos + 2, 2, "\r\n") == 0) {
				rawBuffer.erase(0, crlf_pos + 4);
				return (0);
			}
			std::size_t trailerEnd = rawBuffer.find("\r\n\r\n", crlf_pos + 2);
			if (trailerEnd == std::string::npos)
				return (1);
			std::string trailers = rawBuffer.substr(crlf_pos + 2, trailerEnd - crlf_pos - 2);
			if (!trailers.empty()) {
				std::size_t start = 0;
				while (start < trailers.size()) {
					std::size_t end = trailers.find("\r\n", start);
					if (end == std::string::npos) end = trailers.size();
					if (trailers.find(':', start) == std::string::npos || trailers.find(':', start) > end)
						return (-HTTP_BAD_REQUEST);
					start = end + 2;
				}
			}
			rawBuffer.erase(0, trailerEnd + 4);
			return (0);
		}

		std::size_t	total_chunk_bytes = crlf_pos + 2 + chunk_size + 2;
		
		if (rawBuffer.length() < total_chunk_bytes)
			return (1);

		if (rawBuffer.compare(crlf_pos + 2 + chunk_size, 2, "\r\n") != 0)
		{
			return (-HTTP_BAD_REQUEST);
		}

		if (bodyReceived + static_cast<std::size_t>(chunk_size) > maxBodySize)
		{
			return (-HTTP_PAYLOAD_TOO_LARGE);
		}

		ssize_t written = 0;
		while (written < static_cast<ssize_t>(chunk_size))
		{
			const	ssize_t remaining = chunk_size - written;
			const	char* current_ptr = rawBuffer.c_str() + crlf_pos + 2 + written;
			const	ssize_t result = write(fileFd, current_ptr, remaining);

			if (result == -1)
			{
				return (-HTTP_INTERNAL_SERVER_ERROR);
			}

			bodyReceived += result;
			body.append(current_ptr, static_cast<std::size_t>(result));
			written += result;
		}

		rawBuffer.erase(0, total_chunk_bytes);
	}

	return (1); 
}

int Request::readFd(struct Client &client, File& file, size_t maxBodySize)
{
	_errno = 0;
	if (client.clientFd == -1)
	{
		_errno = -1;
		return (-1);
	}

	{
		char	buffer[MAX_REQUEST_LINE];
		int	result = recv(client.clientFd, buffer, sizeof(buffer), 0);
		if (result == -1)
		{
			_errno = -HTTP_INTERNAL_SERVER_ERROR;
			return (-1);
		}
		else if (result == 0)
		{
			_errno = -1;
			return (0);
		}
		else
		{
			client.rawBuffer.append(buffer, result);

			if (client.state == READING_HEADERS)
			{
				std::size_t firstLine = client.rawBuffer.find("\r\n");
				if (firstLine != std::string::npos && firstLine > MAX_REQUEST_LINE)
				{
					_errno = -HTTP_URI_TOO_LONG;
					return (-1);
				}
				if (client.rawBuffer.size()> MAX_HEADERS_SIZE)
				{
					_errno = -HTTP_REQUEST_HEADER_FIELDS_TOO_LARGE;
					return (-1);
				}

				std::string&	req = client.rawBuffer;

				std::size_t	pos = req.find("\r\n\r\n");
				if (pos != std::string::npos)
				{
					client.requestHeader = req.substr(0, pos);
					req.erase(0, pos + 4);

					client.request.parse(client.requestHeader + "\r\n\r\n");
					if (client.request.hasError())
					{
						_errno = -client.request.getErrorCode();
						return (-1);
					}
					client.contentLength = client.request.getContentLength();
					if (client.request.isChunked())
					{
						client.state = READING_CHUNKS;
					}
					else if (client.contentLength > 0)
					{
						if (client.contentLength > static_cast<long long>(maxBodySize))
						{
							_errno = -HTTP_PAYLOAD_TOO_LARGE;
							return (-1);
						}
						client.state = READING_BODY;
					}
					else if (client.contentLength == 0)
					{
						client.state = REQUEST_COMPLETE;
					}
					else
					{
						client.state = REQUEST_COMPLETE;
					}
				}
			}
			if (client.state == READING_BODY)
			{
				const std::size_t	val = client.rawBuffer.size() > (client.contentLength - client.bodyReceived) 
									? (client.contentLength - client.bodyReceived) : client.rawBuffer.size();

				if (client.bodyReceived + val > static_cast<std::size_t>(client.contentLength))
				{
					_errno = -HTTP_PAYLOAD_TOO_LARGE;
					return (-1);
				}
					if (client.contentLength <= MAX_IN_MEMORY_BODY_SIZE)
				{
					client.bodyReceived += val;
					client.requestBody.append(client.rawBuffer.substr(0, val));
					client.rawBuffer.erase(0, val);
					if (client.bodyReceived == static_cast<std::size_t>(client.contentLength))
						client.state = REQUEST_COMPLETE;
				}
				else
				{
					if (client.requestBodyFd == -1)
					{
						client.requestBodyFd = file.getNewFileFd(client.clientFd);
						if (client.requestBodyFd == -1)
						{
							_errno = -2;
							return (-1);
						}
						std::stringstream bodyPath;
						bodyPath << file.getPath() << client.clientFd;
						client.requestBodyPath = bodyPath.str();
					}

					std::size_t total_written = 0;
					while (total_written < val)
					{
						const ssize_t written = write(client.requestBodyFd, 
													client.rawBuffer.c_str() + total_written, 
													val - total_written);

						if (written <= 0)
						{
							file.closeFile(client.clientFd);
							client.requestBodyFd = -1;
							_errno = -HTTP_INTERNAL_SERVER_ERROR;
							return (-1); 
						}
						total_written += written;
					}
					client.bodyReceived += total_written;
					client.requestBody.append(client.rawBuffer.substr(0, total_written));
					client.rawBuffer.erase(0, total_written);
					if (client.bodyReceived == static_cast<std::size_t>(client.contentLength))
					{
						client.state = REQUEST_COMPLETE;
					}
				}
			}
			if (client.state == READING_CHUNKS)
			{
				if (client.requestBodyFd == -1)
				{
					client.requestBodyFd = file.getNewFileFd(client.clientFd);
					if (client.requestBodyFd == -1)
					{
						_errno = -2;
						return (-1);
					}
					std::stringstream bodyPath;
					bodyPath << file.getPath() << client.clientFd;
					client.requestBodyPath = bodyPath.str();
				}
			const int	result = unchunkBody(client.rawBuffer, client.bodyReceived, client.requestBodyFd, client.requestBody, maxBodySize);
			if (!result)
			{
				client.contentLength = client.bodyReceived;
				client.state = REQUEST_COMPLETE;
			}
				else if (result < 0)
				{
					if (result <= -HTTP_BAD_REQUEST)
						_errno = result;
					else
						_errno = -result;
					file.closeFile(client.clientFd);
					client.requestBodyFd = -1;
					return (-1);
				}
			}
			if (client.state == REQUEST_COMPLETE)
			{
				_errno = 0;
				return (0);
			}
		}
	}
	return (1);
}

int	Request::getErrno()
{
	return (_errno);
}
