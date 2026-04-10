%s COUNTING
%%
"start"         { BEGIN(COUNTING); printf("START\n"); }
"stop"          { BEGIN(INITIAL); printf("STOP\n"); }
[0-9]+          printf("NUM: %s\n", yytext);
[[:space:]]+    /* ignore */
.               printf("OTHER: %s\n", yytext);
%%
