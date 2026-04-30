/**
 * @file DFA.hpp
 * @brief Deterministic finite automaton representation.
 */

#pragma once

#include <vector>
#include <unordered_map>
#include <map>

namespace automata {

/**
 * @brief Deterministic finite automaton.
 * 
 * Represents a complete DFA with transition tables and acceptance information,
 * supporting multiple start conditions and BOL (beginning-of-line) variants.
 */
struct DFA {
	/** @brief Initial DFA state id (used by INITIAL condition). */
	int											initial_state_;
	/** @brief Accepting DFA states: state id -> vector of matching rule indices (sorted by priority, ascending). */
	std::unordered_map<int, std::vector<int>>	final_states_;
	/** @brief Transition table: state -> (symbol -> next state). */
	std::vector<std::unordered_map<char, int>>	transitions_;
	/** @brief Start condition entry states: condition name -> DFA state id.
	 *  Includes both normal conditions (INITIAL, COMMENT, etc.) and BOL variants (INITIAL_BOL, COMMENT_BOL, etc.).
	 */
	std::map<std::string, int>					start_states_;
	int											sink_;
};

} // namespace automata