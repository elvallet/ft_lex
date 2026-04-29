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

- one macro per start condition (`#define INITIAL 0`, `#define COMMENT 1`, ...), excluding BOL variants
- one macro for the total number of conditions: `#define YYNB_CONDITIONS N`
- one lookup table mapping macro index to DFA entry state for **both** normal and BOL variants:

```c
#define INITIAL 0
#define COMMENT 1
#define YYNB_CONDITIONS 2

static int yystart_states[] = {
    dfa_state_for_INITIAL,      // index 0 (INITIAL condition)
    dfa_state_for_COMMENT,      // index 1 (COMMENT condition)
    dfa_state_for_INITIAL_BOL,  // index 2 (INITIAL_BOL variant, auto-added for BOL rules)
    dfa_state_for_COMMENT_BOL   // index 3 (COMMENT_BOL variant)
};
```

`BEGIN(X)` in the runtime template does:

```c
#define BEGIN(x) (yycurrent_state = yystart_states[(x)])
```

So user code switches condition by selecting the corresponding DFA start state. BOL variants are automatically managed by the runtime when a newline is matched.

### 3.2 Transition table — `yytable`

A 2D array of dimensions `NB_STATES × 256`. Each cell contains the destination state when consuming a given character, or `-1` when no transition exists.

```c
static int yytable[NB_STATES][256] = {
    /* state 0 */ { -1, -1, ..., 1, ..., -1 },
    /* state 1 */ { -1, -1, ..., -1, ..., 2 },
    /* ...     */
};
```

**Generation:** for each state `i` from `0` to `NB_STATES - 1`, and for each character value `c` from `0` to `255`, look up `dfa.transitions_[i].find(static_cast<char>(c))`. Write the destination state if found, `-1` otherwise.

**Important:** always cast `c` to `unsigned char` before using it as a map key. On platforms where `char` is signed, values above 127 produce negative indices.

### 3.3 Accepting states tables

The accepting states are stored across three complementary tables to support multiple rules per accepting state:

```c
typedef struct {
    int rule_id;
    int trailing_len;
} YYAcceptEntry;

static YYAcceptEntry yyaccept_data[] = {
    { 0, -1 },   // rule 0, no trailing
    { 2, 3 },    // rule 2, trailing length 3
    { 1, -1 },   // rule 1, no trailing
    // ...
};

static int yyaccept_offset[NB_STATES] = { 0, 1, 3, -1, ... };
static int yyaccept_count[NB_STATES] = { 1, 2, 1, 0, ... };
```

**Purpose:**

- `yyaccept_data[]` is a flat array of all accept entries, sorted by state
- `yyaccept_offset[S]` indexes into `yyaccept_data[]` for state S (or `-1` if non-accepting)
- `yyaccept_count[S]` tells the runtime how many rules to check for state S

**Rule ordering:** Rules within each state are stored in **ascending index order** (index 0 = highest priority). The runtime pushes them onto a stack in reverse order so the highest-priority rule ends up on top.

**Trailing context:** `trailing_len` is set from `Rule::trailing_length_` if the rule has trailing context (non-empty `Rule::trailing_`), or `-1` otherwise. The runtime uses this to call `yyless()` before executing the action.

### 3.4 Prerequisite: contiguous state numbering

All tables require that DFA state IDs are contiguous integers from `0` to `N-1`. This is guaranteed by the subset construction algorithm, which assigns IDs sequentially as new states are discovered. No remapping is needed.

---

## 4. The yylex() template

### 4.1 Template approach

Rather than generating `yylex()` character by character, the body is stored in a static template file `yylex_template.c` with substitution markers for codegen:

| Marker | Replaced with |
| - | - |
| `@@PROLOGUE@@` | User verbatim C code from section 1 (`LexFile::verbatim_top_`) |
| `@@YYTEXT_MODE@@` | `YYARRAY_MODE` or `YYPOINTER_MODE` depending on `LexFile::array_mode_` |
| `@@TABLES@@` | All DFA tables: condition defines, `yytable`, `yyaccept_*` |
| `@@VERBATIM_RULES@@` | Indented verbatim code lines from section 2 |
| `@@RULES@@` | Generated `case N: { action } break;` blocks for each rule |
| `@@EPILOGUE@@` | User verbatim C code from section 3 (`LexFile::verbatim_bottom_`) |

The template is embedded in the binary at compile time via `xxd -i`:

