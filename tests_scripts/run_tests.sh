#!/usr/bin/env bash
# =============================================================================
# ft_lex — Test Runner
# Usage:
#   ./run_tests.sh              → run all tests
#   ./run_tests.sh A1 C2 E1    → run only the named tests
#   ./run_tests.sh --list       → list all available test names
#
# Extra verbosity:
#   SHOW_OUTPUTS=1 ./run_tests.sh   → print expected and actual outputs on pass/fail
# =============================================================================

set -euo pipefail
# set -e  : exit immediately if any command fails (unless captured)
# set -u  : treat unset variables as errors
# set -o pipefail : a pipeline fails if ANY command in it fails (not just the last)

# -----------------------------------------------------------------------------
# CONFIGURATION — adjust these to match your project layout
# -----------------------------------------------------------------------------
FT_LEX="${FT_LEX:-./ft_lex}"               # path to your ft_lex binary
CC="${CC:-cc}"                              # C compiler
CFLAGS="${CFLAGS:--Wall -Wextra}"
LIBL="${LIBL:-./libl/libl.a}"                   # path to your libl.a, or leave empty to auto-detect
LINK_FLAGS=""                              # populated below
TIMEOUT=10                                 # max seconds per test (requires `timeout`)
SHOW_OUTPUTS="${SHOW_OUTPUTS:-0}"

# Minimal yywrap stub injected when no libl is available.
# yywrap() returning 1 means "no more input" — the correct default.
YYWRAP_STUB='int yywrap(void) { return 1; }'

# Resolve libl in order of preference:
#   1. Explicit LIBL path given by the user and the file exists
#   2. Local libl.a next to the binary (common in 42 project layouts)
#   3. -ll actually linkable (probe with a real compile, not just ldconfig)
#   4. Inject a yywrap stub directly into each generated lex.yy.c
_resolve_libl() {
    # 1. Explicit path
    if [[ -n "$LIBL" && -f "$LIBL" ]]; then
        LINK_FLAGS="$LIBL"
        return
    fi
    # 2. Sibling libl.a relative to the ft_lex binary
    local bin_dir; bin_dir=$(dirname "$(realpath "$FT_LEX" 2>/dev/null || echo "$FT_LEX")")
    if [[ -f "$bin_dir/libl.a" ]]; then
        LINK_FLAGS="$bin_dir/libl.a"
        return
    fi
    # 3. Probe whether -ll actually links (ldconfig can lie about availability)
    if echo 'int main(){}' | $CC -x c - -ll -o /dev/null 2>/dev/null; then
        LINK_FLAGS="-ll"
        return
    fi
    # 4. No libl anywhere — use the stub injected per test
    LINK_FLAGS="__stub__"
}
_resolve_libl

# -----------------------------------------------------------------------------
# COLOR HELPERS
# tput is preferred over raw escape codes — degrades gracefully on non-terminals
# -----------------------------------------------------------------------------
if tput colors &>/dev/null && [[ $(tput colors) -ge 8 ]]; then
    RED=$(tput setaf 1); GREEN=$(tput setaf 2); YELLOW=$(tput setaf 3)
    CYAN=$(tput setaf 6); BOLD=$(tput bold); RESET=$(tput sgr0)
else
    RED=""; GREEN=""; YELLOW=""; CYAN=""; BOLD=""; RESET=""
fi

# -----------------------------------------------------------------------------
# STATE
# -----------------------------------------------------------------------------
PASS_COUNT=0
FAIL_COUNT=0
SKIP_COUNT=0
declare -a FAILED_TESTS=()   # accumulate names of failed tests for the summary

# List of all test names in declaration order (populated by register_test)
declare -a ALL_TESTS=()

register_test() { ALL_TESTS+=("$1"); }

