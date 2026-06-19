use std::marker::PhantomData;
use std::io::{Read, Write};

pub trait LexerDef {
    type UserData;

    fn ftlex_main<D: LexerDef>(user_data: D::UserData) {
        let mut scanner = Scanner::<D>::new(
            Box::new(std::io::stdin()),
            Box::new(std::io::stdout()),
            user_data,
        );
        loop {
            if scanner.yylex() == 0 { break; }
        }
    }

    fn transition(state: usize, c: u8) -> i32;

    /// Accept entries for a state: (rule_id, trailing_len, trailing_dfa_id).
    /// trailing_len: -1 = no trailing context, -2 = variable-length trailing.
    /// trailing_dfa_id: -1 unless trailing_len == -2.
    fn accept_entries(state: usize) -> &'static [(i32, i32, i32)];

    fn start_state(condition: usize, bol: bool) -> usize;
    fn sink() -> usize;

    fn yywrap(_scanner: &mut Scanner<Self>) -> bool
    where Self: Sized
    {
        true
    }

    fn execute_action(scanner: &mut Scanner<Self>, rule_id: i32) -> Option<i32>
    where Self: Sized;

    /// Transition function for an isolated trailing-context DFA.
    /// Returns -1 on a dead transition. Default: no trailing DFAs exist.
    fn yytrailing_transition(_dfa_id: usize, _state: usize, _c: u8) -> i32 {
        -1
    }

    /// Whether `state` is an accepting state in trailing DFA `dfa_id`.
    fn yytrailing_accept(_dfa_id: usize, _state: usize) -> bool {
        false
    }
}

pub fn ftlex_main<D: LexerDef>(user_data: D::UserData) {
    D::ftlex_main::<D>(user_data);
}

struct Candidate {
    rule_id:         i32,
    match_end:       usize,
    trailing_len:    i32,
    trailing_dfa_id: i32,
}

pub struct Scanner<D: LexerDef> {
    // I/O
    pub yyin:           Box<dyn Read>,
    pub yyout:          Box<dyn Write>,

    // Public state
    pub yytext:         String,
    pub yyleng:         usize,
    pub yylineno:       u32,
    pub user_data:      D::UserData,

    // Internal buffer
    yybuf:              Vec<u8>,
    yybuf_pos:          usize,

    match_start:        usize,

    // Automaton
    yycurrent_state:    usize,
    yymore_flag:        bool,
    yymore_len:         usize,
    reject_flag:        bool,

    // Candidates
    candidates:         Vec<Candidate>,

    _phantom:           PhantomData<D>,
}

impl<D: LexerDef> Scanner<D> {
    pub fn new(yyin: Box<dyn Read>, yyout: Box<dyn Write>, user_data: D::UserData) -> Self {
        Scanner {
            yyin,
            yyout,
            yytext:             String::new(),
            yyleng:             0,
            yylineno:           1,
            user_data,
            yybuf:              Vec::new(),
            yybuf_pos:          0,
            match_start:        0,
            yycurrent_state:    0,
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

    /// Read and consume the next raw byte from the input stream, like
    /// POSIX lex's input(). Returns -1 on EOF, otherwise the byte value.
    pub fn input(&mut self) -> i32 {
        match self.yyread() {
            Some(c) => {
                if c == b'\n' {
                    self.yylineno += 1;
                }
                c as i32
            }
            None => -1,
        }
    }

    /// Push a byte back into the input stream so the next input() (or
    /// scan) call rereads it, like POSIX lex's unput().
    pub fn unput(&mut self, c: u8) {
        if self.yybuf_pos == 0 {
            self.yybuf.insert(0, c);
        } else {
            self.yybuf_pos -= 1;
            self.yybuf.insert(self.yybuf_pos, c);
        }

        if c == b'\n' && self.yylineno > 1 {
            self.yylineno -= 1;
        }
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

    /// Simulate an isolated trailing-context DFA over yybuf[start..end)
    /// and return the length of the base pattern (i.e. the leftmost split
    /// point whose suffix is fully accepted by the trailing DFA).
    /// Mirrors the C runtime's yy_simulate_trailing: leftmost split =
    /// longest base pattern, consistent with POSIX longest-match rules.
    fn simulate_trailing(&self, dfa_id: i32, start: usize, end: usize) -> Option<usize> {
        for p in start..=end {
            let mut state = 0usize;
            let mut pos = p;
            let mut alive = true;

            while pos < end {
                let c = self.yybuf[pos];
                let next = D::yytrailing_transition(dfa_id as usize, state, c);
                if next < 0 {
                    alive = false;
                    break;
                }
                state = next as usize;
                pos += 1;
            }

            if alive && pos == end && D::yytrailing_accept(dfa_id as usize, state) {
                return Some(p - start);
            }
        }
        None
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
            // Recomputed from the buffer every pass: a trailing '\n' left
            // unconsumed by a previous match (e.g. via ".*") must still
            // count as BOL for the next token, regardless of what the
            // previous yytext looked like.
            let yy_at_bol = self.match_start == 0
                || self.yybuf[self.match_start - 1] == b'\n';

            let state = D::start_state(
                self.yycurrent_state,
                yy_at_bol,
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
                                    trailing_dfa_id: entry.2,
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
                } else if cand.trailing_len == -2 {
                    match self.simulate_trailing(cand.trailing_dfa_id, self.match_start, cand.match_end) {
                        Some(len) => len,
                        None => continue, // trailing DFA never accepted: reject this candidate
                    }
                } else {
                    full_len
                };

                let bu_yymore_len = self.yymore_len;
                self.assign_yytext(self.match_start, committed_len);
                self.yybuf_pos = self.match_start + committed_len;
                self.reject_flag = false;

                let result = D::execute_action(self, cand.rule_id);
                if !self.reject_flag {
                    // Update yylineno (yy_at_bol is now derived from the
                    // buffer at the top of the outer loop, not here).
                    for i in bu_yymore_len..self.yyleng {
                        if self.yytext.as_bytes()[i] == b'\n' {
                            self.yylineno += 1;
                        }
                    }

                    rule_executed = true;
                    self.match_start = self.yybuf_pos;

                    if let Some(v) = result {
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