```makefile
yylex_template.h: yylex_template.c
    xxd -i $< > $@
```

This produces an `unsigned char yylex_template_c[]` array (and size `yylex_template_c_len`) included in `Codegen.cpp`.

### 4.2 Input buffer

`yylex()` maintains an internal growable buffer `yybuf` to support backtracking without seeking in `yyin` (which may be `stdin`, a non-seekable stream).

State variables maintained separately:

| Variable | Role |
| - | - |
| `yybuf_size` | Number of characters currently stored in buffer |
| `yybuf_capa` | Allocated capacity (grows geometrically by doubling) |
| `yybuf_pos` | Read cursor — current position during DFA simulation |

**How yyread() works:**

1. If `yybuf_pos < yybuf_size`, return the buffered character at `yybuf[yybuf_pos++]`
2. Otherwise, call `fgetc(yyin)` to read a new character
3. If EOF, return `EOF`
4. Append character to buffer (growing capacity if necessary)
5. Increment `yybuf_pos` and return the character

**Geometric growth:** when `yybuf_size >= yybuf_capa`, capacity doubles (or initializes to `YYBUF_INIT_SIZE` if zero). This gives O(n) amortized allocation cost.

**Trailing context push-back:** When a token with trailing context is matched, `yybuf_pos` is set to just after the committed portion, automatically leaving trailing characters in the buffer for the next token.

### 4.3 Longest match with backtracking and candidate array

`yylex()` implements the POSIX longest match rule using a **candidate accumulation** strategy:

**Scan Phase:**
The DFA is driven forward, reading characters until it blocks (no transition). During the scan, every time an accepting state is encountered, all matching rules for that state are stored as **candidates** in an array `yycandidates[]`.

```c
typedef struct {
    int rule_id;
    size_t match_end;     // absolute position just past the match
    int trailing_len;     // -1 if no trailing context
} YYCandidate;

static YYCandidate yycandidates[YYCAND_MAX];
static int yyncandidates;  // number of collected candidates
```

Rules are pushed into the array in **reverse priority order** (lowest index first), so after collection, the highest-priority matching rule ends up at the top (highest index) of the array.

**Dispatch Phase:**
After the DFA blocks, the candidates are examined from top to bottom (highest priority first):

- For each candidate, `committed_len = match_end - trailing_len` (or full match if no trailing)
- `yytext` and `yyleng` are set to the committed portion
- `yybuf_pos` is set to just after the committed portion, effectively pushing the trailing characters back into the input
- The rule action is executed; if it contains `REJECT`, the loop continues to the next candidate

```txt
Scan phase:
  state = DFA entry
  yyncandidates = 0
  loop:
    c = yyread()
    if EOF or state = -1: break
    state = yytable[state][c]
    if state is accepting:
        for each rule in yyaccept_data[yyaccept_offset[state]]..+yyaccept_count[state]:
            push {rule_id, yybuf_pos, trailing_len} onto yycandidates[] (reverse priority order)

Dispatch phase:
  if yyncandidates == 0:
      ECHO one character
      continue outer loop
  for ci = yyncandidates - 1 down to 0:  // iterate from top (highest priority)
      candidate = yycandidates[ci]
      committed_len = (candidate.trailing_len >= 0) ? 
          (candidate.match_end - match_start - candidate.trailing_len) : 
          (candidate.match_end - match_start)
      yy_assign_yytext(match_start, committed_len)
      yybuf_pos = match_start + committed_len  // push back trailing chars
      switch (candidate.rule_id):
          case 0: { user action 0 } break;
          case 1: { user action 1 } break;
          ...
      if action did not REJECT: break
```

This approach naturally supports backtracking without explicit state management: trailing characters remain in the buffer and will be reconsumed by the next token match.

### 4.4 Initialization and BOL state management

At the start of `yylex()`, the scanner initializes:

```c
static int initialized = 0;
if (!initialized) {
    if (!yyin) yyin = stdin;
    if (!yyout) yyout = stdout;
    BEGIN(INITIAL);
    initialized = 1;
}
```

This ensures `yycurrent_state` is explicitly set, even though multiple start conditions may exist. The `yystart_states[]` array includes DFA entries for all conditions and their BOL variants.

**BOL handling:** after executing a rule action, the scanner updates `yy_at_bol`:

```c
yy_at_bol = (yyleng > 0 && yytext[yyleng - 1] == '\n');
```

