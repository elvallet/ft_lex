# ft_lex — Automata Pipeline

## Regex → NFA → DFA

> Technical Reference — 42 School Project

---

## 1. Pipeline Overview

The ft_lex automata pipeline transforms a regular expression string into a deterministic finite automaton (DFA) ready for lexical analysis. It is composed of three independent, testable stages:

```txt
string  →  Parser  →  Thompson  →  SubsetConstruction  →  DFA
```

| Stage | Input | Output | Algorithm |
| - | - | - | - |
| Parser | regex string | `vector<Token>` (postfix) | Shunting-yard (Dijkstra) |
| Thompson | `vector<Token>` | NFA | Thompson's construction |
| SubsetConstruction | NFA | DFA | Powerset construction |

Connecting the three stages:

```cpp
Parser              parser;
Thompson            thompson;
SubsetConstruction  sc;

vector<Token> postfixe = parser.parse("a(b|c)*");
NFA           nfa      = thompson.compile(postfixe);
DFA           dfa      = sc.build(nfa);
sc.complete(dfa, nfa);
```

---

## 2. Token

The Token is the unit of communication between the Parser and Thompson. It encapsulates both the type of a regex element and its optional character value.

### 2.1 TokenType enum

| Value | Meaning | Arity |
| - | - | - |
| `CHAR` | Literal character (a–z, A–Z, 0–9) | operand |
| `UNION` | Alternation operator `\|` | binary |
| `CONCAT` | Concatenation operator `·` (inserted synthetically) | binary |
| `STAR` | Kleene star `*` | unary |
| `PLUS` | One-or-more `+` | unary |
| `QUESTION` | Optional `?` | unary |
| `LPAREN` | Left parenthesis `(` | — |
| `RPAREN` | Right parenthesis `)` | — |

### 2.2 Token struct

```cpp
struct Token {
    TokenType  type_;
    char       value_;   // meaningful only when type_ == CHAR
};
```

---

## 3. Parser

The Parser converts a raw regex string into a postfix token sequence. It operates in two sequential passes, exposed through a single public method:

```cpp
vector<Token> Parser::parse(const string& regex);
```

### 3.1 Pass 1 — tokenize_and_insert_concat()

Scans the input character by character. For each character it creates a Token, then decides whether to insert a synthetic `CONCAT` token before pushing it.

**Concatenation insertion rule** — insert `CONCAT` between `(left, right)` when:

- left is: `CHAR`, `STAR`, `PLUS`, `QUESTION`, or `RPAREN`
- AND right is: `CHAR` or `LPAREN`

Example — `"a(b|c)*"`:

```txt
a  ·  (  b  |  c  )  *
```

**Key implementation notes:**

- Check `!tokens.empty()` before comparing — not `size() > 1`, which misses the second token
- The `·` character is UTF-8 multibyte and cannot be used as a `char` literal; `CONCAT` is always inserted synthetically, never parsed from input

### 3.2 Pass 2 — shunting_yard()

Implements Dijkstra's Shunting-yard algorithm. Uses an output `vector<Token>` and an operator `stack<Token>`.

**Operator priorities:**

| Operator | Priority |
| - | - |
| `STAR`, `PLUS`, `QUESTION` | 3 (highest) |
| `CONCAT  ·` | 2 |
| `UNION   \|` | 1 (lowest) |
| `LPAREN` | 0 — never popped by operators |

**Processing rules:**

- `CHAR` → push directly to output
- `LPAREN` → push to operator stack
- `RPAREN` → pop operators to output until `LPAREN`; discard `LPAREN`; throw if stack empty before finding `LPAREN`
- Operator → pop operators of higher or equal priority to output; then push current operator
- End of input → drain operator stack to output; throw if `LPAREN` found (unclosed parenthesis)

**Example trace — `"a·(b|c)*"` → postfix:**

