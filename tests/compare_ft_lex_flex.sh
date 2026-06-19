#!/usr/bin/env bash
# =============================================================================
# ft_lex — ft_lex vs flex output comparison
# Usage:
#   ./compare_ft_lex_flex.sh            → run all comparison cases
#   ./compare_ft_lex_flex.sh A1 B2      → run only selected cases
#   ./compare_ft_lex_flex.sh --list     → list available cases
#
# Extra verbosity:
#   SHOW_OUTPUTS=1 ./compare_ft_lex_flex.sh   → print both scanner outputs
# =============================================================================

set -euo pipefail

FT_LEX="${FT_LEX:-./ft_lex}"
FLEX="${FLEX:-flex}"
CC="${CC:-cc}"
CFLAGS="${CFLAGS:--Wall -Wextra}"
TIMEOUT="${TIMEOUT:-10}"
SHOW_OUTPUTS="${SHOW_OUTPUTS:-0}"

if [[ ! -x "$FT_LEX" ]]; then
    printf 'ERROR: ft_lex binary not found at "%s"\n' "$FT_LEX"
    printf 'Set FT_LEX=/path/to/ft_lex or run from the project root.\n'
    exit 1
fi
FT_LEX="$(realpath "$FT_LEX")"

if ! command -v "$FLEX" &>/dev/null; then
    printf 'ERROR: flex not found (looked for "%s")\n' "$FLEX"
    printf 'Install flex or set FLEX=/path/to/flex.\n'
    exit 1
fi
FLEX="$(command -v "$FLEX")"

if ! command -v timeout &>/dev/null; then
    printf 'WARN: timeout not found, test hang protection disabled\n'
    TIMEOUT=9999
fi

if tput colors &>/dev/null && [[ $(tput colors) -ge 8 ]]; then
    RED=$(tput setaf 1); GREEN=$(tput setaf 2); YELLOW=$(tput setaf 3)
    CYAN=$(tput setaf 6); BOLD=$(tput bold); RESET=$(tput sgr0)
else
    RED=""; GREEN=""; YELLOW=""; CYAN=""; BOLD=""; RESET=""
fi

declare -a FILTER=()
declare -a CASES=(A1 A2 A3 B1 B2)
declare -a FAILED_CASES=()
declare -a TMP_DIRS=()
PASS_COUNT=0
FAIL_COUNT=0

cleanup() {
    for dir in "${TMP_DIRS[@]:-}"; do
        rm -rf "$dir"
    done
}
trap cleanup EXIT

in_array() {
    local needle="$1"
    shift
    local item
    for item in "$@"; do
        [[ "$item" == "$needle" ]] && return 0
    done
    return 1
}

print_section() {
    local title="$1"
    local file="$2"
    local max_lines="${3:-40}"

    printf '    %s%s%s\n' "$BOLD" "$title" "$RESET"
    if [[ ! -f "$file" ]]; then
        printf '      <missing>\n'
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
        printf '      %s[...%d more lines]%s\n' "$YELLOW" "$((lines - max_lines))" "$RESET"
    fi
}

pass_case() {
    local name="$1"
    local description="$2"
    printf '  %-12s%sPASS%s  %s\n' "$name" "$GREEN" "$RESET" "$description"
    (( PASS_COUNT++ )) || true
}

fail_case() {
    local name="$1"
    local reason="$2"
    local details_file="$3"
    local ft_out="$4"
    local flex_out="$5"

    printf '  %-12s%sFAIL%s  %s\n' "$name" "$RED" "$RESET" "$reason"
    [[ -n "$details_file" ]] && print_section "DETAILS" "$details_file"
    [[ -n "$ft_out" && -n "$flex_out" ]] && {
        print_section "FT_LEX" "$ft_out"
        print_section "FLEX" "$flex_out"
    }
    FAILED_CASES+=("$name")
    (( FAIL_COUNT++ )) || true
}

