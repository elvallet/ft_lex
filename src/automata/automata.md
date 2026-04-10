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
    std::unordered_map<int, int> final_states_; // dfa state -> selected rule index
    std::vector<std::unordered_map<char, int>> transitions_;
    std::map<std::string, int> start_states_;   // condition name -> dfa entry state
};
```

- `initial_state_` is the DFA state used by `INITIAL`.
- `start_states_` is used by generated `BEGIN(X)` support.

---

## 3. Condition-Aware Grouping

Before merging NFAs, rules are grouped per start condition.

Selection rules:

- A rule tagged with `<COND>` is included in `COND` only.
- A rule without explicit condition (`INITIAL` only) is also included in each inclusive `%s` condition.
- A rule without explicit condition is not injected into exclusive `%x` conditions.

This matches expected lex behavior for inclusive/exclusive states.

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

- each distinct subset gets one DFA id
- if two conditions map to the same closure, they reuse the same DFA id
- `dfa.start_states_[name]` records the DFA id for each condition
- `dfa.initial_state_ = dfa.start_states_["INITIAL"]`

`final_states` selection is unchanged: minimum rule index wins when a subset contains multiple accepting NFA states.

---

## 6. Completion and Determinism

`SubsetConstruction::complete` ensures total transitions over the discovered alphabet:

- missing edges go to one sink state
- sink loops to itself for all alphabet symbols

This keeps scanner runtime logic simple (single table lookup path).

---

## 7. Practical Consequences

- One DFA now supports normal scanning and start-condition mode switching.
- Rule order remains semantic; the first rule wins on conflicts.
- Inclusive `%s` conditions inherit unqualified rules.
- Exclusive `%x` conditions only activate explicitly qualified rules.
- Generated code can switch condition with `BEGIN(NAME)` and continue from the right DFA entry.

---

## 8. Notes and Limits

- Subset bitmasks use `uint64_t`, so the implementation assumes up to 64 NFA states in one merged construction.
- `INITIAL` is always injected in condition handling, even if not explicitly declared.
