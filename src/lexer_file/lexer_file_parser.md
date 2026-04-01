# Lexer File Parser

## Overview

The `lexer_file` module is responsible for parsing `.l` files (POSIX lex format) and producing a structured representation of their contents.

---

## The `.l` File Format

A `.l` file is divided into three sections separated by `%%` markers:

```.l
[Section 1 — Definitions]
%%
[Section 2 — Rules]
%%
[Section 3 — User code]
```

**Section 1** contains:

- Verbatim C code blocks enclosed in `%{ ... %}` - copied directly into the generated file
- Macro definitions of the form `NAME regex`

**Section 2** contains lexer rules of the form `pattern action`, where:

- `pattern` is a POSIX ERE regex
- `action` is a block of C code enclosed in `{ ... }`
- `action` can be `|` (pipe), meaning "same action as the next rule"
- Lines beginning with whitespace are verbatim C code injected into `yylex()`

**Section 3** is optional and contains raw C code copied verbatim at the end of the generated file (typically a `main()`).

---

## Data Structures

`LexFile`
Top-level parser output.

| Field | Type | Description |
| - | - | - |
| `verbatim_top_` | `string` | All `%{ %}` blocks from section 1, concatenated |
| `macros_` | `vector<pair<string, string>>` | Macros definitions in declaration order |
| `rules_` | `vector<Rule>` | Lexer rules in declaration order |
| `verbatim_rules_` | `vector<string>` | Indented verbatim lines from section 2 |
| `verbatim_bottom_` | `string` | Entire section 3 |

Macros are stored as `vector` to preserve declaration order, which is semantically significant: a macro may only reference macros declared before it.

`Rule`

| Field | Type | Description |
| - | - | - |
| `pattern_` | `string` | The regex pattern, fully macro-expanded |
| `action_` | `string` | The C action block, including surrounding braces |
| `is_pipe_` | `bool` | True is the original action was a pipe |

## Architecture

`LineReader`

Handles all low-level file I/O. Exposes logical lines (not raw physical lines) to the parser.

**Key responsibilities:**

- Opens the file and throws on failure
- Merges line continuations (`\` at end of line) transparently
- Maintains a one-line lookahead for `peek()` without seeking
- Tracks the current `ParseContext` (filename + line number)

**Interface:**

- `next()` - consumes and returns the next logical line, or `nullopt` at EOF
- `peek()` - returns the next logical line without consuming it
- `context()` - returns the current parse position (reflects last consumed line)

`LexParser`

The main parser class. Instanciated with a filename, produces a `LexFile` via `parse()`.

**Internal pipeline:**

```arch
parse()
├── parse_definitions()       — section 1
│   ├── parse_verbatim_block()
│   └── parse_macro_line()
├── parse_rules()             — section 2
│   ├── parse_single_rule()
│   │   ├── split_pattern_action()
│   │   └── complete_action()
├── parse_user_code()         — section 3
├── expand_macros()           — resolve inter-macro references
└── expand_rules()            — resolve macro references in patterns
```

---

## Pattern/Action Splitting

The separator between pattern and action is whitespace, but whitespace inside `[...]` or `"..."` is part of the pattern.
`split_pattern_action()` maintains a small state machine while scanning the line:

| State | Trigger | Exit |
| - | - | - |
| Normal | - | first unprotected whitespace → end of pattern |
| Bracket | '[' | ']' |
| String | `"` | `"` |
| Escaped | '\' | always single character |

## Multi-line Actions

An action may span multiple physical lines if its braces are not balanced on the first line. `complete_action()` counts brace depth while correctly ignoring braces inside:

| Context | Example |
| C strings | `"{"` |
| C chars | `'{'` |
| Line comments | `// {` |
| Block comments | `/* { */` |

It reads additional lines from the `LineReader` until depth reaches zero, or throws `ParseError` on EOF.

---

## Macro Expansion

Macro expansion is a two-pass post-processing step that runs after all parsing is complete.

Pass 1 - `expand_macros()`: Resolves inter-macro references. Iterates macros in declaration order; for macro `i`, substitutes all references to macros `0..i-1` in its regex. This guarantees that by the time a macro is used as a substitution source, it is already fully resolved.

Pass 2 - `expand_rules()`: Substitutes all macro references in rule patterns. Each `{NAME}` is replaced by `(value)` - the parentheses are mandatory to preserve operator precedence (e.g. `{ALNUM}+` must expand to `([0-9]|[a-zA-Z])+`, not `[0-9]|[a-zA-Z]+`).

After expansion, any remaining `{NAME}` where `Name` starts with a letter or `_` is treated as an undefined macro reference and raises a `ParseError`. Quantifiers like `{2,4}` (starting with a digit) are not affected.

---

## Error Handling

All errors are reported via `ParseError`, which carries:

- The source filename
- The line number
- The column number  (when available)

Error messages follow the format: `filename:line:col message`

Detected errors include:

| Error | Message |
| - | - |
| File not found | `Fatal error: Could not open file: ...` |
| Unclosed `%{` | `Unclosed verbatim block '%{'` |
| Unsupported directive | `Unsupported directive` |
| Invalid macro name | `Invalid character in macro` |
| Empty macro regex | `Invalid macro: regex cannot be empty` |
| Unclosed action `{` | `Unclosed action block` |
| Pipe as last rule | `Last rule cannot be pipe` |
| Undefined macro | `Undefined macro in pattern` |