write_stub() {
    local dir="$1"
    cat > "$dir/yywrap.c" << 'EOF'
int yywrap(void) { return 1; }
EOF

    cat > "$dir/main.c" << 'EOF'
int yylex(void);

int main(void) {
    return yylex();
}
EOF
}

build_ft_lex() {
    local dir="$1"
    local log="$dir/ft_lex.generate.log"
    local source="$dir/ft_lex.lex.yy.c"

    if ! (cd "$dir" && timeout "$TIMEOUT" "$FT_LEX" grammar.l >"$log" 2>&1); then
        return 1
    fi
    [[ -f "$dir/lex.yy.c" ]] || return 1
    mv "$dir/lex.yy.c" "$source"

    if ! "$CC" $CFLAGS -o "$dir/ft_lex.scanner" "$source" "$dir/main.c" "$dir/yywrap.c" >"$dir/ft_lex.compile.log" 2>&1; then
        return 2
    fi
}

build_flex() {
    local dir="$1"
    local log="$dir/flex.generate.log"
    local source="$dir/flex.lex.yy.c"

    if ! (cd "$dir" && timeout "$TIMEOUT" "$FLEX" -o flex.yy.c grammar.l >"$log" 2>&1); then
        return 1
    fi
    [[ -f "$dir/flex.yy.c" ]] || return 1
    mv "$dir/flex.yy.c" "$source"

    if ! "$CC" $CFLAGS -o "$dir/flex.scanner" "$source" "$dir/main.c" "$dir/yywrap.c" >"$dir/flex.compile.log" 2>&1; then
        return 2
    fi
}

run_scanner() {
    local binary="$1"
    local input="$2"
    local output="$3"

    if timeout "$TIMEOUT" "$binary" < "$input" > "$output" 2>&1; then
        return 0
    fi

    return $?
}

run_case() {
    local name="$1"
    local description="$2"
    local setup_fn="$3"

    if [[ ${#FILTER[@]} -gt 0 ]] && ! in_array "$name" "${FILTER[@]}"; then
        return
    fi

    local dir
    dir=$(mktemp -d "/tmp/ft_lex_flex_${name}_XXXX")
    TMP_DIRS+=("$dir")
    write_stub "$dir"
    "$setup_fn" "$dir"

    local ft_status
    local flex_status

    if build_ft_lex "$dir"; then
        ft_status=0
    else
        ft_status=$?
    fi
    if (( ft_status != 0 )); then
        case "$ft_status" in
            1) fail_case "$name" "ft_lex generation failed" "$dir/ft_lex.generate.log" "" "" ;;
            2) fail_case "$name" "ft_lex compilation failed" "$dir/ft_lex.compile.log" "" "" ;;
            *) fail_case "$name" "ft_lex build failed" "$dir/ft_lex.generate.log" "" "" ;;
        esac
        return
    fi

    if build_flex "$dir"; then
        flex_status=0
    else
        flex_status=$?
    fi
    if (( flex_status != 0 )); then
        case "$flex_status" in
            1) fail_case "$name" "flex generation failed" "$dir/flex.generate.log" "" "" ;;
            2) fail_case "$name" "flex compilation failed" "$dir/flex.compile.log" "" "" ;;
            *) fail_case "$name" "flex build failed" "$dir/flex.generate.log" "" "" ;;
        esac
        return
    fi

    if ! run_scanner "$dir/ft_lex.scanner" "$dir/input.txt" "$dir/ft_lex.out"; then
        fail_case "$name" "ft_lex scanner failed" "$dir/ft_lex.out" "$dir/ft_lex.out" "$dir/flex.out"
        return
    fi

    if ! run_scanner "$dir/flex.scanner" "$dir/input.txt" "$dir/flex.out"; then
        fail_case "$name" "flex scanner failed" "$dir/flex.out" "$dir/ft_lex.out" "$dir/flex.out"
        return
    fi

    if diff -u "$dir/ft_lex.out" "$dir/flex.out" > "$dir/diff.txt" 2>&1; then
        pass_case "$name" "$description"
        if [[ "$SHOW_OUTPUTS" == "1" ]]; then
            print_section "FT_LEX OUTPUT" "$dir/ft_lex.out"
            print_section "FLEX OUTPUT" "$dir/flex.out"
        fi
    else
        fail_case "$name" "output mismatch" "$dir/diff.txt" "$dir/ft_lex.out" "$dir/flex.out"
    fi
}

