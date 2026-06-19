%s s1 s2
%x x1 x2
%rust_user_data FileList

DEFINE def*i?ne

%{
struct FileList {
    files: Vec<String>,
    index: usize,
}
%}

%%

simple { print!("simple"); return Some(scanner.yyleng as i32); }

simple* {
    print!("simple*");
    if true {
        return Some(scanner.yyleng as i32);
    }
}

(simple){2,4} { print!("(simple){{2,4}}"); return Some(scanner.yyleng as i32); }

(aa|aaa)*aa		{ print!("(aa|aaa)*aa"); scanner.echo(); return Some(scanner.yyleng as i32); }

alternative|ALTERNATIVE { print!("alternative|ALTERNATIVE"); return Some(scanner.yyleng as i32); }

escaping\"\[\\\|\{\$ { print!("escaping\\\"\\[\\\\\\|\\{{\\$"); return Some(scanner.yyleng as i32); }

^anchoring		{ print!("^anchoring"); return Some(scanner.yyleng as i32); }
anchoring		{ print!("anchoring"); return Some(scanner.yyleng as i32); }

^anchoring$		{ print!("^anchoring$"); return Some(scanner.yyleng as i32); }
anchoring$		{ print!("anchoring$"); return Some(scanner.yyleng as i32); }

"regex with"/\ trai*li*ng { print!("\"regex with\"/trai*li*ng"); return Some(scanner.yyleng as i32); }

[]]*					{ print!("[]]*"); return Some(scanner.yyleng as i32); }

inverted[^ inverted]*	{ print!("inverted[^inverted]*"); return Some(scanner.yyleng as i32); }

class[[:digit:]]+		{ print!("class[[:digit:]]"); return Some(scanner.yyleng as i32); }

hex\x61*				{ print!("hex\x61*"); return Some(scanner.yyleng as i32); }

octa\154?\114?			{ print!("octal?L?"); return Some(scanner.yyleng as i32); }

{DEFINE}*				{ print!("{{DEFINE}}*"); return Some(scanner.yyleng as i32); }
switch_to_s1 { scanner.begin(s1); }

switch_to_x1 { scanner.begin(s1); }

<x1>switch_from_x1_to_INITIAL { scanner.begin(INITIAL); }

<s1>"state_specific" { print!("<s1>\"state s1\""); return Some(scanner.yyleng as i32); }

<s1>"state_specific"/with_trailing { print!("<s1>\"state_specific\"/with_trailing"); return Some(scanner.yyleng as i32); }

<x1>"state_specific" { print!("<x1>\"state s1\""); return Some(scanner.yyleng as i32); }

<x1,INITIAL>"x1 or initial" { print!("<x1,INITIAL>\"x1 or initial\""); return Some(scanner.yyleng as i32); }

lexalternative	|
LEXALTERNATIVE 	{ print!("lexalternative | LEXALTERNATIVE"); return Some(scanner.yyleng as i32); }

A	{ print!("A"); return Some(scanner.yyleng as i32); }

AB	{ print!("AB"); scanner.reject(); return Some(scanner.yyleng as i32); }

ABC { print!("ABC"); scanner.reject(); return Some(scanner.yyleng as i32); }

BC	{ print!("BC"); return Some(scanner.yyleng as i32); }

ab		{ scanner.unput(b'c'); scanner.yymore(); print!("ab"); }

cdfg	{ print!("cdfg"); return Some(scanner.yyleng as i32); }

abcdefg { scanner.yyless(4); print!("abcdefg"); return Some(scanner.yyleng as i32); }

efg	{ print!("cdef{}", scanner.input() as u8 as char); return Some(scanner.yyleng as i32); }

.*	{ print!(".*"); return Some(scanner.yyleng as i32); }

%%
fn user_yywrap(scanner: &mut Scanner<GeneratedLexer>) -> bool {
    if scanner.user_data.index >= scanner.user_data.files.len() {
        return true;
    }
    let path = scanner.user_data.files[scanner.user_data.index].clone();
    scanner.user_data.index += 1;
    match std::fs::File::open(&path) {
        Ok(f) => {
            scanner.yyin = Box::new(f);
            false
        }
        Err(_) => true,
    }
}

fn main() {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let user_data = FileList { files: args, index: 0 };

    let mut scanner = Scanner::<GeneratedLexer>::new(
        Box::new(std::io::empty()),
        Box::new(std::io::stdout()),
        user_data,
    );

    // Mirrors the C version's manual first yywrap() call before the loop,
    // to open the first file ahead of scanning.
    GeneratedLexer::yywrap(&mut scanner);

    loop {
        let ret = scanner.yylex();
        if ret == 0 {
            break;
        }
        println!(" ret: {} yytext: [{}]", ret, scanner.yytext);
    }
}
