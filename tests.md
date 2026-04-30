# ft_lex — Test Battery (Integration & Stress)

Tests are grouped into four categories, ordered by complexity.  
Each test specifies: the `.l` grammar, the raw input, the expected output, and the specific failure modes it hunts for.

---

## Category A — Integration (mix of basic features)

### A1 · Macro tokenizer — keyword vs identifier vs number

**Features exercised:** macros, longest-match, multi-char operators, POSIX character classes.

```lex
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
```

**Input:**

```txt
int x = 42;
if x == 3.14 while
ifx integer returnval
0int 10.0 .5
```

**Expected output:**

```txt
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
```

**What this hunts:**

| Input fragment | What could go wrong |
| - | - |
| `integer` | Tokenized as `KW_INT` + `ID(eger)` → longest-match failure |
| `ifx` | Tokenized as `KW_IF` + `ID(x)` → same |
| `0int` | `INT(0)` then `KW_INT` — macro boundary at a digit/letter transition |
| `10.0` | Matched as `INT(10)` + `UNK(.)` + `INT(0)` if FLOAT macro expansion is broken |
| `.5` | FLOAT requires a leading digit; `.` must be caught by the wildcard fallback |
| `==` | Two `ASSIGN` instead of one `EQ` → longest-match failure on operators |

---

### A2 · Operator tokenizer — longest-match stress

**Features exercised:** longest-match with many shared prefixes, no regexes, pure literals.

```lex
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
```

**Input:** (one operator per token, all on one line, then ambiguous sequences)

```txt
>>= <<= >> << >= <= != == = ++ -- += -= + - > <
>>=<<=
```

**Expected output:**

```txt
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
```

**What this hunts:** `>>=` must not be parsed as `>>` + `=`, nor as `>=` + `>`. The DFA must commit to the longest match at every branch point. The concatenated `>>=<<=` on line 2 checks that `match_start` advances correctly between adjacent ambiguous tokens.

---

### A3 · Pipe rules + keyword grouping

**Features exercised:** pipe `|` rule chaining, first-match semantics, macro + literal mix.

```lex
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
```

**Input:**

```txt
cat horse bird blue red green purple dog fish cataract catdog
```

**Expected output:**

```txt
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
ANIMAL(cat)
ANIMAL(dog)
```

**What this hunts:**

| Input | Risk |
| - | - |
| `cataract` | Pipe chain resolves to ANIMAL(cat) + OTHER(aract) if longest-match breaks |
| `catdog` | Must produce ANIMAL(cat) + ANIMAL(dog), not OTHER(catdog) if literal matching fails |
| All pipe groups | Any pipe rule that accidentally executes ECHO instead of the shared action |

---

## Category B — Start conditions

### B1 · C-string parser — `%x` + `yymore()` + `yylineno`

**Features exercised:** exclusive condition transition, yymore() accumulation across a state boundary, yylineno tracking, escape handling.

```lex
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
```