At the start of the next token scan, the DFA entry is chosen based on this flag:

```c
int state = yy_at_bol
    ? yystart_states[yycurrent_state + YYNB_CONDITIONS]  // BOL variant
    : yystart_states[yycurrent_state];                   // normal variant
```

When `yy_at_bol` is true, the scanner uses the BOL-specific DFA entry (offset by `YYNB_CONDITIONS`) which routes to rules marked with `^` anchor. After consuming a non-newline character, `yy_at_bol` becomes false, switching to normal rules on the next token.

### 4.5 yytext and yyleng management

**yytext** is allocated based on the `%pointer` or `%array` mode:

- **YYPOINTER_MODE** (`%pointer`, default): `yytext` is a `char*` pointing to heap-allocated memory. On each `yylex()` call, the old pointer is freed and replaced with a new allocation.
- **YYARRAY_MODE** (`%array`): `yytext` is a fixed buffer of size `YYLMAX`. No allocation occurs; characters are copied into it.

**yyleng** stores the length of the matched token (after trailing-context trimming if applicable).

Lifetime rules:

- `yytext` is valid until the next `yylex()` call (it is freed/overwritten)
- Callers needing persistent text must `strdup()` before the next call
- The `yymore()` macro allows appending the next match to the current `yytext` by setting the `yymore_flag`

### 4.6 Trailing Context Runtime Handling (`r/s`)

For rules with trailing context, the automata side matches `r` followed by `s` as one DFA path. At dispatch time, generated code trims the matched suffix `s` from `yytext` and rewinds the input cursor so the trailing part can be rescanned by subsequent rules.

When a candidate is selected for dispatch:

```c
size_t full_len = cand->match_end - match_start;
size_t committed_len = (cand->trailing_len >= 0)
    ? full_len - (size_t)cand->trailing_len
    : full_len;
yy_assign_yytext(match_start, committed_len);
yybuf_pos = match_start + committed_len;  // push back trailing chars
```

**How it works:**

- `cand->match_end` is the position just past the full match (pattern + trailing)
- `cand->trailing_len` is from `yyaccept_data[]` (set to -1 if no trailing)
- `committed_len = full_len - trailing_len` (or full match if trailing_len == -1)
- `yytext` and `yyleng` are set to the committed portion
- `yybuf_pos` is moved back so the trailing characters remain unconsumed

This reproduces lex trailing-context behavior for fixed-length trailing expressions. The runtime does not call `yyless()` explicitly; instead, it manages buffer positions directly.

---

## 5. Global variables in the generated file

| Variable | Type | Role |
| - | - | - |
| `yyin` | `FILE*` | Input stream (default: `stdin` via libl) |
| `yyout` | `FILE*` | Output stream (default: `stdout` via libl) |
| `yytext` | `char*` or `char[]` | Pointer/array containing the last matched token |
| `yyleng` | `size_t` | Length of the last matched token |
| `yybuf` | `char*` | Internal input buffer for lookahead |
| `yycurrent_state` | `int` | Current start condition state (set by `BEGIN()`) |
| `yy_at_bol` | `int` | Flag indicating beginning-of-line (for BOL anchor rules) |

---

## 6. Codegen class interface

```cpp
class Codegen {
public:
    void generate(const automata::DFA& dfa, const lexer_file::LexFile& lexfile, std::ostream& out);

private:
    void write_prologue(const lexer_file::LexFile& lexfile, std::string& tmpl);
    void write_tables(const automata::DFA& dfa, const lexer_file::LexFile& lexfile, std::string& tmpl);
    void write_yylex(const lexer_file::LexFile& lexfile, std::string& tmpl);
    void write_epilogue(const lexer_file::LexFile& lexfile, std::string& tmpl);
};
```

**Generation flow:**

1. Load embedded template `yylex_template.c` as a mutable string
2. Call each `write_*()` function in order to substitute markers:
   - `@@PROLOGUE@@` → verbatim_top_
   - `@@TABLES@@` → all DFA tables and start condition defines
   - `@@VERBATIM_RULES@@` → indented verbatim lines from section 2
   - `@@RULES@@` → switch dispatch cases
   - `@@EPILOGUE@@` → verbatim_bottom_
   - `@@YYTEXT_MODE@@` → `YYARRAY_MODE` or `YYPOINTER_MODE`
3. Write the final substituted template to the output stream
