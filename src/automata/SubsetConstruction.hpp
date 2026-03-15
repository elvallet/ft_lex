#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <cstdint>

namespace automata {

struct NFA
{
	int														initial_state_;
	std::vector<int>										final_states_;
	std::vector<std::unordered_map<char, std::vector<int>>>	transitions_;
	std::vector<std::vector<int>>							epsilon_transitions_;
	std::unordered_set<char>								alphabet_;
};

struct DFA {
	int											initial_state_;
	std::unordered_set<int>						final_states_;
	std::vector<std::unordered_map<char, int>>	transitions_;
};

class SubsetConstruction {
public:
	DFA		build(const NFA& nfa);
	void	complete(DFA& dfa, const NFA& nfa);

private:
	std::unordered_map<uint64_t, int>	seen_;

	uint64_t				epsilon_closure(const NFA& nfa, uint64_t states);
	uint64_t				delta(const NFA& nfa, uint64_t states, char symbol);
	std::unordered_set<int>	final_states(const NFA& nfa);
};

} // namespace automata