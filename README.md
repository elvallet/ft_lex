# ft_lex

A complete reimplementation of the POSIX `lex` lexical analyzer generator, built as a 42 School project. Takes a `.l` specification file as input and produces a `lex.yy.c` C scanner, comptable with the POSIX.1-2024 standard.

---

## Features

- Full POSIX ERE regex support: character classes `[a-z]`, `[^...]`, POSIX classes `[:alpha:]`, wildcard `.`, alternation, grouping, counted repetitions `{n}` / `{n, m}` / `{n,}`
- Macro definitions with inter-macro references
- Start conditions: inclusive `%s` and exclusive `%x`
- BOL anchor `^` with per-condition dual DFA entry points
- EOL anchor `$` via trailing context
- Trailing context `r/s`, fixed-length or variable-length (isolated DFA simulated at runtime)
- `yytext` in pointer mode (default, heap-allocated) or array mode (`%array`)
- Runtime macros: `BEGIN()`, `REJECT`, `yymore()`, `yyless()`, `ECHO`, `input()`, `unput()`
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
./ft_lex input.l       # produces lex.yy.c
./ft_lex -c input.l    # produces lex.yy.c with compressed DFA tables
./ft_lex -v input.l    # verbose mode, displays statistics
```

### Compiling the generated scanner

```sh
cc lex.yy.c -o scanner -L libl/ -ll
#  or
make scan GRAMMAR=path/to/input.l
```

---

## Architecture

The pipeline transforms a `.l` file into a C scanner through five sequential stages:

```txt
.l file
   └─ LexParser                                    - tokenizes sections, expands macros, extracts rules
        └─ Thompson                                - build one NFA fragment per rule (tagged with rule index)
            └─ ParsingPipeline                     - groups by start condition, merges into one NFA, compiles variable-trailing DFAs
                    └─ SubsetConstruction          - determinizes into a DFA, assigns start states
                            └─ Codegen             - serializes DFA + actions into lex.yy.c
```

Each stage is documented in detail under `/docs`.

---

## Bonus

### Rust backend

`ft_lex` can generate a Rust scanner instead of a C scanner via the `--rust` flag:

```sh
./ft_lex --rust input.l    # produces lex.yy.rs
```

The generated file contains only DFA tables and action dispatch. The scanner engine lives in a companion runtime crate (`ftlex_runtime`) that must be added as a dependency to your Cargo project:

```toml
[dependencies]
ftlex_runtime = { path = "path/to/ftlex_runtime" }
```

Rule actions are written in Rust. The POSIX macro API is exposed as methods on `scanner`:

```lexer
%%

[a-zA-Z]+       { scanner.echo(); return Some(1); }
[0-9]+          { let _ = scanner.yyout.write_all(b"NUMBER\n"); return Some(2); }

%%
```

See [`docs/rust_backend.md`](docs/rust_backend.md) for the full architecture and design decisions, and [`ftlex_runtime/README.md`](runtime/README.md) for the scanner API reference.

### DFA table compression

When the `-c` flag is passed, the dense transition table is replaced with a compressed `base` / `check` / `next` representation - approximately 25-35% of the original size with a modest runtime cost per lookup.
See [`docs/codegen.md`](docs/codegen.md)  §3.5 and  §7 for the algorithm and size tradeoffs.

---

## Documentation

| Document | Contents |
| - | - |
| [`docs/lexer_file_parser.md`](docs/lexer_file_parser.md) | `.l` file format, `LexParser` internals, macro expansion, condition parsing |
| [`docs/automata.md`](docs/automata.md) | NFA/DFA data models, Thompson construction, subset construction, BOL handling |
| [`docs/codegen.md`](docs/codegen.md) | Generated file structure, DFA tables, `yylex()` template, compression mode |
| [`docs/tests.md`](docs/tests.md) | Integration and stress test battery (categories A-E) |
