%{
#include <stdio.h>
#include <stdlib.h>

int word_count = 0;
%}
DIGIT   [0-9]
ALPHA   [a-zA-Z]
ALNUM   {DIGIT}|{ALPHA}
%%
{ALNUM}+    {
    word_count++;
    printf("TOKEN: %s\n", yytext);
}
[ \t\n]+    { /* ignore whitespace */ }
%%
int main(void)
{
    yylex();
    printf("Total tokens: %d\n", word_count);
    return 0;
}
