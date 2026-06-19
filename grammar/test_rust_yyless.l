%%

abcdefg     { scanner.yyless(3); println!("YYLESS3({})", scanner.yytext); return Some(1); }
[a-z]+"!"   { scanner.yyless(scanner.yyleng - 1); println!("EMPHATIC({})", scanner.yytext); return Some(1); }
[a-z]+      { println!("WORD({})", scanner.yytext); return Some(1); }
"!"         { println!("BANG"); return Some(1); }
defg        { println!("DEFG({})", scanner.yytext); return Some(1); }

X+"YZ"      { scanner.yyless(1); scanner.reject(); println!("SHOULD_NOT_PRINT({})", scanner.yytext); return Some(1); }
X+          { scanner.yyless(1); println!("X_PLUS_YLESS1({})", scanner.yytext); return Some(1); }
"X"         { println!("X_ALONE"); return Some(1); }
"YZ"        { println!("YZ_ALONE"); return Some(1); }

[ \t\n]+    { }

%%