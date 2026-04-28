%x MULTILINE
%%
"/*"            { yymore(); BEGIN(MULTILINE); }
<MULTILINE>"*/" { printf("COMMENT: %s\n", yytext); BEGIN(INITIAL); }
<MULTILINE>.    { yymore(); }
<MULTILINE>\n   { yymore(); }
[a-z]+          printf("WORD: %s\n", yytext);
[[:space:]]+    /* ignore */
.               printf("OTHER: %s\n", yytext);
%%