case_A1() {
    local dir="$1"
    cat > "$dir/grammar.l" << 'EOF'
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

    cat > "$dir/input.txt" << 'EOF'
int x = 42;
if x == 3.14 while
ifx integer returnval
0int 10.0 .5
EOF
}

case_A2() {
    local dir="$1"
    cat > "$dir/grammar.l" << 'EOF'
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

    printf '%s\n%s\n' \
        '>>= <<= >> << >= <= != == = ++ -- += -= + - > <' \
        '>>=<<=' \
        > "$dir/input.txt"
}

case_A3() {
    local dir="$1"
    cat > "$dir/grammar.l" << 'EOF'
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

    printf '%s\n' 'cat horse bird blue red green purple dog fish cataract catdog' > "$dir/input.txt"
}

case_B1() {
    local dir="$1"
    cat > "$dir/grammar.l" << 'EOF'
%{
#include <stdio.h>
%}

%x STR

%%

\"                  { BEGIN(STR); yymore(); }
<STR>\\[\"\\ntr]    { yymore(); }
<STR>\n             { printf("ERROR: unterminated string\n");
                      BEGIN(INITIAL); }
<STR>\"             { printf("STRING(%s) len=%zu\n", yytext, yyleng);
                      BEGIN(INITIAL); }
<STR>.              { yymore(); }

[a-zA-Z_][a-zA-Z0-9_]*  { printf("ID(%s)\n", yytext); }
[0-9]+                   { printf("NUM(%s)\n", yytext); }
[ \t\n]+                 { }

%%
EOF

    cat > "$dir/input.txt" << 'EOF'
foo "hello" bar
baz "with \"quotes\"" qux
err "unterminated
next
EOF
}

case_B2() {
    local dir="$1"
    cat > "$dir/grammar.l" << 'EOF'
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

    printf '%s\n' 'foo { bar 42 baz } qux { 7 } end' > "$dir/input.txt"
}

for arg in "$@"; do
    case "$arg" in
        --list|-l)
            printf '%s\n' "${CASES[@]}"
            exit 0
            ;;
        --help|-h)
            printf 'Usage: %s [--list] [CASE...]\n' "$0"
            printf '  --list   print all case names and exit\n'
            printf '  No args  run all comparison cases\n'
            exit 0
            ;;
        *)
            FILTER+=("$arg")
            ;;
    esac
done

printf '\n%s=== ft_lex vs flex comparison ===%s\n' "$BOLD" "$RESET"
printf 'ft_lex:  %s\n' "$FT_LEX"
printf 'flex:    %s\n' "$FLEX"
printf 'filter:  %s\n\n' "${FILTER[*]:-<all>}"

run_case A1 'keywords, identifiers, floats, longest-match' case_A1
run_case A2 'shared operator prefixes' case_A2
run_case A3 'pipe rules and literal grouping' case_A3
run_case B1 'exclusive start condition with yymore()' case_B1
run_case B2 'inclusive start condition' case_B2

printf '\n%s--- Results ---%s\n' "$BOLD" "$RESET"
printf '  %sPASS%s  %d\n' "$GREEN" "$RESET" "$PASS_COUNT"
printf '  %sFAIL%s  %d\n' "$RED" "$RESET" "$FAIL_COUNT"

if [[ ${#FAILED_CASES[@]} -gt 0 ]]; then
    printf '\n  Failed: %s\n' "${FAILED_CASES[*]}"
fi

printf '\n'
[[ $FAIL_COUNT -eq 0 ]]