NAME    = ft_irc

CXX			= c++
CXXFLAGS	= -Wall -Wextra -Werror -std=c++98 -I headers

SRCS    = srcs/main.cpp srcs/client.cpp srcs/parsmessage.cpp srcs/answers.cpp
OBJS    = $(SRCS:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJS)
		$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

clean:
		rm -f $(OBJS)

fclean: clean
		rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re