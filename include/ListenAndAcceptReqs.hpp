#ifndef LISTEN_AND_ACCEPT_REQS
#define LISTEN_AND_ACCEPT_REQS

#include <exception>
#include <cstring>
#include <cerrno>
#include <sys/socket.h>

class ListenAndAcceptReqs
{
	public:
		ListenAndAcceptReqs(const int socketFd);
		~ListenAndAcceptReqs();

	private:
		class ListenOrAcceptionError : public std::exception
		{
			virtual const char* what() const throw();
		};
};

#endif //LISTEN_AND_ACCEPT_REQS