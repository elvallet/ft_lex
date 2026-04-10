%%
[0-9]+          printf("INT: %s\n", yytext);
[a-zA-Z_][a-zA-Z0-9_]*  printf("IDENT: %s\n", yytext);
[-+*/]         printf("OP: %s\n", yytext);
[[:space:]]+    /* ignore whitespace */
.               printf("UNKNOWN: %s\n", yytext);
%%
