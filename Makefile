NAME		:= ft_lex
MAKEFLAGS	+= --no-print-directory

CXX			:= c++
CXXFLAGS	:= -Wall -Wextra -Werror -std=c++17

# --- Sources ---
SRCDIR		:= src/
FILES		:=	automata/Parser.cpp				\
				automata/Pipeline.cpp			\
				automata/SubsetConstruction.cpp	\
				automata/Thompson.cpp			\
				codegen/Codegen.cpp				\
				lexer_file/LexParser.cpp		\
				lexer_file/LineReader.cpp		\
				lexer_file/ParseError.cpp		\
				main.cpp

SRCS		:= $(addprefix $(SRCDIR), $(FILES))

OBJDIR		:= obj/
OBJS		:= $(patsubst $(SRCDIR)%.cpp,$(OBJDIR)%.o,$(SRCS))

# --- Template ---
TEMPLATE	:= src/template/yylex_template.c
TEMPLATE_H	:= src/codegen/yylex_template.h

# --- Rules ---
all: $(TEMPLATE_H) $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TEMPLATE_H): $(TEMPLATE)
	mkdir -p $(dir $@)
	@if command -v xxd >/dev/null 2>&1; then \
		xxd -i -n yylex_template_c $< > $@; \
	else \
		{ \
			echo "unsigned char yylex_template_c[] = {"; \
			od -An -v -t x1 $< | tr -s ' ' '\n' | grep -E '^[0-9a-f]{2}$$' | sed 's/^/0x/; s/$$/,/' | paste -sd' ' -; \
			echo "};"; \
			printf "unsigned int yylex_template_c_len = %s;\n" "$$(wc -c < $<)"; \
		} > $@; \
	fi

obj/codegen/Codegen.o: src/codegen/Codegen.cpp $(TEMPLATE_H)
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<


$(OBJDIR)%.o: $(SRCDIR)%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TEMPLATE_H)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re