%%
"+"     |
"-"     |
"*"     |
"/"     { printf("OPERATOR: %s\n", yytext); }
[0-9]+  { printf("NUMBER\n"); }
%%
