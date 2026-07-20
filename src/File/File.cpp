#include "File.hpp"
#include <cstdio>

File::File() : _errno(0) {}

File::~File() {}

void	File::setPath(const std::string& path)
{
	this->path = path;	
}

std::string	File::getPath() const
{
	return (this->path);
}

int	File::getNewFileFd(int clientFd)
{
	std::string filename = path;
	
	std::stringstream ss;
	ss << clientFd;
	filename += ss.str();

	int fileFd = open(filename.c_str(), 
						O_RDWR | O_CREAT | O_TRUNC, 
						0644);
	
	if (fileFd != -1)
	{
		fcntl(fileFd, F_SETFD, FD_CLOEXEC);
		fdMap[clientFd] = fileFd;
		return (fileFd);
	}

	this->_errno = errno;
	return (-1);
}

int	File::openFile(int clientFd)
{
	std::string filename = path;
	
	std::stringstream ss;
	ss << clientFd;
	filename += ss.str();

	int fileFd = open(filename.c_str(), 
						O_RDONLY, 
						0444);
	
	if (fileFd != -1)
	{
		fdMap[clientFd] = fileFd;
		return (fileFd);
	}

	this->_errno = errno;
	return (-1);
}

void	File::closeFile(int clientFd)
{
	std::map<int, int>::iterator it = fdMap.find(clientFd);
	if (it != fdMap.end())
	{
		if (it->second != -1)
			close(it->second);
		std::string filename = path;
		std::stringstream ss;
		ss << clientFd;
		filename += ss.str();
		std::remove(filename.c_str());
		fdMap.erase(it);
	}
}

int	File::getErr() const
{
	return (this->_errno);
}
