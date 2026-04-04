# ft_lex - Automata Pipeline

## Multi-rule Regex -> NFA -> DFA

> Technical reference for the `src/automata` module.

---

## 1. Overview

The automata module now compiles a full lexer rule set (not a single regex) into one deterministic automaton.

Global flow:

```txt
rules[]
  -> Parser (per rule)
  -> Thompson (per rule NFA, tagged with rule index)
  -> ParsingPipeline::merge (single merged NFA)
  -> SubsetConstruction::build (DFA)
  -> SubsetConstruction::complete (total DFA with sink)
```

Main entry point:

```cpp
automata::ParsingPipeline pipeline;
automata::DFA dfa = pipeline.execute(rules);
```

`rules` is `std::vector<lexer_file::Rule>`, and order matters: lower index means higher priority.

---

## 2. Data Model

### 2.1 NFA

`NFA` represents a non-deterministic automaton with epsilon transitions:

```cpp
struct NFA {
    int initial_state_;
    std::unordered_map<int, int> final_states_; // state id -> rule index
    std::vector<std::unordered_map<char, std::vector<int>>> transitions_;
    std::vector<std::vector<int>> epsilon_transitions_;
    std::unordered_set<char> alphabet_;
};
```

Important detail: `final_states_` is no longer a plain list. It stores which rule each accepting state belongs to.

### 2.2 DFA

```cpp
struct DFA {
    int initial_state_;
    std::unordered_map<int, int> final_states_; // state id -> selected rule index
    std::vector<std::unordered_map<char, int>> transitions_;
};
```

In the DFA, `final_states_[dfa_state]` is the winning rule for this state.

---

## 3. Parser (per rule)

`Parser` supports explicit character-class constructs as first-class regex atoms.

- tokenize regex
- insert explicit `CONCAT`
- convert infix to postfix with shunting-yard
- parse and normalize character classes (`[...]`, `[^...]`, POSIX classes)
- parse wildcard `.` as a character class (ASCII except `\n`)

Character classes are no longer desugared into long alternations like `(a|b|c|...)`.
Instead, parser emits a single `CHARCLASS` token carrying a fixed-size bitset:

```cpp
struct Token {
  TokenType type_;           // CHAR, CHARCLASS, UNION, ...
  char value_;               // used for CHAR
  std::bitset<128> charset_; // used for CHARCLASS
};
```

This keeps the regex representation compact and avoids union-branch explosion.

API:

```cpp
std::vector<Token> Parser::parse(const std::string& regex);
```

This stage is executed once per rule pattern.

---

## 4. Thompson Construction (per rule)

API:

```cpp
NFA Thompson::compile(const std::vector<Token>& postfix, int index);
```

`index` is the rule index from the input `rules` vector.

In addition to literal `CHAR`, Thompson now handles `CHARCLASS` directly.
A `CHARCLASS` token builds one fragment with:

- one start state
- one next state
- one outgoing symbol transition per enabled bit in `bitset<128>`

This preserves semantics while keeping automata size controlled.

After building the NFA fragment, Thompson creates one final state and tags it with the rule index:

```cpp
nfa_.final_states_ = {{final, index}};
```

So each per-rule NFA carries rule identity at acceptance.

---

## 5. ParsingPipeline::execute (multi-rule)

`ParsingPipeline::execute(const std::vector<lexer_file::Rule>& rules)` does:

1. Reinitialize builders (`Thompson`, `SubsetConstruction`).
1. For each rule, parse `rule.pattern_` then compile an NFA with its rule index.
1. Merge all NFAs into one NFA (`merge`).
1. Build DFA from merged NFA.
1. Complete DFA (add sink transitions when missing).

Returns one DFA able to recognize every rule.

---

## 6. NFA Merge Strategy

`ParsingPipeline::merge` combines all per-rule NFAs with a fresh shared initial state:

- create merged state `0` as global start
- copy each NFA with an offset in state ids
- copy all symbol and epsilon transitions with offset
- copy each `final_state -> rule_index` into merged map with offset
- union all alphabets
- add epsilon from merged start to each shifted rule start

This preserves rule identity while creating a single connected NFA.

If no NFA is provided, merge throws:

```txt
No NFA to merge
```

---

## 7. Subset Construction and Rule Priority

### 7.1 build

`SubsetConstruction::build` performs standard powerset construction with bitmask states.

For each discovered subset mask, transitions are computed for each symbol in `nfa.alphabet_`.

### 7.2 final state selection

`SubsetConstruction::final_states` computes DFA accepting states from discovered masks.

If a DFA subset contains multiple NFA finals from different rules, it picks the minimum rule index:

- rule priority = smallest index
- deterministic tie-break compatible with rule ordering

So the first matching rule in the original rule list wins.

### 7.3 complete

`SubsetConstruction::complete` ensures DFA totality:

- any missing transition goes to one sink state
- sink loops to itself on all alphabet symbols

---

## 8. Practical Consequences

- One automaton now handles all lexer rules.
- Accepting DFA states contain the selected rule index directly.
- Rule order in input is semantic and must be stable.
- Multi-match conflicts are resolved during DFA construction (min rule index).
- Character classes are represented atomically from parser to NFA (`CHARCLASS + bitset<128>`).
- Wildcard `.` shares the same internal representation as other character classes.

---

## 9. Notes and Limits

- Subset bitmasks use `uint64_t`, so the current implementation assumes at most 64 NFA states in one merged construction path.
- `ParsingPipeline::execute` expects at least one rule.
