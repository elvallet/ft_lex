# Rust Backend

`ft_lex --rust` generates a `lex.yy.rs` file instead of `lex.yy.c`. This document covers the architecture of the Rust backend, how it differs from the C backend, and the design decisions behind it.

## Archtecture overview

The C backend works by substituting markers into an embedded template (`yylex_template.c`). The scanner logic and the generated tables live in the same file.
The Rust backend uses a different architecture where the scanner engine lives entirely in a separate runtime crate (`ftlex_runtime`), and the generated file contains only:

1. DFA tables as static arrays
2. A marker type `GeneratedLexer`
3. An `impl LexerDef for GeneratedLexer` block wiring the tables to the engine

```architecture
.l file
   │
   ▼
ft_lex --rust
   │
   ├── lex_yy.rs          (tables + LexerDef impl)
   │
   └── ftlex_runtime      (Scanner<D>, yylex logic, public API)
           │
           └── Scanner::<GeneratedLexer>::yylex()
                   │
                   ├── D::transition()       ← calls into lex.yy.rs
                   ├── D::accept_entries()   ← calls into lex.yy.rs
                   └── D::execute_action()   ← calls into lex.yy.rs
```

This separation means the scanner engine is compiled once into the runtime crate and reused across all generated scanners, rather than being re-emitted into every generated file.

## The `LexerDef` trait

The contract between the generated code and the runtime is a trait:

```rust
pub trait LexerDef {
    type UserData;

    fn transition(state: usize, c: u8) -> i32;

    /// (rule_id, trailing_len, trailing_dfa_id).
    /// trailing_len: -1 none, >= 0 fixed, -2 variable.
    /// trailing_dfa_id: -1 unless trailing_len == -2.
    fn accept_entries(state: usize) -> &'static [(i32, i32, i32)];

    fn start_state(condition: usize, bol: bool) -> usize;
    fn sink() -> usize;
    fn execute_action(scanner: &mut Scanner<Self>, rule_id: i32) -> Option<i32>
    where Self: Sized;
    fn yywrap(scanner: &mut Scanner<Self>) -> bool
    where Self: Sized { true }

    /// Only overridden when the grammar has at least one variable-length
    /// trailing context rule. Defaults to "no trailing DFAs exist".
    fn yytrailing_transition(dfa_id: usize, state: usize, c: u8) -> i32 { -1 }
    fn yytrailing_accept(dfa_id: usize, state: usize) -> bool { false }
}
```

All methods are associated functions (no `self`) - they operate on static data or on an explicit `&mut Scanner<Self>` parameter. `GeneratedLexer` is an empty marker type that exists solely to carry this implementation.

