SRCDIR		= src/
OBJDIR		= obj/
MAKEFLAGS	+= --no-print-directory

SRCS	= yywrap.c main.c
OBJS	= $(patsubst %.c,$(OBJDIR)%.o,$(SRCS))

NAME	= libl.a
LIBC	= ar rc
LIBR	= ranlib
CC		= gcc
RM		= rm -f
CFLAGS	= -Wall -Wextra -Werror

$(OBJDIR)%.o: $(SRCDIR)%.c
		@mkdir -p $(dir $@)
		${CC} ${CFLAGS} -c $< -o $@ -I .

${NAME}: ${OBJS}
		${LIBC} ${NAME} ${OBJS}
		${LIBR} ${NAME}

all : ${NAME}

clean :
		${RM} ${OBJS} 

fclean : clean
		${RM} ${NAME}

re : fclean all clean