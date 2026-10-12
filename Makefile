NAME		= ircserv
BOT_NAME	= ircbot

CXX			= c++
CXXFLAGS	= -Wall -Wextra -Werror -std=c++98 -I headers

SRCS		= src/main.cpp src/Server.cpp src/client.cpp src/parsmessage.cpp src/answers.cpp \
				src/utils.cpp src/Channel.cpp src/userCommands.cpp src/operatorCommands.cpp
BOT_SRCS	= src/main_bot.cpp src/Bot.cpp

OBJS		= $(SRCS:.cpp=.o)
BOT_OBJS	= $(BOT_SRCS:.cpp=.o)

RM			= rm -f

all: $(NAME) $(BOT_NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

$(BOT_NAME): $(BOT_OBJS)
	$(CXX) $(CXXFLAGS) $(BOT_OBJS) -o $(BOT_NAME)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJS) $(BOT_OBJS)

fclean: clean
	$(RM) $(NAME) $(BOT_NAME)

re: fclean all

.PHONY: all clean fclean re