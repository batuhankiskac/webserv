#include "Request.hpp"
#include "Client.hpp"
#include <limits>

int	Request::_errno = 0;

static int	hexDigitValue(char c)
{
	if (c >= '0' && c <= '9')
		return (c - '0');
	if (c >= 'a' && c <= 'f')
		return (c - 'a' + 10);
	if (c >= 'A' && c <= 'F')
		return (c - 'A' + 10);
	return (-1);
}

static bool	parseChunkSize(const std::string& line, std::size_t& chunkSize)
{
	std::size_t	end = line.find(';');
	if (end == std::string::npos)
		end = line.size();
	std::size_t	start = 0;
	while (start < end && (line[start] == ' ' || line[start] == '\t'))
		++start;
	while (end > start && (line[end - 1] == ' ' || line[end - 1] == '\t'))
		--end;
	if (start == end)
		return (false);

	chunkSize = 0;
	for (std::size_t i = start; i < end; ++i) {
		const int	digit = hexDigitValue(line[i]);
		if (digit == -1)
			return (false);
		if (chunkSize > (std::numeric_limits<std::size_t>::max()
			- static_cast<std::size_t>(digit)) / 16)
			return (false);
		chunkSize = chunkSize * 16 + static_cast<std::size_t>(digit);
	}
	return (true);
}

static int	writeChunkData(Client& client, int fileFd, std::size_t amount)
{
	std::size_t	written = 0;
	while (written < amount) {
		const ssize_t	result = write(fileFd,
			client.rawBuffer.data() + written, amount - written);
		if (result <= 0)
			return (-HTTP_INTERNAL_SERVER_ERROR);
		written += static_cast<std::size_t>(result);
	}
	client.bodyReceived += written;
	client.chunkBytesRemaining -= written;
	client.rawBuffer.erase(0, written);
	return (1);
}

static int	unchunkBody(Client& client, int fileFd, const size_t maxBodySize)
{
	while (true) {
		if (client.chunkState == CHUNK_READING_SIZE) {
			const std::size_t	crlf = client.rawBuffer.find("\r\n");
			if (crlf == std::string::npos) {
				if (client.rawBuffer.size() > CHUNK_SEMI_SIZE)
					return (-HTTP_URI_TOO_LONG);
				return (1);
			}
			if (crlf > CHUNK_SEMI_SIZE)
				return (-HTTP_URI_TOO_LONG);

			std::size_t	chunkSize = 0;
			if (!parseChunkSize(client.rawBuffer.substr(0, crlf), chunkSize))
				return (-HTTP_BAD_REQUEST);
			if (chunkSize > std::numeric_limits<std::size_t>::max()
				- client.bodyReceived)
				return (-HTTP_BAD_REQUEST);
			if (maxBodySize != static_cast<size_t>(-1)
				&& (client.bodyReceived > maxBodySize
					|| chunkSize > maxBodySize - client.bodyReceived))
				return (-HTTP_PAYLOAD_TOO_LARGE);

			client.rawBuffer.erase(0, crlf + 2);
			client.chunkBytesRemaining = chunkSize;
			if (chunkSize == 0) {
				client.chunkMetadataBytes = 0;
				client.chunkState = CHUNK_READING_TRAILERS;
			} else {
				client.chunkState = CHUNK_READING_DATA;
			}
		} else if (client.chunkState == CHUNK_READING_DATA) {
			if (client.rawBuffer.empty())
				return (1);
			const std::size_t	amount = client.rawBuffer.size()
				< client.chunkBytesRemaining ? client.rawBuffer.size()
				: client.chunkBytesRemaining;
			const int	result = writeChunkData(client, fileFd, amount);
			if (result < 0)
				return (result);
			if (client.chunkBytesRemaining == 0)
				client.chunkState = CHUNK_EXPECTING_DATA_CRLF;
		} else if (client.chunkState == CHUNK_EXPECTING_DATA_CRLF) {
			if (client.rawBuffer.size() < 2)
				return (1);
			if (client.rawBuffer.compare(0, 2, "\r\n") != 0)
				return (-HTTP_BAD_REQUEST);
			client.rawBuffer.erase(0, 2);
			client.chunkState = CHUNK_READING_SIZE;
		} else if (client.chunkState == CHUNK_READING_TRAILERS) {
			const std::size_t	crlf = client.rawBuffer.find("\r\n");
			if (crlf == std::string::npos) {
				if (client.chunkMetadataBytes > MAX_HEADERS_SIZE
					|| client.rawBuffer.size() > MAX_HEADERS_SIZE
						- client.chunkMetadataBytes)
					return (-HTTP_REQUEST_HEADER_FIELDS_TOO_LARGE);
				return (1);
			}
			if (client.chunkMetadataBytes > MAX_HEADERS_SIZE
				|| crlf + 2 > MAX_HEADERS_SIZE - client.chunkMetadataBytes)
				return (-HTTP_REQUEST_HEADER_FIELDS_TOO_LARGE);
			const std::string	line = client.rawBuffer.substr(0, crlf);
			client.chunkMetadataBytes += crlf + 2;
			client.rawBuffer.erase(0, crlf + 2);
			if (line.empty()) {
				client.chunkState = CHUNK_COMPLETE;
				return (0);
			}
			if (line.find(':') == std::string::npos)
				return (-HTTP_BAD_REQUEST);
		} else if (client.chunkState == CHUNK_COMPLETE) {
			return (0);
		}
	}

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
					else if (client.contentLength == -1
						&& client.request.getHeaders().count("content-length") != 0)
					{
						_errno = -HTTP_BAD_REQUEST;
						return (-1);
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
				const int	result = unchunkBody(client, client.requestBodyFd,
					maxBodySize);
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
