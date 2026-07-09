NAME = webserv

CXX = c++

CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -I ./include

SRCS = src/main.cpp \
	   src/ConfigParser/LocationBlock.cpp \
	   src/ConfigParser/ServerBlock.cpp \
	   src/ConfigParser/WebservConfig.cpp \
	   src/File/File.cpp \
	   src/RequestParser/Request.cpp \
	   src/RequestParser/RequestParser.cpp \
	   src/Response/RequestHandler.cpp \
	   src/Response/Response.cpp \
	   src/Server/Client.cpp \
	   src/Server/ListenAndAcceptReqs.cpp \
	   src/Server/Server.cpp

OBJS = $(SRCS:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re

