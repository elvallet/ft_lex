%%

[a-zA-Z]+[0-9]  { scanner.yyless(scanner.yyleng - 1); let _ = scanner.yyout.write_all(b"WORD\n"); return Some(1); }
[0-9]+          { let _ = scanner.yyout.write_all(b"NUMBER\n"); return Some(2); }

%%