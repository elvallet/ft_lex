/**
 * @file DFA.hpp
 * @brief Deterministic finite automaton representation.
 */

#pragma once

#include <vector>
#include <unordered_map>

namespace automata {

/**
 * @brief Deterministic finite automaton.
 */
struct DFA {
	/** @brief Initial DFA state id. */
	int											initial_state_;
	/** @brief Accepting DFA states: state id -> selected rule index. */
	std::unordered_map<int, int>				final_states_;
	/** @brief Transition table: state -> (symbol -> next state). */
	std::vector<std::unordered_map<char, int>>	transitions_;
};

} // namespace automata