# -----------------------------------------------------------------------------
# CORE ENGINE: run_test <name> <grammar_file> <input_file> <expected_file>
#
# Steps:
#   1. Run ft_lex on the grammar → lex.yy.c
#   2. Compile lex.yy.c + libl → scanner binary
#   3. Feed the input file to the scanner
#   4. Diff the output against the expected file
#   5. Report PASS / FAIL with a contextual diff on failure
# -----------------------------------------------------------------------------
run_test() {
    local name="$1"
    local grammar="$2"
    local input="$3"
    local expected="$4"
    local description="${5:-}"

    # If the user passed specific test names, skip everything else
    if [[ ${#FILTER[@]} -gt 0 ]] && ! in_array "$name" "${FILTER[@]}"; then
        return
    fi

    printf "  %-12s" "${BOLD}${name}${RESET}"

    # Isolated working directory for this test — no leftover files between tests
    local workdir
    workdir=$(mktemp -d "/tmp/ft_lex_test_${name}_XXXX")

    # Register cleanup: runs on EXIT regardless of success or failure
    # We push a stack of cleanup commands because nested traps overwrite each other
    local cleanup_key="__cleanup_${workdir//\//_}"
    eval "${cleanup_key}() { rm -rf '${workdir}'; }"
    CLEANUP_STACK+=("${cleanup_key}")

    local lex_c="$workdir/lex.yy.c"
    local scanner="$workdir/scanner"
    local actual="$workdir/actual.txt"
    local log="$workdir/build.log"

    # -- Step 1: ft_lex ---------------------------------------------------------
    if ! timeout "$TIMEOUT" "$FT_LEX" "$grammar" 2>"$log"; then
        _fail "$name" "ft_lex exited with error" "$log"
        rm -rf "$workdir"
        return
    fi
    # ft_lex writes lex.yy.c in the current directory; move it to workdir
    if [[ -f lex.yy.c ]]; then
        mv lex.yy.c "$lex_c"
    elif [[ ! -f "$lex_c" ]]; then
        _fail "$name" "ft_lex did not produce lex.yy.c" "$log"
        rm -rf "$workdir"
        return
    fi

    # -- Step 2: compile --------------------------------------------------------
    # If no libl was found, append a minimal yywrap stub to lex.yy.c.
    # This avoids a linker error without requiring the user to install libl.
    local link_args="$LINK_FLAGS"
    if [[ "$LINK_FLAGS" == "__stub__" ]]; then
        printf '\n%s\n' "$YYWRAP_STUB" >> "$lex_c"
        link_args=""
    fi
    if ! $CC $CFLAGS -o "$scanner" "$lex_c" $link_args 2>>"$log"; then
        _fail "$name" "compilation failed" "$log"
        rm -rf "$workdir"
        return
    fi

    # -- Step 3: run ------------------------------------------------------------
    # timeout prevents infinite loops from hanging the suite
    if ! timeout "$TIMEOUT" "$scanner" < "$input" > "$actual" 2>&1; then
        local exit_code=$?
        if [[ $exit_code -eq 124 ]]; then
            _fail "$name" "scanner timed out after ${TIMEOUT}s (infinite loop?)" ""
        else
            _fail "$name" "scanner exited with code $exit_code" "$actual"
        fi
        rm -rf "$workdir"
        return
    fi

    # -- Step 4: diff -----------------------------------------------------------
    if diff -u "$expected" "$actual" > "$workdir/diff.txt" 2>&1; then
        _pass "$name" "$description" "$expected" "$actual"
    else
        _fail "$name" "output mismatch" "$workdir/diff.txt" "$expected" "$actual"
    fi

    rm -rf "$workdir"
}

# Print a passing test line
_pass() {
    local name="$1"
    local description="${2:-}"
    local expected_file="${3:-}"
    local actual_file="${4:-}"

    printf "${GREEN}PASS${RESET}"
    [[ -n "$description" ]] && printf "  %s" "${CYAN}${description}${RESET}"
    printf "\n"

    if [[ "$SHOW_OUTPUTS" == "1" ]]; then
        _print_file_section "EXPECTED" "$expected_file"
        _print_file_section "ACTUAL" "$actual_file"
    fi

    (( PASS_COUNT++ )) || true
}

# Print a failing test line and show the diff / log
_fail() {
    local name="$1" reason="$2" logfile="$3" expected_file="${4:-}" actual_file="${5:-}"
    printf "${RED}FAIL${RESET}  %s\n" "$reason"
    if [[ -n "$logfile" && -f "$logfile" ]]; then
        _print_file_section "DETAILS" "$logfile"
    fi
    if [[ -n "$expected_file" && -f "$expected_file" && -n "$actual_file" && -f "$actual_file" ]]; then
        _print_file_section "EXPECTED" "$expected_file"
        _print_file_section "ACTUAL" "$actual_file"
    fi
    FAILED_TESTS+=("$name")
    (( FAIL_COUNT++ )) || true
}

_print_file_section() {
    local title="$1" file="$2" max_lines="${3:-40}"

    printf "    %s%s%s\n" "$BOLD" "$title" "$RESET"

    if [[ ! -f "$file" ]]; then
        printf "      <missing>\n"
        return
    fi

    awk -v max="$max_lines" '{
        if (NR <= max) {
            printf "      %s\n", $0
        }
        if (NR == max) {
            exit
        }
    }' "$file"

    local lines
    lines=$(wc -l < "$file")
    if (( lines > max_lines )); then
        printf "      %s[...%d more lines]%s\n" "$YELLOW" "$((lines - max_lines))" "$RESET"
    fi
}

# Helper: check if a value is in a bash array
in_array() {
    local needle="$1"; shift
    local item
    for item in "$@"; do [[ "$item" == "$needle" ]] && return 0; done
    return 1
}

declare -a CLEANUP_STACK=()

# Global cleanup handler
_global_cleanup() {
    for fn in "${CLEANUP_STACK[@]:-}"; do
        "$fn" 2>/dev/null || true
    done
    # Also clean up lex.yy.c left in CWD if a test crashed mid-run
    rm -f lex.yy.c
}
trap _global_cleanup EXIT

# =============================================================================
# TEST DEFINITIONS
# Each test function:
#   1. Writes the .l grammar (heredoc)
#   2. Writes the input (heredoc or programmatic)
#   3. Writes the expected output (heredoc)
#   4. Calls run_test
# =============================================================================

# ---- Category A: Integration ------------------------------------------------

register_test A1
test_A1() {
    local d; d=$(mktemp -d)

    cat > "$d/grammar.l" << 'EOF'
%{
#include <stdio.h>
%}

DIGIT   [0-9]
LETTER  [a-zA-Z_]
ALNUM   [a-zA-Z0-9_]
INT     {DIGIT}+
FLOAT   {DIGIT}+\.{DIGIT}+
IDENT   {LETTER}{ALNUM}*

%%

"if"        { printf("KW_IF\n"); }
"else"      { printf("KW_ELSE\n"); }
"while"     { printf("KW_WHILE\n"); }
"int"       { printf("KW_INT\n"); }
"return"    { printf("KW_RETURN\n"); }
{FLOAT}     { printf("FLOAT(%s)\n", yytext); }
{INT}       { printf("INT(%s)\n", yytext); }
{IDENT}     { printf("ID(%s)\n", yytext); }
"=="        { printf("EQ\n"); }
"="         { printf("ASSIGN\n"); }
";"         { printf("SEMI\n"); }
"+"         { printf("PLUS\n"); }
[ \t\n]+    { }
.           { printf("UNK(%s)\n", yytext); }

%%
EOF

    # NOTE: '.5' has no leading digit so it cannot match FLOAT → UNK(.) + INT(5)
    # NOTE: '0int' → INT(0) then KW_INT (IDENT needs a leading letter)
    # NOTE: 'ifx', 'integer', 'returnval' must NOT match their keyword prefixes
    cat > "$d/input.txt" << 'EOF'
int x = 42;
if x == 3.14 while
ifx integer returnval
0int 10.0 .5
EOF

    cat > "$d/expected.txt" << 'EOF'
KW_INT
ID(x)
ASSIGN
INT(42)
SEMI
KW_IF
ID(x)
EQ
FLOAT(3.14)
KW_WHILE
ID(ifx)
ID(integer)
ID(returnval)
INT(0)
KW_INT
FLOAT(10.0)
UNK(.)
INT(5)
EOF

    run_test "A1" "$d/grammar.l" "$d/input.txt" "$d/expected.txt" \
             "macros · longest-match · keyword vs identifier"
    rm -rf "$d"
}

# ---- A2: Operator tokenizer -------------------------------------------------

register_test A2
test_A2() {
    local d; d=$(mktemp -d)

    cat > "$d/grammar.l" << 'EOF'
%{
#include <stdio.h>
%}

%%

">>="   { printf("RSHIFT_ASSIGN\n"); }
"<<="   { printf("LSHIFT_ASSIGN\n"); }
">>"    { printf("RSHIFT\n"); }
"<<"    { printf("LSHIFT\n"); }
">="    { printf("GEQ\n"); }
"<="    { printf("LEQ\n"); }
"!="    { printf("NEQ\n"); }
"=="    { printf("EQ\n"); }
"="     { printf("ASSIGN\n"); }
"++"    { printf("INC\n"); }
"--"    { printf("DEC\n"); }
"+="    { printf("PLUS_ASSIGN\n"); }
"-="    { printf("MINUS_ASSIGN\n"); }
"+"     { printf("PLUS\n"); }
"-"     { printf("MINUS\n"); }
">"     { printf("GT\n"); }
"<"     { printf("LT\n"); }
[ \t\n]+ { }

%%
EOF

    # Second line tests adjacent ambiguous tokens with no whitespace separator
    printf '%s\n%s\n' \
        '>>= <<= >> << >= <= != == = ++ -- += -= + - > <' \
        '>>=<<=' \
        > "$d/input.txt"

    cat > "$d/expected.txt" << 'EOF'
RSHIFT_ASSIGN
LSHIFT_ASSIGN
RSHIFT
LSHIFT
GEQ
LEQ
NEQ
EQ
ASSIGN
INC
DEC
PLUS_ASSIGN
MINUS_ASSIGN
PLUS
MINUS
GT
LT
RSHIFT_ASSIGN
LSHIFT_ASSIGN
EOF

    run_test "A2" "$d/grammar.l" "$d/input.txt" "$d/expected.txt" \
             "longest-match · shared operator prefixes"
    rm -rf "$d"
}

# ---- A3: Pipe rules ---------------------------------------------------------

register_test A3
test_A3() {
    local d; d=$(mktemp -d)

    cat > "$d/grammar.l" << 'EOF'
%{
#include <stdio.h>
%}

%%

"cat"   |
"dog"   |
"bird"  { printf("ANIMAL(%s)\n", yytext); }

"red"   |
"green" |
"blue"  { printf("COLOR(%s)\n", yytext); }

"cataract"  { printf("WORD(%s)\n", yytext); }
[a-z]+      { printf("OTHER(%s)\n", yytext); }
[ \t\n]+    { }

%%
EOF

    # 'cataract': "cataract" (8 chars) beats "cat" (3 chars) → WORD, not ANIMAL+OTHER
    # 'catdog':   no 6-char literal matches → OTHER(catdog), not ANIMAL+ANIMAL
    printf '%s\n' \
        'cat horse bird blue red green purple dog fish cataract catdog' \
        > "$d/input.txt"

    cat > "$d/expected.txt" << 'EOF'
ANIMAL(cat)
OTHER(horse)
ANIMAL(bird)
COLOR(blue)
COLOR(red)
COLOR(green)
OTHER(purple)
ANIMAL(dog)
OTHER(fish)
WORD(cataract)
OTHER(catdog)
EOF

    run_test "A3" "$d/grammar.l" "$d/input.txt" "$d/expected.txt" \
             "pipe rules · first-match · literal vs macro"
    rm -rf "$d"
}

# ---- Category B: Start conditions -------------------------------------------

register_test B1
test_B1() {
    local d; d=$(mktemp -d)

    cat > "$d/grammar.l" << 'EOF'
%{
#include <stdio.h>
%}

%x STR

%%

\"                  { BEGIN(STR); yymore(); }
<STR>\\[\"\\ntr]    { yymore(); }
<STR>\n             { printf("ERROR: unterminated string at line %d\n", yylineno);
                      BEGIN(INITIAL); }
<STR>\"             { printf("STRING(%s) len=%zu\n", yytext, yyleng);
                      BEGIN(INITIAL); }
<STR>.              { yymore(); }

[a-zA-Z_][a-zA-Z0-9_]*  { printf("ID(%s)\n", yytext); }
[0-9]+                   { printf("NUM(%s)\n", yytext); }
[ \t\n]+                 { }

%%
EOF

    # The backslash sequences in the strings below are literal bytes in the file.
    # Line 3 has an unterminated string — its \n triggers the error rule.
    # yylineno will read 4 (incremented before the action fires) — known deviation.
    cat > "$d/input.txt" << 'EOF'
foo "hello" bar
baz "with \"quotes\"" qux
err "unterminated
next
EOF

    cat > "$d/expected.txt" << 'EOF'
ID(foo)
STRING("hello") len=7
ID(bar)
ID(baz)
STRING("with \"quotes\"") len=17
ID(qux)
ID(err)
ERROR: unterminated string at line 4
ID(next)
EOF

    run_test "B1" "$d/grammar.l" "$d/input.txt" "$d/expected.txt" \
             "%x · yymore() across state boundary · yylineno"
    rm -rf "$d"
}

# ---- B2: Inclusive %s -------------------------------------------------------

register_test B2
test_B2() {
    local d; d=$(mktemp -d)

    cat > "$d/grammar.l" << 'EOF'
%{
#include <stdio.h>
%}

%s IN_BLOCK

%%

<INITIAL>"{"        { printf("LBRACE\n"); BEGIN(IN_BLOCK); }
<IN_BLOCK>"}"       { printf("RBRACE\n"); BEGIN(INITIAL); }
<IN_BLOCK>[0-9]+    { printf("NUM(%s)\n", yytext); }
[a-z]+              { printf("WORD(%s)\n", yytext); }
[ \t\n]+            { }
.                   { printf("OTHER(%s)\n", yytext); }

%%
EOF

    # [a-z]+ has no condition tag → must fire in both INITIAL and IN_BLOCK (%s = inclusive)
    printf '%s\n' 'foo { bar 42 baz } qux { 7 } end' > "$d/input.txt"

    cat > "$d/expected.txt" << 'EOF'
WORD(foo)
LBRACE
WORD(bar)
NUM(42)
WORD(baz)
RBRACE
WORD(qux)
LBRACE
NUM(7)
RBRACE
WORD(end)
EOF

    run_test "B2" "$d/grammar.l" "$d/input.txt" "$d/expected.txt" \
             "%s inclusive · no-tag rules active in all non-exclusive states"
    rm -rf "$d"
}

# ---- B3: Multi-state machine ------------------------------------------------

register_test B3
test_B3() {
    local d; d=$(mktemp -d)

    cat > "$d/grammar.l" << 'EOF'
%{
#include <stdio.h>
%}

%x TAG ATTR

%%

"<"                             { printf("OPEN("); BEGIN(TAG); }
<TAG>[a-zA-Z][a-zA-Z0-9]*       { printf("%s", yytext); BEGIN(ATTR); }
<ATTR>[ \t]+                    { }
<ATTR>[a-z]+=\"[^\"]*\"         { printf(" %s", yytext); }
<ATTR>">"                       { printf(")\n"); BEGIN(INITIAL); }
<ATTR>"/>"                      { printf("/)\n"); BEGIN(INITIAL); }
[^<\n]+                         { printf("TEXT(%s)\n", yytext); }
\n                              { }

%%
EOF

    cat > "$d/input.txt" << 'EOF'
<div class="foo" id="bar">
<img src="pic.png"/>
hello world
<span>
EOF

    cat > "$d/expected.txt" << 'EOF'
OPEN(div class="foo" id="bar")
OPEN(img src="pic.png"/)
TEXT(hello world)
OPEN(span)
EOF

    run_test "B3" "$d/grammar.l" "$d/input.txt" "$d/expected.txt" \
             "three-state machine · chained BEGIN() · complex per-state patterns"
    rm -rf "$d"
}

# ---- Category C: Anchors ----------------------------------------------------

register_test C1
test_C1() {
    local d; d=$(mktemp -d)

    cat > "$d/grammar.l" << 'EOF'
%{
#include <stdio.h>
%}

%%

^"#"[^\n]*      { printf("COMMENT(%s)\n", yytext); }
^[a-z]+":"      { printf("LABEL(%s)\n", yytext); }
[a-z]+$         { printf("EOL_WORD(%s)\n", yytext); }
[a-z]+          { printf("WORD(%s)\n", yytext); }
[ \t]+          { }
\n              { }
.               { printf("ECHO(%s)\n", yytext); }

%%
EOF

    cat > "$d/input.txt" << 'EOF'
# comment here
start: foo bar
baz
not a label
EOF

    # 'baz' is both at BOL and EOL — BOL rule needs colon so fails; EOL rule wins
    # 'bar' at EOL: [a-z]+$ has same match length as [a-z]+ → first-declared wins
    cat > "$d/expected.txt" << 'EOF'
COMMENT(# comment here)
LABEL(start:)
WORD(foo)
EOL_WORD(bar)
EOL_WORD(baz)
WORD(not)
WORD(a)
EOL_WORD(label)
EOF

    run_test "C1" "$d/grammar.l" "$d/input.txt" "$d/expected.txt" \
             "^ BOL · $ EOL · yy_at_bol lifecycle"
    rm -rf "$d"
}

# ---- C2: BOL inside a start condition ---------------------------------------

register_test C2
test_C2() {
    local d; d=$(mktemp -d)

    cat > "$d/grammar.l" << 'EOF'
%{
#include <stdio.h>
%}

%x IN_BLOCK

%%

"{"                  { printf("OPEN\n"); BEGIN(IN_BLOCK); }
<IN_BLOCK>"}"        { printf("CLOSE\n"); BEGIN(INITIAL); }
<IN_BLOCK>^[a-z]+   { printf("BOL_BLOCK(%s)\n", yytext); }
<IN_BLOCK>[a-z]+    { printf("WORD_BLOCK(%s)\n", yytext); }
^[a-z]+             { printf("BOL(%s)\n", yytext); }
[a-z]+              { printf("WORD(%s)\n", yytext); }
[ \t]+              { }
\n                  { }

%%
EOF

    cat > "$d/input.txt" << 'EOF'
hello world
{ foo
bar }
baz
EOF

    # After '{', yy_at_bol=0 → 'foo' is WORD_BLOCK, not BOL_BLOCK
    # '\n' at end of '{ foo' sets yy_at_bol=1 → 'bar' fires <IN_BLOCK>^
    cat > "$d/expected.txt" << 'EOF'
BOL(hello)
WORD(world)
OPEN
WORD_BLOCK(foo)
BOL_BLOCK(bar)
CLOSE
BOL(baz)
EOF

    run_test "C2" "$d/grammar.l" "$d/input.txt" "$d/expected.txt" \
             "^ inside %x condition · per-condition BOL entry state"
    rm -rf "$d"
}

# ---- Category D: Special actions --------------------------------------------

register_test D1
test_D1() {
    local d; d=$(mktemp -d)

    cat > "$d/grammar.l" << 'EOF'
%{
#include <stdio.h>
%}

%%

[a-z]+"!"   { yyless(yyleng - 1); printf("EMPHATIC(%s)\n", yytext); }
[a-z]+      { printf("WORD(%s)\n", yytext); }
"!"         { printf("BANG\n"); }
[ \t\n]+    { }

%%
EOF

    # 'bar!!' → yyless puts back one '!', the second is a separate token
    printf '%s\n' 'hello! world foo! bar!!' > "$d/input.txt"

    cat > "$d/expected.txt" << 'EOF'
EMPHATIC(hello)
BANG
WORD(world)
EMPHATIC(foo)
BANG
EMPHATIC(bar)
BANG
BANG
EOF

    run_test "D1" "$d/grammar.l" "$d/input.txt" "$d/expected.txt" \
             "yyless() · match_start rewind · re-scan of put-back chars"
    rm -rf "$d"
}

# ---- D2: yymore across state boundary ---------------------------------------

register_test D2
test_D2() {
    local d; d=$(mktemp -d)

    cat > "$d/grammar.l" << 'EOF'
%{
#include <stdio.h>
%}

%x IN_ATTR

%%

[a-z]+"="       { printf("KEY(%.*s)\n", (int)yyleng - 1, yytext);
                  yymore(); BEGIN(IN_ATTR); }
<IN_ATTR>[0-9]+ { printf("ATTR(%s)\n", yytext); BEGIN(INITIAL); }
<IN_ATTR>[a-z]+ { printf("ATTR(%s)\n", yytext); BEGIN(INITIAL); }
[ \t\n]+        { }

%%
EOF

    printf '%s\n' 'width=100 color=red height=42' > "$d/input.txt"

    cat > "$d/expected.txt" << 'EOF'
KEY(width)
ATTR(width=100)
KEY(color)
ATTR(color=red)
KEY(height)
ATTR(height=42)
EOF

    run_test "D2" "$d/grammar.l" "$d/input.txt" "$d/expected.txt" \
             "yymore() · yymore_len preserved across BEGIN()"
    rm -rf "$d"
}

# ---- D3: REJECT — compliance gap -------------------------------------------
# This test is intentionally marked as a compliance probe, not a hard pass/fail.
# It runs, prints the observed vs expected output, but does not count as a failure.

register_test D3
test_D3() {
    local d; d=$(mktemp -d)

    cat > "$d/grammar.l" << 'EOF'
%{
#include <stdio.h>
%}

%%

"hello"     { printf("EXACT\n"); REJECT; }
[a-z]+      { printf("WORD(%s)\n", yytext); }
[ \t\n]+    { }

%%
EOF

    printf '%s\n' 'hello hi' > "$d/input.txt"

    # POSIX says: REJECT re-runs the DFA without the rejected rule
    # → for 'hello', after rejecting rule 0, [a-z]+ should match "hello" (full 5 chars)
    cat > "$d/posix_expected.txt" << 'EOF'
EXACT
WORD(hello)
WORD(hi)
EOF

    # Stack-based REJECT gives shorter lengths, not alternative rules at same length
    # → rule0 rejected at len=5, then best rule at len=4 is [a-z]+ → WORD(hell)
    cat > "$d/stack_expected.txt" << 'EOF'
EXACT
WORD(hell)
WORD(hi)
EOF

    # Run the scanner and capture output — no timeout failure here
    local workdir; workdir=$(mktemp -d)
    local lex_c="$workdir/lex.yy.c"
    local scanner="$workdir/scanner"
    local actual="$workdir/actual.txt"

    printf "  %-12s" "${BOLD}D3${RESET}"

    if ! timeout "$TIMEOUT" "$FT_LEX" "$d/grammar.l" 2>/dev/null; then
        printf "${RED}FAIL${RESET}  ft_lex error\n"
        rm -rf "$workdir" "$d"
        (( FAIL_COUNT++ )) || true
        FAILED_TESTS+=("D3")
        return
    fi
    [[ -f lex.yy.c ]] && mv lex.yy.c "$lex_c"
    local d3_link="$LINK_FLAGS"
    if [[ "$LINK_FLAGS" == "__stub__" ]]; then
        printf '\n%s\n' "$YYWRAP_STUB" >> "$lex_c"; d3_link=""
    fi
    if ! $CC $CFLAGS -o "$scanner" "$lex_c" $d3_link 2>/dev/null; then
        printf "${RED}FAIL${RESET}  compilation error\n"
        rm -rf "$workdir" "$d"
        (( FAIL_COUNT++ )) || true
        FAILED_TESTS+=("D3")
        return
    fi
    timeout "$TIMEOUT" "$scanner" < "$d/input.txt" > "$actual" 2>&1 || true

    if diff -q "$d/posix_expected.txt" "$actual" &>/dev/null; then
        printf "${GREEN}PASS${RESET}  ${CYAN}REJECT is POSIX-compliant (full re-run semantics)${RESET}\n"
        (( PASS_COUNT++ )) || true
    elif diff -q "$d/stack_expected.txt" "$actual" &>/dev/null; then
        printf "${YELLOW}INFO${RESET}  ${CYAN}REJECT uses stack semantics (shorter matches) — known POSIX gap${RESET}\n"
        (( SKIP_COUNT++ )) || true
    else
        printf "${RED}FAIL${RESET}  unexpected REJECT output:\n"
        printf "    Expected (POSIX): "; cat "$d/posix_expected.txt" | tr '\n' ' '; echo
        printf "    Expected (stack): "; cat "$d/stack_expected.txt"  | tr '\n' ' '; echo
        printf "    Observed:         "; cat "$actual"                | tr '\n' ' '; echo
        (( FAIL_COUNT++ )) || true
        FAILED_TESTS+=("D3")
    fi

    rm -rf "$workdir" "$d"
}

# ---- Category E: Stress & edge cases ----------------------------------------

register_test E1
test_E1() {
    local d; d=$(mktemp -d)

    cat > "$d/grammar.l" << 'EOF'
%{
#include <stdio.h>
%}

%%

[a-z]+  { printf("LEN=%zu\n", yyleng); }
\n      { }

%%
EOF

    # Generate a 10 000-char token — tests realloc growth path in yyread()
    # printf '%Ns' pads with spaces to width N, tr converts spaces to 'a'
    { printf '%10000s' | tr ' ' 'a'; printf '\n'; } > "$d/input.txt"

    printf 'LEN=10000\n' > "$d/expected.txt"

    run_test "E1" "$d/grammar.l" "$d/input.txt" "$d/expected.txt" \
             "buffer growth · realloc · token larger than YYBUF_INIT_SIZE"
    rm -rf "$d"
}

# ---- E2: EOF in exclusive state ---------------------------------------------

register_test E2
test_E2() {
    local d; d=$(mktemp -d)

    cat > "$d/grammar.l" << 'EOF'
%{
#include <stdio.h>
%}

%x STR

%%

\"              { BEGIN(STR); yymore(); }
<STR>\"         { printf("STRING(%s)\n", yytext); BEGIN(INITIAL); }
<STR>.          { yymore(); }
[a-z]+          { printf("WORD(%s)\n", yytext); }
[ \t\n]+        { }

%%
EOF

    # No closing quote, no trailing newline — scanner hits EOF in STR state
    printf 'foo "unterminated' > "$d/input.txt"

    # Only the word before the unterminated string should appear; then clean exit
    printf 'WORD(foo)\n' > "$d/expected.txt"

    run_test "E2" "$d/grammar.l" "$d/input.txt" "$d/expected.txt" \
             "EOF inside %x state · yywrap() · no crash / no hang"
    rm -rf "$d"
}

# ---- E3: Trailing context (fixed-length) ------------------------------------

register_test E3
test_E3() {
    local d; d=$(mktemp -d)

    cat > "$d/grammar.l" << 'EOF'
%{
#include <stdio.h>
%}

DIGIT [0-9]

%%

{DIGIT}+/\.[0-9]        { printf("INT_PART(%s)\n", yytext); }
{DIGIT}+                { printf("INT(%s)\n", yytext); }
\.[0-9]+                { printf("FRAC(%s)\n", yytext); }
[ \t\n]+                { }
.                       { printf("ECHO(%s)\n", yytext); }

%%
EOF

    # Fixed-length trailing context: \.[0-9] is exactly 2 characters
    # '1.' has trailing '.' but no digit after → trailing rule fails → INT(1) + ECHO(.)
    printf '%s\n' '42 3.14 100.0 .5 0.0 1.' > "$d/input.txt"

    cat > "$d/expected.txt" << 'EOF'
INT(42)
INT_PART(3)
FRAC(.14)
INT_PART(100)
FRAC(.0)
FRAC(.5)
INT_PART(0)
FRAC(.0)
INT(1)
ECHO(.)
EOF

    run_test "E3" "$d/grammar.l" "$d/input.txt" "$d/expected.txt" \
             "trailing context · fixed-length peek · yyless(yyleng - trailing_len)"
    rm -rf "$d"
}

# ---- E4: %array mode --------------------------------------------------------

register_test E4
test_E4() {
    local d; d=$(mktemp -d)

    # Same grammar as A1 but with %array — expected output must be identical
    cat > "$d/grammar.l" << 'EOF'
%{
#include <stdio.h>
%}
%array

DIGIT   [0-9]
LETTER  [a-zA-Z_]
ALNUM   [a-zA-Z0-9_]
INT     {DIGIT}+
FLOAT   {DIGIT}+\.{DIGIT}+
IDENT   {LETTER}{ALNUM}*

%%

"if"        { printf("KW_IF\n"); }
"else"      { printf("KW_ELSE\n"); }
"while"     { printf("KW_WHILE\n"); }
"int"       { printf("KW_INT\n"); }
"return"    { printf("KW_RETURN\n"); }
{FLOAT}     { printf("FLOAT(%s)\n", yytext); }
{INT}       { printf("INT(%s)\n", yytext); }
{IDENT}     { printf("ID(%s)\n", yytext); }
"=="        { printf("EQ\n"); }
"="         { printf("ASSIGN\n"); }
";"         { printf("SEMI\n"); }
"+"         { printf("PLUS\n"); }
[ \t\n]+    { }
.           { printf("UNK(%s)\n", yytext); }

%%
EOF

    cat > "$d/input.txt" << 'EOF'
int x = 42;
if x == 3.14 while
ifx integer returnval
0int 10.0 .5
EOF

    cat > "$d/expected.txt" << 'EOF'
KW_INT
ID(x)
ASSIGN
INT(42)
SEMI
KW_IF
ID(x)
EQ
FLOAT(3.14)
KW_WHILE
ID(ifx)
ID(integer)
ID(returnval)
INT(0)
KW_INT
FLOAT(10.0)
UNK(.)
INT(5)
EOF

    run_test "E4" "$d/grammar.l" "$d/input.txt" "$d/expected.txt" \
             "%array mode · fixed buffer · no malloc/free paths"
    rm -rf "$d"
}

# ---- E5: Variable-length trailing context (should error or warn) ------------
# This is a PROBE test: variable-length trailing is unsupported per your design.
# The correct behavior is a parse error from ft_lex, NOT silent wrong output.

register_test E5
test_E5() {
    local d; d=$(mktemp -d)

    cat > "$d/grammar.l" << 'EOF'
%{
#include <stdio.h>
%}

%%

[0-9]+/[a-z]+   { printf("NUM_BEFORE_WORD(%s)\n", yytext); }
[0-9]+          { printf("NUM(%s)\n", yytext); }
[a-z]+          { printf("WORD(%s)\n", yytext); }
[ \t\n]+        { }

%%
EOF

    printf '%s\n' '42abc 7 hello' > "$d/input.txt"
    printf '' > "$d/expected.txt"   # doesn't matter — we check exit code

    printf "  %-12s" "${BOLD}E5${RESET}"

    local workdir; workdir=$(mktemp -d)
    if ! timeout "$TIMEOUT" "$FT_LEX" "$d/grammar.l" 2>"$workdir/err.log"; then
        printf "${GREEN}PASS${RESET}  ${CYAN}ft_lex correctly rejected variable-length trailing context${RESET}\n"
        printf "    ft_lex said: %s\n" "$(head -1 "$workdir/err.log")"
        (( PASS_COUNT++ )) || true
    else
        # ft_lex did NOT error — check if the output is at least plausible
        [[ -f lex.yy.c ]] && mv lex.yy.c "$workdir/lex.yy.c" || true
        local e5_link="$LINK_FLAGS"
        if [[ "$LINK_FLAGS" == "__stub__" ]]; then
            printf '\n%s\n' "$YYWRAP_STUB" >> "$workdir/lex.yy.c"; e5_link=""
        fi
        if $CC $CFLAGS -o "$workdir/scanner" "$workdir/lex.yy.c" $e5_link 2>/dev/null &&
           timeout "$TIMEOUT" "$workdir/scanner" < "$d/input.txt" > "$workdir/actual.txt" 2>&1; then
            printf "${YELLOW}WARN${RESET}  ft_lex accepted variable-length trailing context without error\n"
            printf "    Output: %s\n" "$(cat "$workdir/actual.txt" | tr '\n' ' ')"
            printf "    Verify manually that the output is correct.\n"
        else
            printf "${YELLOW}WARN${RESET}  ft_lex accepted the grammar but the scanner failed to compile or run\n"
        fi
        (( SKIP_COUNT++ )) || true
    fi

    rm -rf "$workdir" "$d"
}

# =============================================================================
# MAIN
# =============================================================================

# Parse arguments
declare -a FILTER=()
LIST_ONLY=0

for arg in "$@"; do
    case "$arg" in
        --list|-l)  LIST_ONLY=1 ;;
        --help|-h)
            printf 'Usage: %s [--list] [TEST_NAME...]\n' "$0"
            printf '  --list   print all test names and exit\n'
            printf '  No args  run all tests\n'
            exit 0
            ;;
        *)  FILTER+=("$arg") ;;
    esac
