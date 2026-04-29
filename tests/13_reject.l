%%
"fish"          { printf("FISH\n"); REJECT; }
"fish"+"ing"    { printf("FISHING\n"); }
[a-z]+          { printf("WORD: %s\n", yytext); }
[[:space:]]+    /* ignore */
.               { printf("OTHER: %s\n", yytext); }
%%