**Input** (lines as-is, `\` is a literal backslash in the file):

```txt
foo "hello" bar
baz "with \"quotes\"" qux
err "unterminated
next
```

**Expected output:**

```txt
ID(foo)
STRING("hello") len=7
ID(bar)
ID(baz)
STRING("with \"quotes\"") len=17
ID(qux)
ID(err)
ERROR: unterminated string at line 4
ID(next)
```

> **Design note — yylineno off-by-one:**  
> The runtime increments `yylineno` *before* executing the action (inside the match loop).  
> When the `\n` rule fires for the unterminated string, the newline has already been counted.  
> The real `lex` reports line 3 (where the string started); your runtime will report line 4.  
> This is expected given the current template architecture — flag it as a known deviation.

**What this hunts:**

- `yymore_len` not preserved when `BEGIN()` changes state  
- `yytext` double-freed or not NUL-terminated after many `yymore()` calls  
- Escape sequence `\\[\"\\ntr]` matching two characters as a unit (vs. matching `\\` alone + `"`)  
- `<STR>.` not firing for space/digit characters inside the string

---

### B2 · Inclusive `%s` condition

**Features exercised:** inclusive condition, rules with no condition tag active in all non-exclusive states.

```lex
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
```

**Input:**

```txt
foo { bar 42 baz } qux { 7 } end
```

**Expected output:**

```txt
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
```

**What this hunts:**  
`[a-z]+` has no condition tag → must fire in both `INITIAL` and `IN_BLOCK`.  
If the implementation treats no-tag as `<INITIAL>` only (which would be wrong for `%s`), `bar` and `baz` would be echoed instead of matched.

---

### B3 · Multi-state machine — mini HTML-like tokenizer

**Features exercised:** three distinct states, state transitions chained, attribute patterns, complex patterns per state.

```lex
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
```

**Input:**

```txt
<div class="foo" id="bar">
<img src="pic.png"/>
hello world
<span>text</span>
```

**Expected output:**

```txt
OPEN(div class="foo" id="bar")
OPEN(img src="pic.png"/)
TEXT(hello world)
OPEN(span)
TEXT(text)
OPEN(span)
```

> Note: `</span>` is parsed as `<` → OPEN, then `/span` does not match `[a-zA-Z][a-zA-Z0-9]*` (starts with `/`).  
> `/` is not matched by any `<ATTR>` rule except `"/>"`. Add a `<TAG>"/"[a-z]+` rule in a real implementation.  
> For this test, the interesting part is the three-state machine's transitions, not HTML compliance.

**What this hunts:**

- State not properly restored after `BEGIN(INITIAL)`  
- `<ATTR>[ \t]+` swallowing input and leaving the DFA stuck  
- `[^<\n]+` matching across state boundary if condition filtering is wrong

---

## Category C — Anchors

### C1 · BOL `^` and EOL `$` anchors — isolation test

**Features exercised:** `^` dual DFA entry points, `$` as trailing `\n` context, interaction with `[ \t]+`.

```lex
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
```

**Input:**

```txt
# comment here
start: foo bar
baz
not a label
```

**Expected output:**

```txt
COMMENT(# comment here)
LABEL(start:)
WORD(foo)
EOL_WORD(bar)
EOL_WORD(baz)
WORD(not)
WORD(a)
EOL_WORD(label)
```

**What this hunts:**

| Case | Risk |
| - | - |
| `# comment here` at BOL | `^` entry state not selected → matched by wildcard `.` per character |
| `start:` at BOL | `start` alone (5 chars) vs `start:` (6 chars) — BOL-rule must win longest match |
| `bar` at EOL | `[a-z]+$` vs `[a-z]+` — both accept at same length; first-declared must win |
| `baz` — BOL *and* EOL | `^[a-z]+":"` does not match (no colon); `[a-z]+$` must fire |
| `not` at BOL | `^[a-z]+":"` fails (no colon after `not a`); must fall through to `WORD` |
| `yy_at_bol` reset | After matching non-newline content on a line, BOL must be cleared |

---

### C2 · BOL `^` inside a start condition

**Features exercised:** per-condition BOL entry state (`COND_BOL` dual entry point), interaction of `^` with `BEGIN()`.

```lex
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
```

**Input:**

```txt
hello world
{ foo
bar }
baz
```

**Expected output:**

```txt
BOL(hello)
WORD(world)
OPEN
WORD_BLOCK(foo)
BOL_BLOCK(bar)
CLOSE
BOL(baz)
```

**Rationale:**

- `hello` is at BOL of line 1, INITIAL → `BOL(hello)`  
- After `{`, `yy_at_bol` is 0 → `foo` is not at BOL → `WORD_BLOCK`  
- After the `\n` at end of line 2, `yy_at_bol = 1`; we're in `IN_BLOCK` → `<IN_BLOCK>^[a-z]+` fires → `BOL_BLOCK(bar)`  
- After `}`, INITIAL, `\n` sets BOL → `baz` → `BOL(baz)`

**What this hunts:** The codegen must produce a `_BOL` variant entry state for each condition that has BOL rules. If `IN_BLOCK_BOL` is missing, `bar` is tokenized by `WORD_BLOCK` instead of `BOL_BLOCK`.

---

## Category D — Special actions

### D1 · `yyless()` — token splitting

**Features exercised:** `yyless()` rewinds `match_start` and truncates `yytext`; the put-back characters must be re-scanned correctly.

```lex
%{
#include <stdio.h>
%}

%%

[a-z]+"!"   { yyless(yyleng - 1); printf("EMPHATIC(%s)\n", yytext); }
[a-z]+      { printf("WORD(%s)\n", yytext); }
"!"         { printf("BANG\n"); }
[ \t\n]+    { }

%%
```

**Input:**

```txt
hello! world foo! bar!!
```

**Expected output:**

```txt
EMPHATIC(hello)
BANG
WORD(world)
EMPHATIC(foo)
BANG
EMPHATIC(bar)
BANG
BANG
```

**Step-by-step for `bar!!`:**

1. `bar!!` — `[a-z]+"!"` matches `bar!` (4 chars), `yyless(3)` → yytext=`bar`, prints `EMPHATIC(bar)`  
2. `!` is rescanned → `BANG`  
3. `!` again → `BANG`

**What this hunts:**

- `match_start` after `yyless` is `old_match_start + yyless_n`, not `old_match_start + match_len` → critical for correct re-scan  
- `yyless()` in `%array` mode: `yytext[n] = '\0'` must not overflow the fixed buffer  
- `yyleng` must be updated to `n` so the action can use it correctly after `yyless`  
- `yy_at_bol` must not be incorrectly set after `yyless` (the truncated text may not end in `\n`)

---

### D2 · `yymore()` — accumulation across state boundary

**Features exercised:** `yymore()` with `BEGIN()`, `yymore_len` maintained across the call boundary.

```lex
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
```

**Input:**

```txt
width=100 color=red height=42
```

**Expected output:**

```txt
KEY(width)
ATTR(width=100)
KEY(color)
ATTR(color=red)
KEY(height)
ATTR(height=42)
```

**Mechanism:** after `width=` fires, `yymore()` sets the flag. On the next call, `match_start` stays at `w`, `yymore_len = 6`. The `IN_ATTR` rule matches `100` starting from `w` but appends at position 6 → final `yytext = "width=100"`.

**What this hunts:**

- `yymore_len` reset to 0 when `BEGIN()` changes `yycurrent_state` (the flag and len must survive state transitions)  
- `yymore_flag` consumed too early (before the outer loop re-enters)  
- `yy_assign_yytext()` `memcpy` offset wrong: copies `len` bytes starting at `start`, but `start` must still point to the beginning of the original match, not the beginning of the `IN_ATTR` sub-match

---

### D3 · `REJECT` — stack behavior and POSIX compliance gap

**Features exercised:** REJECT stack, rule priority, comparison with POSIX expected behavior.

```lex
%{
#include <stdio.h>
%}

%%

"hello"     { printf("EXACT\n"); REJECT; }
[a-z]{5}    { printf("FIVE(%s)\n", yytext); REJECT; }
[a-z]+      { printf("WORD(%s)\n", yytext); }
[ \t\n]+    { }

%%
```

**Input:** `hello hi`

**POSIX expected behavior** (re-runs DFA without rejected rule):

```txt
EXACT
FIVE(hello)
WORD(hello)
WORD(hi)
```

**Observed behavior with stack-based REJECT** (pops shorter lengths):

```txt
EXACT           ← (rule0, len=5) popped
WORD(hell)      ← (rule2, len=4) — best at len 4 is [a-z]+, NOT [a-z]{5} (which needs exactly 5)
WORD(hel)       ← (rule2, len=3)
WORD(he)        ← (rule2, len=2)
WORD(h)         ← (rule2, len=1)
                ← stack exhausted → echo 'h'? No: last REJECT with no more stack → echo, advance
WORD(hi)
```

> **This test deliberately reveals the compliance gap.**  
> True POSIX REJECT re-runs the automaton excluding the rejected rule for the *same* input position.  
> The stack-based implementation gives *shorter* matches, not *alternative* rules at full length.  
> Document this as a known limitation if you do not implement full re-run semantics.

---

## Category E — Stress and edge cases

### E1 · Buffer growth — very long token

**Features exercised:** `yyread()` / `realloc()` growth path for tokens larger than `YYBUF_INIT_SIZE` (256).

```lex
%{
#include <stdio.h>
%}

%%

[a-z]+  { printf("LEN=%zu\n", yyleng); }
\n      { }

%%
```

**Input:** programmatically generate one line of 10 000 `a` characters followed by `\n`.

```sh
python3 -c "print('a' * 10000)" | ./your_scanner
# or:
printf '%10000s\n' | tr ' ' 'a' | ./your_scanner
```

**Expected output:**

```txt
LEN=10000
```

**What this hunts:**

- `realloc` reallocates `yybuf` but `match_start` and `yybuf_pos` remain valid (they are byte offsets, not pointers — should be fine)  
- The NUL sentinel `yybuf[yybuf_size] = '\0'` is maintained after growth  
- `yy_assign_yytext` `memcpy(buf + yymore_len, yybuf + start, len)` correct length for 10 000 bytes

Run also with `valgrind --leak-check=full` to catch heap errors.

---

### E2 · EOF inside an exclusive state — graceful exit

**Features exercised:** `yywrap()` called mid-state, no crash, no infinite loop.

```lex
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
```

**Input:** `foo "unterminated` ← no closing quote, no trailing newline

**Expected output:**

```txt
WORD(foo)
```

Then: clean exit with return value 0. No output for the partial string (it is silently dropped).

**What this hunts:**

- The inner `while (1)` loop hits EOF while in `STR`. The condition `if (last_match != -1) break;` must not trigger (no accepting state was reached in `STR` from this position). It falls through to `yywrap()` → return 0.  
- No use-after-free on the partially accumulated `yytext`  
- No hang caused by `yywrap()` returning 0 (which would mean "more input available" and could cause an infinite loop reading EOF forever)

---

### E3 · Trailing context — interaction with overlapping patterns

**Features exercised:** trailing context peeks without consuming, interaction with non-trailing alternatives of different lengths.

```lex
%{
#include <stdio.h>
%}

DIGIT [0-9]

%%

{DIGIT}+/\.{DIGIT}+     { printf("INT_PART(%s)\n", yytext); }
{DIGIT}+                { printf("INT(%s)\n", yytext); }
\.{DIGIT}+              { printf("FRAC(%s)\n", yytext); }
[ \t\n]+                { }
.                       { printf("ECHO(%s)\n", yytext); }

%%
```

**Input:**

```txt
42 3.14 100. .5 0.0
```

**Expected output:**

```txt
INT(42)
INT_PART(3)
FRAC(.14)
INT(100)
ECHO(.)
FRAC(.5)
INT_PART(0)
FRAC(.0)
```

**Case-by-case analysis:**

| Input | Explanation |
| - | - |
| `42` | No `\.digit` follows → trailing rule fails → `INT(42)` |
| `3.14` | `3` / `.14` — trailing matches → `INT_PART(3)`. Then `.14` rescanned → `FRAC(.14)` |
| `100.` | `.` followed by space, not a digit → trailing rule fails → `INT(100)`. Then `.` → `ECHO(.)` |
| `.5` | Doesn't match `{DIGIT}+` → `FRAC(.5)` directly |
| `0.0` | `0` / `.0` — trailing matches → `INT_PART(0)` → `FRAC(.0)` |

**What this hunts:**

- Trailing length computed as the fixed length of `\.{DIGIT}+` — but `\.{DIGIT}+` is variable-length (1+ digits). Your implementation only supports *fixed-length* trailing context. For `3.14`, the trailing context is `.1` then you'd need to push back 2 chars; for `3.1415` it's `.1415` → 5 chars. If the implementation computes trailing length as `1` (just the `.`), the results will be wrong for multi-digit fractions.  
- This test will reveal whether variable-length trailing context is silently broken or properly rejected.

---

### E4 · `%array` mode — fixed buffer, no heap

**Features exercised:** `%array` declaration, `YYLMAX` sentinel, `yyless()` in array mode.

Take grammar A1 and add `%array` to the definitions section:

```lex
%{
#include <stdio.h>
%}
%array

DIGIT   [0-9]
...
%%
...
```

Run the same input as A1. **Expected output is identical.**

**What this hunts:**

- `yy_assign_yytext` path for `YYARRAY_MODE`: `memcpy(yytext + yymore_len, yybuf + start, ...)` instead of `malloc`  
- `yyless()`: `yytext[yyless_n] = '\0'` is safe (array is static, size `YYLMAX = 8192`)  
- No `free(yytext)` calls in array mode (the `#if !defined(YYARRAY_MODE)` guards)  
- Token longer than `YYLMAX`: if E1 is run in `%array` mode with 10 000 chars, `yy_assign_yytext` clips at `YYLMAX - 1 = 8191`. The output should be `LEN=8191`, not a crash.

---

## Summary table

| Test | Features combined | Primary failure mode hunted |
| - | - | - |
| A1 | Macros, longest-match, literals vs patterns | Keyword/identifier prefix collision |
| A2 | Shared-prefix operators, longest-match | Wrong operator split at shared prefix |
| A3 | Pipe rules, first-match, literal vs macro | Pipe inheriting wrong action; `cataract` split |
| B1 | `%x`, `yymore()`, `yylineno`, escapes | `yymore_len` lost on state transition |
| B2 | `%s` inclusive, no-tag rules | No-tag rules silently inactive in inclusive state |
| B3 | Three states, chained `BEGIN()` | State not restored; DFA stuck after ATTR |
| C1 | `^`, `$`, both in same grammar | `yy_at_bol` not cleared; `$` consuming `\n` |
| C2 | `^` inside `%x` condition | Missing `COND_BOL` start state for the condition |
| D1 | `yyless()`, re-scan, `%array` compat | `match_start` not rewound; sentinel not placed |
| D2 | `yymore()` across `BEGIN()` boundary | `yymore_len` reset on state change |
| D3 | REJECT, rule priority stack | Compliance gap: shorter vs alternative |
| E1 | Buffer growth, long token | `realloc` breaks offsets; sentinel lost |
| E2 | EOF in `%x` state | Hang or crash on incomplete token at EOF |
| E3 | Trailing context, variable-length | Wrong trailing length for multi-char suffix |
| E4 | `%array` + `yyless()` + `yymore()` | Array mode guards missing; YYLMAX truncation |
