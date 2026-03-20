/**
 * @file NFA.hpp
 * @brief Non-deterministic finite automaton representation.
 */

#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace automata {

/**
 * @brief Non-deterministic finite automaton with epsilon transitions.
 */
struct NFA
{
	/** @brief Initial NFA state id. */
	int														initial_state_;
	/** @brief Accepting NFA states. */
	std::vector<int>										final_states_;
	/** @brief Symbol transitions: state -> (symbol -> destination states). */
	std::vector<std::unordered_map<char, std::vector<int>>>	transitions_;
	/** @brief Epsilon transitions: state -> destination states. */
	std::vector<std::vector<int>>							epsilon_transitions_;
	/** @brief Input alphabet used by symbol transitions. */
	std::unordered_set<char>								alphabet_;
};

} // namespace automata