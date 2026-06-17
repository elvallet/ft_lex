%%

[a-zA-Z]+  { scanner.yymore(); }
[0-9]+     { let text = scanner.yytext.clone(); let _ = scanner.yyout.write_all(format!("TOKEN:{}\n", text).as_bytes()); return Some(1); }

%%