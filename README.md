# ft_lex

A complete reimplementation of the POSIX `lex` lexical analyzer generator, built as a 42 School project. Takes a `.l` specification file as input and produces a `lex.yy.c` C scanner, comptable with the POSIX.1-2024 standard.

---

## Features

- Full POSIX ERE regex support: character classes `[a-z]`, `[^...]`, POSIX classes `[:alpha:]`, wildcard `.`, alternation, grouping, counted repetitions `{n}` / `{n, m}` / `{n,}`
- Macro definitions with inter-macro references
- Start conditions: inclusive `%s` and exclusive `%x`
- BOL anchor `^` with per-condition dual DFA entry points
- EOL anchor `$` via trailing context
- Fixed-length trailing context `r/s`
- `yytext` in pointer mode (default, heap-allocated) or array mode (`%array`)
- Runtime macros: `BEGIN()`, `REJECT`, `yymore()`, `yyless()`, `ECHO`
- Pipe rules (`|` as action)
- Verbatim C blocks (`%{ %}`, indented lines in rules sections, user code section)
- Optional DFA table compression (`-c` flag)
- Companion `libl` static library providing `yywarp()`, `yyout`, and a default `main()`

---

## Build

**Requirements:** a C++17 compiler, `make`, and `xxd` (fallback to `od` if absent).

```sh
make            # builds ft_lex and libl/libl.a
make clean      # removes object files
make fclean     # removes object files and binaries
make re         # fclean + all
```

The build produces two independent artefacts:

- `ft_lex` - the lexer generator tool
- `libl/libl.a` - the companion runtime library for generated scanners

---

## Usage

### Generating a scanner

```sh
./ft_lex input.l      # produces lex.yy.c
./ft_lex input.l -c   # produces lex.yy.c with compressed DFA tables
```

### Compiling the generated scanner

```sh
cc lex.yy.c -o scanner -L libl/ -ll
```

---

## Architecture

The pipeline transforms a `.l` file into a C scanner through five sequential stages:

```txt
.l file
   └─ LexParser                                    - tokenizes sections, expands macros, extracts rules
        └─ Thompson                                - build one NFA fragment per rule (tagged with rule index)
            └─ ParsingPipeline                     - groups by start condition, merges into one NFA
                    └─ SubsetConstruction          - determinizes into a DFA, assigns start states
                            └─ Codegen             - serializes DFA + actions into lex.yy.c
```

Each stage is documented in detail under `/docs`.

---

## Known Limitations

**Variable-length trailing context.** The `r/s` operator is supported only when the trailing part `s` has a fixed, statically computable length. Patterns like `foo/bar+` (variable-length trailing) are rejected at parse time with an explicit error.

---

## Documentation

| Document | Contents |
| - | - |
| [`docs/lexer_file_parser.md`](docs/lexer_file_parser.md) | `.l` file format, `LexParser` internals, macro expansion, condition parsing |
| [`docs/automata.md`](docs/automata.md) | NFA/DFA data models, Thompson construction, subset construction, BOL handling |
| [`docs/codegen.md`](docs/codegen.md) | Generated file structure, DFA tables, `yylex()` template, compression mode |
| [`docs/tests.md`](docs/tests.md) | Integration and stress test battery (categories A-E) |
