#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ECHO do { fwrite(yybuffer + yybuf_pos - 1, 1, 1, stdout); } while(0)
#define YYBUF_INIT_SIZE 256

static char		*yybuffer	= NULL;
static size_t	yybuf_size	= 0;
static size_t	yybuf_capa	= 0;
static size_t	yybuf_pos	= 0;

char	*yytext	= NULL;
size_t	yyleng	= 0;
FILE	*yyin	= NULL;

int yyread(void) {
	if (yybuf_pos < yybuf_size)
		return (unsigned char)yybuffer[yybuf_pos++];
	
	int c	= fgetc(yyin);
	if (c == EOF)
		return EOF;
	
	if (yybuf_size + 1 >= yybuf_capa) {
		size_t new_capa = yybuf_capa == 0 ? YYBUF_INIT_SIZE : yybuf_capa * 2;
		char *tmp = realloc(yybuffer, new_capa);
		if (!tmp)
			return EOF;	
		yybuffer	= tmp;
		yybuf_capa	= new_capa;
	}

	yybuffer[yybuf_size++]	= (char)c;
	yybuffer[yybuf_size]	= '\0';
	yybuf_pos++;
	return (unsigned char)c;
}

int yylex(void) {
	free(yytext);
	yytext = NULL;

	@@VERBATIM_RULES@@

	static size_t match_start = 0;

	while (1) {
		int		state			= @@INITIAL_STATE@@;
		int 	last_match		= -1;
		size_t	last_match_pos	= match_start;
	
		yybuf_pos	= match_start;

		while (1) {
			int c = yyread();

			if (c == EOF) {
				if (last_match != -1)
					break;
				if (yywrap() == 1)
					return 0;
				continue;
			}

			state = yy_table[state][(unsigned char)c];

			if (state == -1)
				break;
			
			if (yy_accept[state] != -1) {
				last_match		= yy_accept[state];
				last_match_pos	= yybuf_pos;
			}
		}

		if (last_match == -1) {
			ECHO;
			match_start++;
			continue;
		}

		yyleng		= last_match_pos - match_start;
		yytext		= strndup(yybuffer + match_start, yyleng);
		yybuf_pos	= last_match_pos;
		match_start	= last_match_pos;

		swicth (last_match) {
			@@RULES@@
		}
	}
}