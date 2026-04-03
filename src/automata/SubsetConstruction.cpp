/**
 * @file SubsetConstruction.cpp
 * @brief Implementation of subset construction and DFA completion.
 */

#include "SubsetConstruction.hpp"
#include <stack>
#include <climits>

using namespace automata; using namespace std;

/**
 * @brief Build a DFA from an NFA using subset construction.
 * @param nfa Source NFA.
 * @return Equivalent DFA.
 */
DFA SubsetConstruction::build(const NFA& nfa) {
	DFA				dfa;
	stack<uint64_t>	worklist;
	int				id = 0;

	uint64_t S = epsilon_closure(nfa, (1ULL << nfa.initial_state_));
	seen_.insert({S, id});
	dfa.initial_state_ = id++;
	worklist.push(S);

	while (!worklist.empty()) {
		uint64_t S = worklist.top();
		worklist.pop();
		for (char symbol : nfa.alphabet_) {
			uint64_t T = epsilon_closure(nfa, delta(nfa, S, symbol));
			if (T != 0) {
				if (seen_.find(T) == seen_.end()) {
					seen_.insert({T, id++});
					worklist.push(T);
				}
				int S_id = seen_[S];
				while (dfa.transitions_.size() <= static_cast<size_t>(S_id))
					dfa.transitions_.push_back({});
				dfa.transitions_[S_id][symbol] = seen_[T];
			}
		}
	}
	dfa.final_states_ = final_states(nfa);

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
uint64_t SubsetConstruction::epsilon_closure(const NFA& nfa, uint64_t states) {
	uint64_t	res = 0;
	stack<int>	worklist;

	uint64_t tmp = states;
	while (tmp) {
		int state = __builtin_ctzll(tmp);
		worklist.push(state);
		tmp &= tmp - 1;
	}

	while (!worklist.empty()) {
		int x = worklist.top();
		worklist.pop();
		res |= (1ULL << x);
		for (auto e : nfa.epsilon_transitions_[x]) {
			if (!(res & (1ULL << e)))
				worklist.push(e);
		}
	}

	return res;
}

/**
 * @brief Apply one symbol move to a set of NFA states.
 * @param nfa Source NFA.
 * @param states Input state bitmask.
 * @param symbol Transition symbol.
 * @return Destination states bitmask.
 */
uint64_t SubsetConstruction::delta(const NFA& nfa, uint64_t states, char symbol) {
	uint64_t	res = 0;
	uint64_t	tmp = states;

	while (tmp) {
		int state = __builtin_ctzll(tmp);
		auto it = nfa.transitions_[state].find(symbol);
		if (it != nfa.transitions_[state].end()) {
			for (auto i : it->second)
				res |= (1ULL << i);
		}
		tmp &= tmp - 1;
	}

	return res;
}

/**
 * @brief Derive accepting DFA states from discovered NFA subsets.
 * @param nfa Source NFA.
 * @return Map of accepting DFA state ids to the selected rule index.
 *
 * If several rule finals are present in one subset, the smallest rule index
 * is kept.
 */
unordered_map<int, int> SubsetConstruction::final_states(const NFA& nfa) {
	unordered_map<int, int> res;

	for (auto& [mask, dfa_id] : seen_) {
		int min_rule = INT_MAX;
		for (auto& [final_bit, rule_index] : nfa.final_states_) {
			if (mask & (1ULL << final_bit)) {
				min_rule = min(min_rule, rule_index);
			}
		}
		if (min_rule != INT_MAX) {
			res[dfa_id] = min_rule;
		}
	}

	return res;
}
