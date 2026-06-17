use std::marker::PhantomData;
use std::io::{Read, Write};

pub trait LexerDef {
    fn ftlex_main<D: LexerDef>() {
        let mut scanner = Scanner::<D>::new(
            Box::new(std::io::stdin()),
            Box::new(std::io::stdout()),
        );
        loop {
            if scanner.yylex() == 0 { break; }
        }
    }

    fn transition(state: usize, c: u8) -> i32;
    fn accept_entries(state: usize) -> &'static [(i32, i32)];
    fn start_state(condition: usize, bol: bool) -> usize;
    fn sink() -> usize;

    fn yywrap(_scanner: &mut Scanner<Self>) -> bool
    where Self: Sized
    {
        true
    }

    fn execute_action(scanner: &mut Scanner<Self>, rule_id: i32) -> Option<i32>
    where Self: Sized;
}

pub fn ftlex_main<D: LexerDef>() {
    D::ftlex_main::<D>();
}

struct Candidate {
    rule_id:        i32,
    match_end:      usize,
    trailing_len:   i32,
}

pub struct Scanner<D: LexerDef> {
    // I/O
    pub yyin:           Box<dyn Read>,
    pub yyout:          Box<dyn Write>,

    // Public state
    pub yytext:         String,
    pub yyleng:         usize,
    pub yylineno:       u32,

    // Internal buffer
    yybuf:              Vec<u8>,
    yybuf_pos:          usize,
    
    match_start:        usize,

    // Automaton
    yycurrent_state:    usize,
    yy_at_bol:          bool,
    yymore_flag:        bool,
    yymore_len:         usize,
    reject_flag:        bool,

    // Candidates
    candidates:         Vec<Candidate>,

    _phantom:           PhantomData<D>,
}

impl<D: LexerDef> Scanner<D> {
    pub fn new(yyin: Box<dyn Read>, yyout: Box<dyn Write>) -> Self {
        Scanner {
            yyin,
            yyout,
            yytext:             String::new(),
            yyleng:             0,
            yylineno:           1,
            yybuf:              Vec::new(),
            yybuf_pos:          0,
            match_start:        0,
            yycurrent_state:    0,
            yy_at_bol:          true,
            yymore_flag:        false,
            yymore_len:         0,
            reject_flag:        false,
            candidates:         Vec::new(),
            _phantom:           PhantomData,
        }
    }

    pub fn echo(&mut self) {
        let _ = self.yyout.write_all(self.yytext.as_bytes());
    }

    pub fn begin(&mut self, condition: usize) {
        self.yycurrent_state = condition;
    }

    pub fn yyless(&mut self, n: usize) {
        self.yytext.truncate(n);
        self.yyleng = n;
        self.yybuf_pos = self.match_start + n;
    }

    pub fn yymore(&mut self) {
        self.yymore_flag = true;
    }

    pub fn reject(&mut self) {
        self.reject_flag = true;
    }

    fn yyread(&mut self) -> Option<u8> {
        if self.yybuf_pos < self.yybuf.len() {
            let c = self.yybuf[self.yybuf_pos];
            self.yybuf_pos += 1;
            return Some(c);
        }

        let mut byte = [0u8; 1];
        match self.yyin.read(&mut byte) {
            Ok(1) => {
                self.yybuf.push(byte[0]);
                self.yybuf_pos += 1;
                Some(byte[0])
            }
            _ => None,
        }
    }

    fn assign_yytext(&mut self, start: usize, len: usize) {
        let slice = &self.yybuf[start..start + len];
        if self.yymore_len > 0 {
            let extra = String::from_utf8_lossy(slice).into_owned();
            self.yytext.push_str(&extra);
        } else {
            self.yytext = String::from_utf8_lossy(slice).into_owned();
        }
        self.yyleng = self.yymore_len + len;
    }

    pub fn yylex(&mut self) -> i32 {
        // Reset yytext unless yymore() was requested
        if self.yymore_flag {
            self.yymore_flag = false;
        } else {
            self.yytext.clear();
            self.yyleng = 0;
            self.yymore_len = 0;
        }

    	/* ===================================================================
	    * Outer loop: scan and dispatch one token per iteration.
	    * =================================================================== */  
        loop {
            let state = D::start_state(
                self.yycurrent_state,
                self.yy_at_bol,
            );

            self.yybuf_pos = self.match_start;
            self.candidates.clear();

            let mut current_state = state as i32;
            /* -----------------------------------------------------------------
            * SCAN PHASE — drive the DFA and collect all possible candidates.      
            * ----------------------------------------------------------------- */
            loop {
                match self.yyread() {
                    None => {
                        // EOF
                        if !self.candidates.is_empty() {
                            break;
                        }
                        if D::yywrap(self) {
                            return 0;
                        }
                        continue;
                    }
                    Some(c) => {
                        current_state = D::transition(
                            current_state as usize, c
                        );

                        if current_state == -1
                            || current_state == D::sink() as i32
                        {
                            break;
                        }

                        let entries = D::accept_entries(
                            current_state as usize
                        );
                        if !entries.is_empty() {
                            let pos = self.yybuf_pos;
                            for entry in entries.iter().rev() {
                                self.candidates.push(Candidate {
                                    rule_id: entry.0,
                                    match_end: pos,
                                    trailing_len: entry.1,
                                });
                            }
                        }
                    }
                }
            }

            /* -----------------------------------------------------------------
		    * No candidate: POSIX default rule — echo the first unmatched char.
		    * ----------------------------------------------------------------- */
            if self.candidates.is_empty() {
                let c = self.yybuf[self.match_start];
                let _ = self.yyout.write_all(&[c]);
                self.match_start += 1;
                continue;
            }

            /* -----------------------------------------------------------------
		    * DISPATCH PHASE - try candidates from highest to lowest priority.
            * ----------------------------------------------------------------- */
            let mut rule_executed = false;

            // !! clone to avoid borrow conflict !!
            let candidates: Vec<Candidate> = self.candidates.drain(..).collect();

            for cand in candidates.iter().rev() {
                let full_len = cand.match_end - self.match_start;
                let committed_len = if cand.trailing_len >= 0 {
                    full_len - cand.trailing_len as usize
                } else {
                    full_len
                };

                let bu_yymore_len = self.yymore_len;
                self.assign_yytext(self.match_start, committed_len);
                self.yybuf_pos = self.match_start + committed_len;
                self.reject_flag = false;

                let result = D::execute_action(self, cand.rule_id);
                if !self.reject_flag {
                    // Update yylineno and yy_at_bol
                    for i in bu_yymore_len..self.yyleng {
                        if self.yytext.as_bytes()[i] == b'\n' {
                            self.yylineno += 1;
                        }
                    }
                    self.yy_at_bol = self.yytext
                        .as_bytes()
                        .last()
                        .copied()
                        == Some(b'\n');

                    rule_executed = true;
                    if let Some(v) = result {
                        self.match_start = self.yybuf_pos;
                        self.yytext.clear();
                        self.yyleng = 0;
                        self.yymore_len = 0;
                        self.yymore_flag = false;
                        return v;
                    }
                    break;
                }
            }

            if !rule_executed {
                let c = self.yybuf[self.match_start];
                let _ = self.yyout.write_all(&[c]);
                self.match_start += 1;
                continue;
            }

            self.match_start = self.yybuf_pos;

            if self.yymore_flag {
                self.yymore_len = self.yyleng;
            } else {
                self.yytext.clear();
                self.yyleng = 0;
                self.yymore_len = 0;
            }
            self.yymore_flag = false;
        }
    }
}