```txt
Token   Output                  Stack
a       [a]                     []
·       [a]                     [·]
(       [a]                     [·, (]
b       [a, b]                  [·, (]
|       [a, b]                  [·, (, |]
c       [a, b, c]               [·, (, |]
)       [a, b, c, |]            [·]
*       [a, b, c, |, *]         [·]
end     [a, b, c, |, *, ·]      []
```

---

## 4. Thompson's Construction

Converts the postfix token sequence into an NFA. Processes tokens left to right using a `stack<Fragment>`. Each operator pops one or two fragments, combines them, and pushes the result.

### 4.1 Key structures

#### DanglingOut

Represents a transition that exists in the NFA graph but whose destination state is not yet determined — it is "pending" connection to the next fragment.

```cpp
struct DanglingOut {
    int   state_;       // source state
    bool  is_epsilon_;  // type of pending transition
    char  c_;           // ignored if is_epsilon_ == true
};
```

#### Fragment

An incomplete NFA sub-graph with exactly one entry point and a list of dangling outputs.

```cpp
struct Fragment {
    int                  start_;  // entry state ID
    vector<DanglingOut>  out_;    // pending transitions
};
```

### 4.2 Utility methods

#### add_state() → int

Appends an empty slot to `transitions_` and `epsilon_transitions_` simultaneously. Returns the new state ID (= current size − 1). Every state creation goes through this method.

#### patch(vector\<DanglingOut\>& out, int target) → void

Connects all pending transitions in `out` to `target`. For each `DanglingOut`: if `is_epsilon_` → append `target` to `epsilon_transitions_[state_]`; otherwise → append `target` to `transitions_[state_][c_]`.

### 4.3 Fragment constructors

| Method | Pops | Behaviour |
| - | - | - |
| `make_literal(c)` | 0 | Creates 2 states, 1 char transition. `DanglingOut` is an ε from the second state. |
| `make_concat(a, b)` | 2 | Patches `a.out_` → `b.start_`. Returns `{a.start_, b.out_}`. |
| `make_union(a, b)` | 2 | New state `s` with ε→`a.start_` and ε→`b.start_`. Returns `{s, a.out_ ∪ b.out_}`. |
| `make_star(a)` | 1 | New state `s` with ε→`a.start_`. Patches `a.out_`→`s` (loop). Returns `{s, {ε from s}}`. |
| `make_plus(a)` | 1 | New state `s` with ε→`a.start_`. Patches `a.out_`→`s` (loop). Returns `{a.start_, {ε from s}}`. |
| `make_question(a)` | 1 | New state `s` with ε→`a.start_`. Returns `{s, a.out_ + {ε from s}}`. |

**Key distinction between `*` and `+`:** both create a loop state `s`, but `make_star` returns `s` as `start_` (allowing zero traversals), while `make_plus` returns `a.start_` (forcing at least one traversal).

### 4.4 compile() — main loop

```txt
for each token in postfixe:
    CHAR      → push make_literal(c)
    CONCAT    → b=pop, a=pop, push make_concat(a, b)
    UNION     → b=pop, a=pop, push make_union(a, b)
    STAR      → a=pop, push make_star(a)
    PLUS      → a=pop, push make_plus(a)
    QUESTION  → a=pop, push make_question(a)

final_state = add_state()
patch(stack.top().out_, final_state)
nfa.initial_state_ = stack.top().start_
nfa.final_states_  = { final_state }
```

When popping two fragments for binary operators, the first pop is `b` and the second is `a` — order matters for concat.

### 4.5 NFA state counts (reference)

| Regex | States | Notes |
| - | - | - |
| `a` | 3 | `q0 --a--> q1 --ε--> q2(final)` |
| `ab` | 5 | two literals + concat |
| `a\|b` | 6 | two literals + union state + final |
| `a*` | 4 | literal + loop state + final |
| `a(b\|c)*` | 9 | full example — see §4.6 |

### 4.6 Example — a(b|c)*

Full NFA produced by Thompson:

