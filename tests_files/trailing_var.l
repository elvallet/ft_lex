%%

"foo"/ba*r {
    printf("MATCH foo, yytext=[%s], yyleng=%lu\n", yytext, (unsigned long)yyleng);
}

"x"/y+z {
    printf("MATCH x, yytext=[%s], yyleng=%lu\n", yytext, (unsigned long)yyleng);
}

. {
    printf("DEFAULT char=[%s]\n", yytext);
}

\n {
    /* swallow newlines quietly */
}

%%

int main(void)
{
    yylex();
    return 0;
}