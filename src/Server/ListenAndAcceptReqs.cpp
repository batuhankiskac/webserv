#include "include/ListenAndAcceptReqs.hpp"

ListenAndAcceptReqs::ListenAndAcceptReqs(const int socketFd)
{
	if (listen(socketFd, SOMAXCONN) < 0)
	{
		throw ListenAndAcceptReqs::ListenOrAcceptionError();
	}

}

ListenAndAcceptReqs::~ListenAndAcceptReqs()
{

}

const char* ListenAndAcceptReqs::ListenOrAcceptionError::what() const throw()
{
	return (std::strerror(errno));
}

// HENÜZ TAMAMLANMADI.
