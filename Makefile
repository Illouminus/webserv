NAME = webserv

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -g -std=c++98

OBJDIR = obj
INCLUDES = includes

SRCS = main.cpp \
       src/parsing/Parser.cpp \
       src/parsing/LocationConfig.cpp \
       src/parsing/ServerConfig.cpp \
		 src/WebServ.cpp \
		 src/parsing/HttpParser.cpp \
		 src/HttpResponse.cpp \
		 src/Responder.cpp

OBJS = $(SRCS:%.cpp=$(OBJDIR)/%.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -I $(INCLUDES) -o $(NAME) $(OBJS)

$(OBJDIR)/%.o: %.cpp
	@mkdir -p $(dir $@) 
	$(CXX) $(CXXFLAGS) -I $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(OBJDIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
