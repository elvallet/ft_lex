#include "SubsetConstruction.hpp"
#include <stack>


using namespace automata; using namespace std;

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
	return dfa;
}

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

unordered_set<int> SubsetConstruction::final_states(const NFA& nfa) {
	unordered_set<int>	res;

	for (auto& [mask, dfa_id] : seen_) {
		for (auto f : nfa.final_states_) {
			if (mask & (1ULL << f)) {
				res.insert(dfa_id);
				break;
			}
		}
	}

	return res;
}