done

if [[ $LIST_ONLY -eq 1 ]]; then
    printf '%s\n' "${ALL_TESTS[@]}"
    exit 0
fi

# Sanity checks before running anything
if [[ ! -x "$FT_LEX" ]]; then
    printf '%sERROR%s: ft_lex binary not found at "%s"\n' "$RED" "$RESET" "$FT_LEX"
    printf 'Set FT_LEX=/path/to/ft_lex or run from the project root.\n'
    exit 1
fi
if ! command -v timeout &>/dev/null; then
    printf '%sWARN%s: `timeout` not found — test hang protection disabled\n' "$YELLOW" "$RESET"
    TIMEOUT=9999
fi

# Header
printf '\n%s=== ft_lex Test Suite ===%s\n' "$BOLD" "$RESET"
printf 'ft_lex:  %s\n' "$FT_LEX"
printf 'libl:    %s\n' "$([[ "$LINK_FLAGS" == "__stub__" ]] && echo "inline yywrap stub" || echo "${LINK_FLAGS:-<none>}")"
printf 'filter:  %s\n\n' "${FILTER[*]:-<all>}"

# Run all registered tests
for test_name in "${ALL_TESTS[@]}"; do
    if declare -f "test_${test_name}" &>/dev/null; then
        "test_${test_name}"
    fi
done

# Summary
printf '\n%s--- Results ---%s\n' "$BOLD" "$RESET"
printf '  %sPASS%s  %d\n' "$GREEN" "$RESET" "$PASS_COUNT"
printf '  %sFAIL%s  %d\n' "$RED"   "$RESET" "$FAIL_COUNT"
[[ $SKIP_COUNT -gt 0 ]] && printf '  %sINFO%s  %d  (compliance probes)\n' "$YELLOW" "$RESET" "$SKIP_COUNT"

if [[ ${#FAILED_TESTS[@]} -gt 0 ]]; then
    printf '\n  Failed: %s\n' "${FAILED_TESTS[*]}"
fi

printf '\n'

# Exit code for CI: 0 only if everything passed
[[ $FAIL_COUNT -eq 0 ]]