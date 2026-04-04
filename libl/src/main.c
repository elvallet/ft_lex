#include <stdio.h>

int yylex(void);
extern FILE	*yyin;
extern FILE	*yyout;

__attribute__((weak)) int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	if (argc > 1) {
		yyin = fopen(argv[1], "r");
		if (!yyin) {
			fprintf(stderr, "ft_lex: cannot open %s\n", argv[1]);
			return 1;
		}
	} else {
		yyin = stdin;
	}
	yyout = stdout;
	yylex();
}