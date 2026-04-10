/**
 * @file SubsetConstruction.hpp
 * @brief NFA to DFA conversion using subset construction.
 */

#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>

#include "NFA.hpp"
#include "DFA.hpp"

namespace automata {

/**
 * @brief Builds a DFA from an NFA and completes missing transitions.
 */
class SubsetConstruction {
public:
	/**
	 * @brief Build a DFA from an NFA using subset construction.
	 * @param nfa Source NFA.
	 * @return Equivalent DFA (possibly incomplete).
	 */
	DFA		build(const NFA& nfa, const std::map<std::string, int>& entry_points);
	/**
	 * @brief Complete missing transitions by adding a sink state when required.
	 * @param dfa DFA to complete.
	 * @param nfa Source NFA used to recover the alphabet.
	 */
	void	complete(DFA& dfa, const NFA& nfa);

private:
	/** @brief Mapping from NFA-state bitmask to DFA state id. */
	std::unordered_map<uint64_t, int>	seen_;

	/**
	 * @brief Compute epsilon-closure for a set of NFA states.
	 * @param nfa Source NFA.
	 * @param states Bitmask of active NFA states.
	 * @return Bitmask of states reachable through epsilon transitions.
	 */
	uint64_t				epsilon_closure(const NFA& nfa, uint64_t states);
	/**
	 * @brief Apply one symbol transition step on a set of NFA states.
	 * @param nfa Source NFA.
	 * @param states Bitmask of active NFA states.
	 * @param symbol Input symbol.
	 * @return Bitmask of destination states.
	 */
	uint64_t				delta(const NFA& nfa, uint64_t states, char symbol);
	/**
	 * @brief Compute DFA accepting states from discovered subsets.
	 * @param nfa Source NFA.
	 * @return Map of DFA accepting state id -> selected rule index.
	 *
	 * When a subset contains multiple NFA final states, the smallest rule index
	 * is selected to preserve lexer rule priority.
	 */
	std::unordered_map<int, int>	final_states(const NFA& nfa);
};

} // namespace automata