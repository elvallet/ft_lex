# ft_lex - Automata Pipeline

## Multi-rule and Start-Condition Aware Regex -> NFA -> DFA

> Technical reference for the `src/automata` module.

---

## 1. Overview

The automata module compiles a full lexer specification into one deterministic automaton while preserving lex rule priority and start-condition semantics.

Global flow:

```txt
rules[] + declared conditions
  -> Parser (per rule)
  -> Thompson (per rule NFA, tagged with rule index)
  -> ParsingPipeline::group by condition
  -> ParsingPipeline::merge_keyed (single merged NFA + per-condition entries)
  -> SubsetConstruction::build (DFA + per-condition DFA start states)
  -> SubsetConstruction::complete (total DFA with sink)
```

Main entry point:

```cpp
automata::ParsingPipeline pipeline;
automata::DFA dfa = pipeline.execute(rules, conditions);
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

`final_states_` stores the rule index attached to each accepting NFA state.

### 2.2 DFA

```cpp
struct DFA {
    int initial_state_;
    std::unordered_map<int, std::vector<int>> final_states_; // dfa state -> list of rule indices (sorted by priority)
    std::vector<std::unordered_map<char, int>> transitions_;
    std::map<std::string, int> start_states_;   // condition name -> dfa entry state
    int sink_;                                   // sink state id for invalid transitions
    std::vector<DFA> trailing_dfas_;            // isolated DFAs for variable-length trailing context, see §9
};
```

- `initial_state_` is the DFA state used by `INITIAL`.
- `final_states_` stores a **list of matching rule indices** for each accepting state, sorted in ascending order (index 0 = highest priority).
- `start_states_` contains entries for each condition, including BOL variants (e.g., `INITIAL`, `COMMENT`, `COMMENT_BOL`). Used by generated `BEGIN(X)` support.
- `sink_` is the state id assigned to the sink state created during completion. All undefined transitions route to this state. The sink state loops to itself on all symbols, ensuring the scanner can gracefully handle unexpected input without crashing.
- `trailing_dfas_` holds one standalone DFA per rule with varianle-length trailing context, indexed by `Rule::trailing_dfa_id_`. Empty when the grammar uses no variable-length trailing context. See §9.

---

## 3. Condition-Aware Grouping

Before merging NFAs, rules are grouped per start condition.

Selection rules:

- A rule tagged with `<COND>` is included in `COND` only.
- A rule without explicit condition (`INITIAL` only) is also included in each inclusive `%s` condition.
- A rule without explicit condition is not injected into exclusive `%x` conditions.

This matches expected lex behavior for inclusive/exclusive states.

---

## 3.1 Beginning-of-Line (BOL) Anchor Handling

Rules with `^` anchor are marked as BOL-only. For each start condition, two NFA entry groups are created:

- **Normal group**: rules without `^` anchor (active after any character is matched)
- **BOL group**: rules with `^` anchor (active at start of buffer or after newline)

In the merged NFA, each condition gets two entry points:

- `CONDITION` - entry after normal matching
- `CONDITION_BOL` - entry for BOL-anchored rules

The generated scanner initializes with `INITIAL_BOL` and switches to `INITIAL` after consuming the first character. After matching `\n`, the scanner switches back to the BOL variant.

The `dfa.start_states_` map includes both variants, and the runtime manages the transition between them.

---

## 4. Keyed Merge Strategy

`ParsingPipeline::merge_keyed` merges all groups into one NFA and returns per-condition NFA entry points.

For each condition:

- create one synthetic condition entry state
- copy each included NFA with offset remapping
- preserve `final_state -> rule_index`
- add epsilon from condition entry to each shifted rule NFA start

Output:

```cpp
std::pair<NFA, std::map<std::string, int>>
```

The map is `condition -> entry_state_in_merged_nfa`.

---

## 5. Subset Construction with Multiple Entries

`SubsetConstruction::build(const NFA&, const std::map<std::string, int>& entry_points)` seeds one epsilon-closure subset per condition entry point.

Internally, a DFA state is identified by a `StateSet`:

- `StateSet` stores the active NFA state ids in a sorted `std::vector<int>`
- `epsilon_closure()` and `delta()` always normalize the result before returning it
- `StateSet::operator==` compares the normalized vectors directly
- `StateSetHash` hashes the same normalized vector so `seen_` can deduplicate identical subsets

That means two different paths that reach the same NFA subset will always map to the same DFA state id.

Build-time behavior:

- each distinct subset gets one DFA id
- if two conditions map to the same closure, they reuse the same DFA id
- `dfa.start_states_[name]` records the DFA id for each condition (including BOL variants)
- `dfa.initial_state_ = dfa.start_states_["INITIAL"]`

**Accepting state handling**: when a subset contains multiple NFA final states (each tagged with a rule index):

- All matching rule indices are collected and **stored in ascending order** (lowest index = highest priority)
- This sorted list is saved in `dfa.final_states_[dfa_state_id]`
- The runtime uses this list to select the highest-priority rule when dispatching actions

---

## 6. Completion and Determinism

`SubsetConstruction::complete` ensures total transitions over the discovered alphabet:

- missing edges go to one sink state
- sink loops to itself for all alphabet symbols

This keeps scanner runtime logic simple (single table lookup path).

---

## 7. Practical Consequences

- One DFA now supports normal scanning, BOL-anchored rules, and start-condition mode switching.
- Rule order remains semantic; the first rule wins on conflicts (stored in ascending index order in `final_states_`).
- Inclusive `%s` conditions inherit unqualified rules.
- Exclusive `%x` conditions only activate explicitly qualified rules.
- Generated code can switch condition with `BEGIN(NAME)` and continue from the right DFA entry.
- BOL transitions are managed by the generated scanner (switches to `CONDITION_BOL` after matching newline).

---

## 8. Notes and Limits

- Subset identity is based on the canonical `StateSet` vector, not on a fixed-size bitmask, so the construction is not tied to a 64-state limit.
- `INITIAL` is always injected in condition handling, even if not explicitly declared.
- Each condition gets a BOL variant (`CONDITION_BOL`) for `^`-anchored rules, automatically managed by the runtime.

---

## 9. Variable-Length Trailing Context DFAs

In addition to the main merged DFA, `ParsingPipeline::execute` compiles one **isolated DFA per rule** whose trailing context is variable-length (`Rule::trailing_is_variable_ -- true`).

For each such rule the trailing pattern alone (`Rule::trailing_`, not `pattern_ + trailing_`) is run through the same `Parser`  → `Thompson`  → `SubsetConstruction` pipeline used for the main DFA, but as a single-rule, single-condition automaton with no relation to the rest of the grammar. The rusulting DFA is appended to `dfa.trailing_dfas_`, and the originating rule's `trailing_dfa_id_` is set to its index in that vector.

```cpp
for (size_t i = 0; i < rules.size(); ++i) {
    if (rules[i].trailing_is_variable_) {
        NFA nfa = thompson_.compile(parser_.parse(rules[i].trailing_), i);
        DFA dfa = subset_construction_.build(nfa, {{"INITIAL", i}});
        trailing_dfas.push_back(dfa);
        rules[i].trailing_dfa_id_ = count++;
    }
}
```

These trailing DFAs always start at state `0` (a property the runtime relies on directly, without needing a `strat_states_` lookup). They are serialized by Codegen alongside the main DFA (see `docs/codgen.md` §3.6) and simulated at runtime the base-pattern/trailing-pattern boundary for each match (see `docs/codegen.md` §4.7).

This keeps fixed-length and variable-length trailing context orthogonal at the automata leel: fixed-length trailing is folded directly into the main DFA path (`pattern_` + `trailing_` compiled together, see `compile_trailing_length()` in `docs/lexer_file_parser.md`), while variable-length trailing gets its own dedicated automaton resolved dynamically.

---

## 10. Regex Repetitions in Parser

`Parser::tokenize_and_insert_concat` supports brace-based repetitions by rewriting them into existing unary operators and explicit concatenations.

Supported forms:

- `{n}`: exactly `n` repetitions of the previous fragment.
- `{n,m}`: at least `n` and at most `m` repetitions.
- `{n,}`: at least `n` repetitions.

Previous fragment means:

- one character token (`CHAR`)
- one character class token (`CHARCLASS`)
- one parenthesized group (`(...)`)

Implementation strategy in `Parser.cpp`:

- `{n}` duplicates the previous fragment `n - 1` times, inserting `CONCAT` between copies.
- `{n,m}` first applies the `{n}` expansion, then appends `(m - n)` optional copies, each encoded as `CONCAT + fragment + QUESTION`.
- `{n,}` first applies the `{n}` expansion, then appends `PLUS` so the last mandatory fragment becomes one-or-more, yielding a minimum of `n`.

This keeps the downstream Thompson and shunting-yard logic unchanged, because repetitions are normalized into already-supported operators.
