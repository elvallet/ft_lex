%%
[0-9]+"."[0-9]+     printf("FLOAT: %s\n", yytext);
[0-9]+              printf("INT: %s\n", yytext);
"+"                 printf("PLUS\n");
"-"                 printf("MINUS\n");
"*"                 printf("STAR\n");
"/"                 printf("SLASH\n");
"("                 printf("LPAREN\n");
")"                 printf("RPAREN\n");
[[:blank:]]+        /* ignore */
\n                  printf("NEWLINE\n");
.                   printf("INVALID: %s\n", yytext);
%%
