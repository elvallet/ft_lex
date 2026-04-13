%%
[0-9]{3}                printf("EXACT3: %s\n", yytext);
[a-z]{2,4}              printf("BETWEEN2AND4: %s\n", yytext);
[A-Z]{2,}               printf("ATLEAST2: %s\n", yytext);
[[:space:]]+            /* ignore */
.                       printf("OTHER: %s\n", yytext);
%%