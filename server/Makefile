CXX     = g++
CXXFLAGS = -Wall -Wextra -Werror -std=c++17 -Iinclude

NAME    = zappy_server

SRCS    = src/main.cpp \
          src/Args.cpp \
          src/Server.cpp \
          src/Client.cpp \
          src/World.cpp \
          src/Map.cpp \
          src/Player.cpp

OBJS    = $(SRCS:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJS)

clean:
	rm -f $(OBJS)
	rm -f zappy_server

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
