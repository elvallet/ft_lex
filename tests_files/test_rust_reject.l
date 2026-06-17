%%

[A-Z]+     { scanner.reject(); }
[a-zA-Z]+  { let _ = scanner.yyout.write_all(b"WORD\n"); return Some(1); }
[a-z]+     { let _ = scanner.yyout.write_all(b"LOWER\n"); return Some(2); }

%%