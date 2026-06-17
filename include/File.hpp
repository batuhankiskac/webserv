#ifndef FILE_HPP
#define FILE_HPP

#include <string>
#include <map>
#include <unistd.h>
#include <fcntl.h>
#include <sstream>
#include <cerrno>

class File
{
	private:
		std::string			path;
		std::map<int, int>	fdMap;
		int	_errno;

	public:
		File();
		~File();
		void	setPath(const std::string& path);
		std::string	getPath() const;
		
		int		getNewFileFd(int clientFd);
		int		openFile(int clientFd);
		void	closeFile(int clientFd);

		int	getErr() const;
};

#endif // FILE_HPP