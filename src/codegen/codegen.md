# ft_lex — Code Generation

## DFA → lex.yy.c

> Technical Reference — 42 School Project

---

## 1. Overview

The code generation stage is the final step of the ft_lex pipeline. It takes a fully constructed DFA and a parsed `.l` file, and serializes them into a valid C source file `lex.yy.c` that implements the lexer.

```txt
LexFile + DFA  →  Codegen  →  lex.yy.c
```

Codegen is conceptually a **sophisticated printer** — it does not compute anything new. It translates data structures into valid C syntax.

The entry point:

```cpp
Codegen codegen;
codegen.generate(dfa, lexfile, "lex.yy.c");
```

The generated scanner now supports start conditions (`%s`, `%x`) through a condition-to-DFA-entry indirection used by `BEGIN(NAME)`.
It also supports fixed-length trailing context (`r/s`) by rewinding the consumed lookahead with `yyless(...)` before executing the user action.

---

## 2. Structure of the generated file

The generated `lex.yy.c` is composed of four sections, written in order:

| Section | Source | Content |
| - | - | - |
| Prologue | `LexFile::verbatim_top_` | User `%{ ... %}` block |
| Static tables | `DFA` | Transition table + accepting states table |
| `yylex()` | Template + `LexFile` | DFA simulation loop + user actions |
| Epilogue | `LexFile::verbatim_bottom_` | User code after second `%%` |

---

## 3. Static tables

The DFA is serialized as start-condition metadata plus two static C arrays.

### 3.1 Start conditions metadata

Codegen emits:

- one macro per start condition (`#define INITIAL 0`, `#define COMMENT 1`, ...)
- one lookup table mapping macro index to DFA entry state:

```c
static int yystart_states[] = { dfa_state_for_INITIAL, dfa_state_for_COMMENT, ... };
```

`BEGIN(X)` in the runtime template does:

```c
#define BEGIN(x) (yycurrent_state = yystart_states[(x)])
```

So user code switches condition by selecting the corresponding DFA start state.

### 3.2 Transition table — `yy_table`

A 2D array of dimensions `NB_STATES × 256`. Each cell contains the destination state when consuming a given character, or `-1` when no transition exists.

```c
static int yy_table[NB_STATES][256] = {
    /* state 0 */ { -1, -1, ..., 1, ..., -1 },
    /* state 1 */ { -1, -1, ..., -1, ..., 2 },
    /* ...     */
};
```

**Generation:** for each state `i` from `0` to `NB_STATES - 1`, and for each character value `c` from `0` to `255`, look up `dfa.transitions_[i].find(static_cast<char>(c))`. Write the destination state if found, `-1` otherwise.

**Important:** always cast `c` to `unsigned char` before using it as a map key. On platforms where `char` is signed, values above 127 produce negative indices.

### 3.3 Accepting states table — `yy_accept`

A 1D array of size `NB_STATES`. Each cell contains the rule index if the state is accepting, or `-1` otherwise.

```c
static int yy_accept[NB_STATES] = { -1, -1, 2, -1, 0 };
```

**Generation:** for each state `i`, look up `dfa.final_states_.find(i)`. Write the rule index if found, `-1` otherwise.

**Why rule index and not a boolean:** `yylex()` needs to know not just that a match occurred, but which action to dispatch. Multiple accepting states correspond to different rules — the index is the direct key into the switch dispatch.

### 3.4 Prerequisite: contiguous state numbering

Both tables require that DFA state IDs are contiguous integers from `0` to `N-1`. This is guaranteed by the subset construction algorithm, which assigns IDs sequentially as new states are discovered. No remapping is needed.

---

## 4. The yylex() template

### 4.1 Template approach

Rather than generating `yylex()` character by character via `out_ <<`, the body of `yylex()` is stored in a static template file `yylex_template.c`. This template contains the fixed DFA simulation logic with two substitution markers:

| Marker | Replaced with |
| - | - |
| `@@VERBATIM_RULES@@` | Contents of `LexFile::verbatim_rules_` |
| `@@RULES@@` | Generated `case N: { action } break;` blocks |

The template is embedded in the binary at compile time via `xxd -i`:

```makefile
yylex_template.h: yylex_template.c
    xxd -i $< > $@
```

This produces an `unsigned char yylex_template_c[]` array included directly in `Codegen.cpp`.

### 4.2 Input buffer

`yylex()` maintains an internal growable buffer `yybuffer` to support backtracking without seeking in `yyin` (which may be `stdin`, a non-seekable stream).

Two cursors are maintained separately:

| Variable | Role |
| - | - |
| `yybuf_size` | Number of characters written into the buffer |
| `yybuf_capa` | Allocated capacity (grows geometrically) |
| `yybuf_pos` | Read cursor — where the DFA simulation currently is |

`yyread()` first serves characters from `yybuffer[yybuf_pos]` if available. Only when `yybuf_pos == yybuf_size` does it call `fgetc(yyin)` and append to the buffer.

**Geometric growth:** capacity doubles when full, giving O(n) amortized allocation cost instead of O(n²) with per-character `realloc`.

### 4.3 Longest match with backtracking

`yylex()` implements the POSIX longest match rule: it continues past accepting states until the DFA is blocked, then backtracks to the last accepting position.

```txt
loop:
    state         = yycurrent_state
    last_match    = -1
    last_match_pos = match_start
    yybuf_pos     = match_start

    inner loop:
        c = yyread()
        if EOF:
            if last_match != -1: break to dispatch
            if yywrap() == 1: return 0
            else: continue
        state = yy_table[state][(unsigned char)c]
        if state == -1: break
        if yy_accept[state] != -1:
            last_match     = yy_accept[state]
            last_match_pos = yybuf_pos

    dispatch:
    if last_match == -1:
        ECHO (copy one character to stdout)
        match_start++
        continue outer loop

    yytext = strndup(yybuffer + match_start, last_match_pos - match_start)
    yyleng = last_match_pos - match_start
    yybuf_pos   = last_match_pos
    match_start = last_match_pos

    switch (last_match):
        case 0: { user action 0 } break;
        case 1: { user action 1 } break;
        ...
```

**Backtracking:** when the DFA blocks, `yybuf_pos` is reset to `last_match_pos`. Characters after the match remain in the buffer and will be reconsumed by the next `yylex()` call.

### 4.4 yytext lifetime

`yytext` is a `char*` pointing to a `strndup` copy of the matched token. It is valid until the next call to `yylex()`, which frees it at the top of its body:

```c
int yylex(void) {
    free(yytext);
    yytext = NULL;
    if (!yyinitialized) {
        BEGIN(INITIAL);
        yyinitialized = 1;
    }
    /* ... */
}
```

This bootstrap is required because DFA state `0` is not guaranteed to be the `INITIAL` entry state when multiple start conditions are compiled.

This matches the POSIX specification: callers that need to retain the value across calls must `strdup` it themselves.

### 4.5 Trailing Context Runtime Handling (`r/s`)

For rules with trailing context, the automata side matches `r` followed by `s` as one DFA path. At dispatch time, generated code trims the matched suffix `s` from `yytext` and rewinds the input cursor so the trailing part can be rescanned by subsequent rules.

Generated switch logic (per rule):

```c
case N:
    yyless(yyleng - TRAILING_LEN);
    { user action }
    break;
```

`TRAILING_LEN` comes from `Rule::trailing_length_`, computed during lex file parsing.

The runtime macro in the template updates all relevant cursors consistently:

- `yyleng` becomes `yyleng - TRAILING_LEN`
- `yytext` is truncated at the new length
- `yybuf_pos` and `match_start` are moved back so trailing characters remain unread for the next match

This reproduces lex trailing-context behavior for fixed-length trailing expressions.

---

## 5. Global variables in the generated file

| Variable | Type | Role |
| - | - | - |
| `yyin` | `FILE*` | Input stream (default: `stdin` via libl) |
| `yyout` | `FILE*` | Output stream (default: `stdout` via libl) |
| `yytext` | `char*` | Pointer to the last matched token |
| `yyleng` | `size_t` | Length of the last matched token |
| `yybuffer` | `char*` | Internal input buffer |
| `yycurrent_state` | `int` | Current DFA entry state selected by `BEGIN()` |

For trailing-context rules, `yyless()` mutates `yyleng`, `yytext`, and input cursors before action execution.

`yyin` and `yyout` are initialized by the `libl` runtime, not in `lex.yy.c` itself.

---

## 6. Codegen class interface

```cpp
class Codegen {
public:
    void generate(const DFA& dfa, const LexFile& lexfile, const std::string& path);

private:
    std::ofstream out_;

    void write_prologue(const LexFile& lexfile);   // verbatim_top_
    void write_tables(const DFA& dfa);             // start metadata + yy_table + yy_accept
    void write_yylex(const DFA& dfa, const LexFile& lexfile); // template substitution
    void write_epilogue(const LexFile& lexfile);   // verbatim_bottom_
};
```

`out_` is an `ofstream` member opened in `generate()` and shared across all private methods. It closes automatically at end of scope.
