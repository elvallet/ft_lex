#pragma once

#include <vector>
#include <cstddef>

namespace codegen {

/**
 * @brief Builds a maximum spanning tree of default[] links between DFA states.
 * 
 * Each state is compared to every other state via a similarity score
 * (number of symbols where both states transition identically). Prim's
 * algorithm grows a tree over these scores; the resulting parent[] array
 * is later used to compute diff-only transition profiles, which TablePacker
 * places into next[]/check[] instead of full rows.
 */
class DefaultChainBuilder {
public:
	/**
	 * @param rows One dense row per state: rows[s][i] = destination state
	 * 			   for the i-th alphabet symbol (sink included). All rows
	 * 			   must share the same length.	 
	 * @param size Number of states (== rows.size())
	 */
	DefaultChainBuilder(std::vector<std::vector<int>> rows, size_t size);

	/**
	 * @brief Run Prim's algorithm to build the default[] chain.
	 * @param initial_state Root of the spanning tree (no parent, default = -1).
	 * @return parent[s] = default[] target for state s, or -1 for the root.
	 */
	std::vector<int>	prim(int initial_state);

private:
	/// Number of alphabet symbols matching where row[A] == row[B].
	int		sim(int A, int B);

	/// Relax state's best known connection to the tree given a newly added member.
	void	update_state(int last_added, int state);

	std::vector<std::vector<int>>	rows_;
	size_t							nb_states_;
	size_t							alphabet_size_;

	std::vector<bool>				in_tree_;
	std::vector<int>				best_score_;	/// < -1 = no known connection yet.
	std::vector<int>				best_source_;
};

} // namespace codegen