%%
[0-9]+  {
    int n = atoi(yytext);
    printf("NUMBER: %d\n", n);
}
[a-z]+  { printf("WORD\n"); }
%%
