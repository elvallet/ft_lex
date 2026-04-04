%%
[^a-zA-Z0-9[:space:]]+  printf("PUNCT: %s\n", yytext);
[[:alpha:]]+            printf("WORD: %s\n", yytext);
[[:digit:]]+            printf("NUM: %s\n", yytext);
[[:space:]]+            /* skip */
