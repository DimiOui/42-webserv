PROGRAM = webserv
HDRS = includes
CXX = g++

TMPDIR = .tmp

CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -g

ROUTER = $(addprefix Router/, Lexer.cpp Parser.cpp Block.cpp)
SRCS = $(addprefix srcs/, main.cpp $(ROUTER))
OBJS = $(addprefix $(TMPDIR)/, $(SRCS:%.cpp=%.o))

RM = rm -rf

all: $(PROGRAM)

$(PROGRAM): $(OBJS) | Makefile
	$(CXX) $(CXXFLAGS) $^ -o $@ -g

$(OBJS): $(TMPDIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -I $(HDRS) -MMD -MF $(TMPDIR)/$*.d -c $< -o $@

clean:
	$(RM) $(TMPDIR)

fclean: clean
	$(RM) $(PROGRAM)

re: fclean all

.PHONY: all clean fclean re

-include $(OBJS:%.o=%.d)
