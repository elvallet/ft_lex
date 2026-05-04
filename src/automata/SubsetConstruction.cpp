/**
 * @file SubsetConstruction.cpp
 * @brief Implementation of subset construction and DFA completion.
 */

#include "SubsetConstruction.hpp"
#include <stack>
#include <climits>
#include <algorithm>


using namespace automata; using namespace std;

size_t SubsetConstruction::StateSetHash::operator()(const StateSet& subset) const noexcept {
	// Hash the ordered subset representation so identical subsets reuse the same DFA id.
	size_t hash = 0;
	for (int state : subset.states_) {
		hash ^= std::hash<int>{}(state) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
	}
	return hash;
}

/**
 * @brief Build a DFA from an NFA using subset construction.
 * @param nfa Source NFA.
 * @return Equivalent DFA.
 */
DFA SubsetConstruction::build(const NFA& nfa, const map<string, int>& entry_points) {
	DFA				dfa;
	stack<StateSet>	worklist;
	int				id = 0;

	for (auto& [cond_name, nfa_entry] : entry_points) {
		// Seed one DFA subset per condition entry and reuse ids when closures are identical.
		StateSet	S	= epsilon_closure(nfa, StateSet{{nfa_entry}});
		if (seen_.find(S) == seen_.end()) {
			seen_.insert({S, id});
			worklist.push(S);
			id++;
		}
		dfa.start_states_[cond_name]	= seen_[S];
	}
	// The canonical DFA initial state is the one attached to INITIAL condition.
	dfa.initial_state_	= dfa.start_states_.at("INITIAL");

	while (!worklist.empty()) {
		StateSet	S	= worklist.top();
		worklist.pop();
		for (char sym : nfa.alphabet_) {
			StateSet	T	= epsilon_closure(nfa, delta(nfa, S, sym));
			if (!T.states_.empty()) {
				if (seen_.find(T) == seen_.end()) {
					seen_.insert({T, id++});
					worklist.push(T);
				}
				int S_id	= seen_[S];
				while (dfa.transitions_.size() <= static_cast<size_t>(S_id))
					dfa.transitions_.push_back({});
				dfa.transitions_[S_id][sym]	= seen_[T];
			}
		}
	}

	dfa.final_states_	= final_states(nfa);

	while (dfa.transitions_.size() < static_cast<size_t>(id))
		dfa.transitions_.push_back({});

	return dfa;
}

/**
 * @brief Ensure all DFA states have transitions for each alphabet symbol.
 * @param dfa DFA to complete in place.
 * @param nfa Source NFA used to access the alphabet.
 */
void SubsetConstruction::complete(DFA& dfa, const NFA& nfa) {
	const int	SINK = dfa.transitions_.size();
	bool		sink_needed = false;

	for (auto& trans: dfa.transitions_) {
		for (char c: nfa.alphabet_) {
			auto it = trans.find(c);
			if (it == trans.end()) {
				trans.insert({c, SINK});
				sink_needed = true;
			}
		}
	}

	if (sink_needed) {
		dfa.transitions_.push_back({});
		dfa.sink_	= SINK;
		for (char c: nfa.alphabet_) {
			dfa.transitions_[SINK].insert({c, SINK});
		}
	}
}

/**
 * @brief Compute epsilon-closure from a bitmask of NFA states.
 * @param nfa Source NFA.
 * @param states Input state bitmask.
 * @return Closure bitmask.
 */
SubsetConstruction::StateSet SubsetConstruction::epsilon_closure(const NFA& nfa, const StateSet& states) {
	StateSet	res;
	stack<int>	worklist;
	vector<char>	visited(nfa.transitions_.size(), 0);

	for (int state : states.states_) {
		if (state >= 0 && static_cast<size_t>(state) < visited.size() && !visited[state])
			worklist.push(state);
	}

	while (!worklist.empty()) {
		int x = worklist.top();
		worklist.pop();
		if (x < 0 || static_cast<size_t>(x) >= visited.size() || visited[x])
			continue;
		visited[x] = 1;
		res.states_.push_back(x);
		for (int e : nfa.epsilon_transitions_[x]) {
			if (e >= 0 && static_cast<size_t>(e) < visited.size() && !visited[e])
				worklist.push(e);
		}
	}

	// Keep the subset canonical before it is used as a key in seen_.
	sort(res.states_.begin(), res.states_.end());
	return res;
}

/**
 * @brief Apply one symbol move to a set of NFA states.
 * @param nfa Source NFA.
 * @param states Input state bitmask.
 * @param symbol Transition symbol.
 * @return Destination states bitmask.
 */
SubsetConstruction::StateSet SubsetConstruction::delta(const NFA& nfa, const StateSet& states, char symbol) {
	StateSet	res;
	vector<char>	visited(nfa.transitions_.size(), 0);

	for (int state : states.states_) {
		if (state < 0 || static_cast<size_t>(state) >= nfa.transitions_.size())
			continue;
		auto it = nfa.transitions_[state].find(symbol);
		if (it == nfa.transitions_[state].end())
			continue;
		for (int dest : it->second) {
			if (dest >= 0 && static_cast<size_t>(dest) < visited.size() && !visited[dest]) {
				visited[dest] = 1;
				res.states_.push_back(dest);
			}
		}
	}

	// Keep the subset canonical before it is used as a key in seen_.
	sort(res.states_.begin(), res.states_.end());
	return res;
}

/**
 * @brief Derive accepting DFA states from discovered NFA subsets.
 * @param nfa Source NFA.
 * @return Map of accepting DFA state ids to rule indices (all matching rules).
 *
 * All rule indices from NFA final states present in a subset are collected
 * and sorted by priority (index). The collection preserves all matching rules.
 */
unordered_map<int, vector <int>> SubsetConstruction::final_states(const NFA& nfa) {
	unordered_map<int, vector <int>> res;

	for (auto& [mask, dfa_id] : seen_) {
		vector<int> rules;
		for (auto& [final_bit, rule_index] : nfa.final_states_) {
			if (binary_search(mask.states_.begin(), mask.states_.end(), final_bit)) {
				rules.push_back(rule_index);
			}
		}
		if (!rules.empty()) {
			// Sort by priority (lower index = higher priority)
			std::sort(rules.begin(), rules.end());
			res[dfa_id] = rules;
		}
	}

	return res;
}
