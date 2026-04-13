%%
"write"/"r"         printf("WRITE_BEFORE_R: %s\n", yytext);
"read"/"ing"        printf("READ_BEFORE_ING: %s\n", yytext);
[a-zA-Z]+           printf("WORD: %s\n", yytext);
[[:space:]]+        /* ignore */
.                   printf("OTHER: %s\n", yytext);
%%