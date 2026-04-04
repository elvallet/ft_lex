%%
"int"       printf("KEYWORD: int\n");
"return"    printf("KEYWORD: return\n");
"+"         printf("PLUS\n");
"++"        printf("INCREMENT\n");
[a-z]+      printf("IDENT: %s\n", yytext);
\n          /* ignore */
" "         /* ignore */
.           printf("UNKNOWN: %s\n", yytext);
%%
