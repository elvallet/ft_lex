%x COMMENT
%%
"/*"            { BEGIN(COMMENT); }
<COMMENT>"*/"   { BEGIN(INITIAL); }
<COMMENT>.      /* ignore */
<COMMENT>\n     /* ignore */
[a-zA-Z_]+     printf("IDENT: %s\n", yytext);
[[:space:]]+   /* ignore */
.              printf("CHAR: %s\n", yytext);
%%