`type UserData` lets a grammar carry custom state across `yyless` calls (for example, a list of files to chain through, mirroring the C backend's "reopen the next file in `yywrap`" pattern - see §"Custom user data with multi-file scanning" below). It has no default and must always be set, even to `()` when unused; `ft_lex` emits `type UserData = ();` automatically unless the `.l` file declares `%rust_user_data <TypeName>`.

## Generated file structure

```rust
// 1. Imports
use ftlex_runtime::{LexerDef, Scanner};
use std::io::Write;

// 2. User prologue (verbatim top section of the .l file)

// 3. Start condition constants
pub const INITIAL: usize = 0;
pub const YYNB_CONDITIONS: usize = N;

// 4. DFA tables
static YYSTART_STATES: [usize; N] = [...];
static YYTABLE: [[i32; 256]; N] = [...];
static YYACCEPT_DATA: &[(i32, i32, i32)] = &[...];
static YYACCEPT_OFFSET: [i32; N] = [...];
static YYACCEPT_COUNT: [usize; N] = [...];

// 4b. Trailing DFA tables - only emitted if the grammar has at least
// one variabke-length trailing context rule
static YYTRAILING_0_TABLE: [[i32; 256]; N0] = [...];
static YYTRAILING_0_ACCEPT: [bool; N0] = [...];
// one TABLE/ACCEPT pair per variable-trailing rule

// 5. Marker type + trait implementation
pub struct GeneratedLexer;

impl LexerDef for GeneratedLexer {
    type UserData = (); // or the type named by %rust_user_data

    fn transition(state: usize, c: u8) -> i32 { YYTABLE[state][c as usize] }
    fn accept_entries(state: usize) -> &'static [(i32, i32, i32)] { ... }
    fn start_state(condition: usize, bol: bool) -> usize { ... }
    fn sink() -> usize { SINK_STATE }

    // Only emitted if the grammar has variable-length trailing context
    fn yytrailing_transition(dfa_id: usize, state: usize, c: u8) -> i32 {
        match dfa_id {
            0 => YYTRAILING_0_TABLE[state][c as usize],
            _ => -1,
        }
    }
    fn yytrailing_accept(dfa_id: usize, state: usize) -> bool {
        match dfa_id {
            0 => state < YYTRAILING_0_ACCEPT.len() && YYTRAILING_0_ACCEPT[state],
            _ => false,
        }
    }

    // Only emitted if the .l file's epilogue defines `fn user_yywrap`
    fn yywrap(scanner: &mut Scanner<Self>) -> bool { user_yywrap(scanner) }

    #[allow(unreachable_code)]
    fn execute_action(scanner: &mut Scanner<Self>, rule_id: i32) -> Option<i32> {
        match rule_id {
            0 => { /* user action */ None }
            1 => { /* user action */ None }
            _ => None,
        }
    }
}

// 6. User epilogue (verbatim bottom section of the .l file)

// 7. fn main() - emitted only if the epilogue does not define one
```

## Key design decisions

### No global mutable state

The C runtime relies on global variables (`yyin`, `yyout`, `yytext`, etc). In Rust, `static mut` requires `unsafe` and is incompatible with the ownership model. All scanner state lives in `Scanner<D>` a struct instanciated by the caller.

### `yytext` as owned `String`

The C runtime manages `yytext` as a raw `char*` with manual `malloc`/`free`, or as a fixed stack buffer in `%array` mode. In Rust, `yytext` is an owned `String` - allocation is handled automatically, and the distinction between `%array` and `%pointer` disappears entirely.

### `execute_action` return value

In C, a rule aciton can `return TOKEN` from `yylex` because actions are inlined inside the function body. In Rust, `execute_action` is a separate function call. Returning a token is expressed as `return Some(TOKEN)` inside the action body; `yylex` inspects the return value and propagates it. Actions that consume input without returning a token fall through to `None`.

### `None` appended to every action brace

Because actions are emitted as verbatim text, the codegen cannot know wheter an action returns explicitly. A trailing `None` is appended after every action block so that the `match` arm always has a type `Option<i32>`, regardless of what the user wrote. Actions that contain `return Some(...)` make the `None` unreachable - the `#[allow(unreachable_code)]` attribute on `execute_action` suppresses the resulting warning.

### `yywrap` overrride via a detected marker function

The C runtime provides `yywrap` as a weak symbol in `libl`, allowing user code to overrid it by defining a function with the same name. Rust has no portable weak symbol mechanism, and two `impl LexerDef for GeneratedLexer` blocks for the same type in the same file is a compile error - so the override can't be a second `impl` block either.

Instead, Codegen looks for a free function named `fn user_yywrap(scanner: &mut Scanner<GeneratedLexer>) -> bool` in the `.l` file's epilogue (the verbatim bottom section). If found, the single generated `impl LexerDef for GeneratedLexer` block includes:

```rust
fn yywrap(scanner: &mut Scanner<Self>) -> bool { user.yywrap(scanner) }
```

If no `user_yywrap` is detected, no override is emitted and the trait's default (`true`, stop at EOF) applies. This is the same "look for a known identifier in the epilogue" pattern already used to detect a user-supplied `fn main` (see  "Generated file structure" above) - kept deliberately simple and consistent rather then introducing a new directive syntax.

### Custom user data and multi-file scanning

`Scanner<D>` carries a `pub user_data: D::UserData` field. Combined with the `yywrap` overrrid above, this is what makes patterns like chaining multiple input files (the C backend's classic `argv` + `yywrap` reopen loop) expressible in Rust without resorting to global mutable state:

```lexer
%rust_user_data FileList

%{
    pub struct FileList {
        files: Vec<String>,
        index: usize,
    }
%}

%%
[a-z]+  { print!("{}", scanner.yytext); return Some(1); }
%%

fn user_yywrap(scanner: &mut Scanner<GeneratedLexer>) -> bool {
    if scanner.user_data.index >= scanner.user_data.files.len() {
        return true; // no more files: stop
    }
    let path = scanner.user_data.files[scanner.user_data.index].clone();
    scanner.user_data.index += 1;
    match std::fs::File::open(&path) {
        Ok(f) => { scanner.yyin = Box::new(f), false }
        Err(_) => true,
    }
}
```

`%rust_user_data <TypeName>` tells Codegen to emit `type UserData = TypeName;` instead of the default `type UserData = ();`. The named type must be defined somewhere reachable in the generated file (typically the `%{ %}` prologue). It must be defined in `pub`: `pub struct <TypeName>`. Because `Scanner::new` takes the initial `user_data` value as a constructor argument, a `.l` file using `%rust_user_data` needs its own `fn main()` (see "Generated file structure") rather than relying on the default `ftlex_main`, which only knows how to construct `()`.

### `PhantomData<D>` in `Scanner<D>`

`Scanner<D>` is generic over `D: LexerDef` but stores no value of type `D` - it only calls associated functions on `D`. Rust requires that every type parameter be "used" in the struct layout for variance and lifetime analysis. `PhantomData<D>` satisfies this requirement at zero runtime cost.

## Difference from the C backend

| Aspect | C backend | Rust bacekend |
| - | - | - |
| Scanner logic | Embedded template (`yylex_template.c`) | Runtime crate (`ftlex_runtime`) |
| Table compression | Supported `-c` | Not supported |
| `yytext` type | `char*` or `char[]` | `String` |
| `%array` / `%pointer` | Supported | Ignored |
| POSIX macros | Preprocessor macros | Methods on `scanner` |
| `yywrap` override | Weak symbol | Detected `fn user_yywrap` marker, wired into the generated `impl` |
| Custom per-scanner state | Global variables | `Scanner::user_data` typed via `%rust_user_data` |
| Trailing context (fixed) | Folded into main DFA, rewound via buffer bookkeeping | Same |
| Trailing context (variable) | Isolated DFA, simulated via `yy_simulate_trailing` | Same, via `D::yytrailing_transition`/`yytrailing_accept` |
| Memory cleanup | `yylex_destroy()` | Automatic |
| Output file | `lex.yy.c` | `lex.yy.rs` |

## Known limitations

- **No table comrpession** - the Rust backend always emits the full `[[i32; 256]; N]` transition table. The `-c` flag has no effect.
- **No `%array`/`%pointer` - these declarations are parsed and ignored. `yytext` is always an owned `String`.
- `yywrap` **override requires a separate file** - two `impl` blocks for the same trait on the same type in the same file is a compile error in Rust.
- **No syntax checking of action bodies** - `ft_lex` treats action text as opaque. Rust syntax errors in actions are reported by `rustc`, not `ft_lex`.
