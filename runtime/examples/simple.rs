use ftlex_runtime::{LexerDef, Scanner};
use std::io::{stdin, stdout};

struct MyLexer;

impl LexerDef for MyLexer {
    fn transition(state: usize, c: u8) -> i32 {
        match (state, c) {
            (0, b'a'..=b'z') | (0, b'A'..=b'Z') => 1,
            (0, b'0'..=b'9')                     => 2,
            (1, b'a'..=b'z') | (1, b'A'..=b'Z') => 1,
            (2, b'0'..=b'9')                     => 2,
            _                                    => -1,
        }
    }

    fn accept_entries(state: usize) -> &'static [(i32, i32)] {
        match state {
            1 => &[(0, -1)],  // rule 0 = WORD,   no trailing context
            2 => &[(1, -1)],  // rule 1 = NUMBER, no trailing context
            _ => &[],
        }
    }

    fn start_state(_condition: usize, _bol: bool) -> usize {
        0
    }

    fn sink() -> usize {
        usize::MAX
    }

    fn yywrap(_scanner: &mut Scanner<Self>) -> bool {
        true
    }

    fn execute_action(scanner: &mut Scanner<Self>, rule_id: i32) -> Option<i32> {
        match rule_id {
            0 => {
                let _ = scanner.yyout.write_all(b"WORD\n");
                Some(1)
            }
            1 => {
                let _ = scanner.yyout.write_all(b"NUMBER\n");
                Some(2)
            }
            _ => None,
        }
    }
}

fn main() {
    let mut scanner = Scanner::<MyLexer>::new(
        Box::new(stdin()),
        Box::new(stdout()),
    );

    loop {
        let tok = scanner.yylex();
        if tok == 0 {
            break;
        }
    }
}