%x STRING
%x COMMENT
%%
"\""            { BEGIN(STRING); }
<STRING>"\""    { BEGIN(INITIAL); printf("STRING END\n"); }
<STRING>.       printf("%s", yytext);
"/*"            { BEGIN(COMMENT); }
<COMMENT>"*/"   { BEGIN(INITIAL); }
<COMMENT>.      /* ignore */
[a-z]+          printf("WORD: %s\n", yytext);
[[:space:]]+    /* ignore */
%%
