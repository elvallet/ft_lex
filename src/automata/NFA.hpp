#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace automata {

struct NFA
{
	int														initial_state_;
	std::vector<int>										final_states_;
	std::vector<std::unordered_map<char, std::vector<int>>>	transitions_;
	std::vector<std::vector<int>>							epsilon_transitions_;
	std::unordered_set<char>								alphabet_;
};

} // namespace automata