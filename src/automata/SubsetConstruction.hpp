/**
 * @file SubsetConstruction.hpp
 * @brief NFA to DFA conversion using subset construction.
 */

#pragma once

#include <cstddef>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <string>

#include "NFA.hpp"
#include "DFA.hpp"

namespace automata {

/**
 * @brief Builds a DFA from an NFA and completes missing transitions.
 */
class SubsetConstruction {
public:
	/**
	 * @brief Canonical representation of an NFA state subset.
	 */
	struct StateSet {
		std::vector<int> states_;

		bool operator==(const StateSet& other) const noexcept { return states_ == other.states_; }
	};

	/**
	 * @brief Hash for StateSet.
	 */
	struct StateSetHash {
		std::size_t operator()(const StateSet& subset) const noexcept;
	};

	/**
	 * @brief Build a DFA from an NFA using subset construction.
	 * @param nfa Source NFA.
	 * @param entry_points Condition name -> NFA entry state used to seed subsets.
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
	/** @brief Mapping from NFA-state subset to DFA state id. */
	std::unordered_map<StateSet, int, StateSetHash>	seen_;

	/**
	 * @brief Compute epsilon-closure for a set of NFA states.
	 * @param nfa Source NFA.
	 * @param states Active NFA states.
	 * @return States reachable through epsilon transitions.
	 */
	StateSet			epsilon_closure(const NFA& nfa, const StateSet& states);
	/**
	 * @brief Apply one symbol transition step on a set of NFA states.
	 * @param nfa Source NFA.
	 * @param states Active NFA states.
	 * @param symbol Input symbol.
	 * @return Destination states.
	 */
	StateSet			delta(const NFA& nfa, const StateSet& states, char symbol);
	/**
	 * @brief Compute DFA accepting states from discovered subsets.
	 * @param nfa Source NFA.
	 * @return Map of DFA accepting state id -> vector of matching rule indices (sorted ascending).
	 *
	 * When a subset contains multiple NFA final states, all matching rule indices are collected
	 * and stored in ascending order (index 0 = highest priority). This allows the runtime to
	 * examine all possibilities and apply the priority-based selection at dispatch time.
	 */
	std::unordered_map<int, std::vector <int>>	final_states(const NFA& nfa);
};

} // namespace automata