%%
^"#"[^\n]*      printf("COMMENT: %s\n", yytext);
^[a-z]+         printf("BOL_WORD: %s\n", yytext);
[a-z]+          printf("WORD: %s\n", yytext);
[[:space:]]+    /* ignore */
.               printf("OTHER: %s\n", yytext);
%%