```txt
q0 --a--> q1
q1 --ε--> q7
q2 --b--> q3     q3 --ε--> q7
q4 --c--> q5     q5 --ε--> q7
q6 --ε--> q2     q6 --ε--> q4   (union b|c)
q7 --ε--> q6     q7 --ε--> q8   (Kleene star)
q8 = final state
```

---

## 5. Subset Construction (NFA → DFA)

Converts the NFA produced by Thompson into a fully deterministic DFA. Each DFA state corresponds to a set of NFA states reachable simultaneously under the same input prefix. NFA state sets are represented as `uint64_t` bitmasks for efficiency.

### 5.1 Core operations

#### epsilon_closure(states) → uint64_t

Starting from a bitmask of NFA states, follows all ε-transitions (BFS) and returns the expanded bitmask. Every state reachable without consuming a character is included.

#### delta(states, symbol) → uint64_t

From a bitmask of NFA states, collects all states reachable by consuming `symbol`, then applies `epsilon_closure` to the result.

### 5.2 build() algorithm

```txt
initial_dfa_state = epsilon_closure({ nfa.initial_state_ })
worklist = [ initial_dfa_state ]

while worklist not empty:
    S = pop from worklist
    for each symbol in nfa.alphabet_:
        T = delta(S, symbol)
        if T not empty and T not yet seen:
            add T to worklist
        add transition S --symbol--> T to DFA

DFA state is final if its bitmask contains any NFA final state
```

### 5.3 complete()

Adds a sink (dead) state to ensure the DFA is total — every (state, symbol) pair has exactly one transition. Any missing transition is routed to the sink state, from which no final state is reachable. Required for correct simulation.

### 5.4 Example — DFA for a(b|c)*

| DFA State | Accepts | On a | On b | On c |
| - | - | - | - | - |
| q0 (initial) | no | q1 | q4 (sink) | q4 (sink) |
| q1 | yes | q4 | q3 | q2 |
| q2 | yes | q4 | q3 | q2 |
| q3 | yes | q4 | q3 | q2 |
| q4 (sink) | no | q4 | q4 | q4 |

The DFA correctly accepts `a`, `ab`, `ac`, `abbc`, `abcbc`… and rejects `b`, `ba`, empty string, etc.

---

## 6. Testing Strategy

Each stage is validated independently before integration.

### 6.1 Parser tests

| Input | Expected postfix output |
| - | - |
| `ab` | `[a, b, ·]` |
| `a\|b` | `[a, b, \|]` |
| `a*b` | `[a, *, b, ·]` |
| `a(b\|c)` | `[a, b, c, \|, ·]` |
| `(a` | throw — unclosed parenthesis |
| `a)` | throw — unexpected closing parenthesis |
| *(empty)* | throw — empty regex |

### 6.2 Thompson tests

Validate structural properties of the produced NFA: correct state count, exactly one final state, no transitions out of the final state, and expected graph structure traced by hand. A `print_nfa()` debug helper is strongly recommended during development.

### 6.3 Integration tests

| Regex | Accepts | Rejects |
| - | - | - |
| `ab` | `ab` | `a`, `b`, `abc` |
| `a\|b` | `a`, `b` | `ab`, *(empty)* |
| `a*` | *(empty)*, `a`, `aaa` | `b`, `ab` |
| `a(b\|c)*` | `a`, `ab`, `ac`, `abbc`, `abcbc` | `b`, `ba`, *(empty)* |

---

## 7. NFA Structure Reference

The `NFA` struct is the interface between Thompson and SubsetConstruction. Thompson writes it; SubsetConstruction reads it.

```cpp
struct NFA {
    int                                                      initial_state_;
    vector<int>                                              final_states_;
    vector<unordered_map<char, vector<int>>>                 transitions_;
    vector<vector<int>>                                      epsilon_transitions_;
    unordered_set<char>                                      alphabet_;
};
```

States are identified by their index in `transitions_` and `epsilon_transitions_`. Creating a state = pushing an empty slot to both vectors simultaneously via `add_state()`. Thompson always produces exactly one final state.
