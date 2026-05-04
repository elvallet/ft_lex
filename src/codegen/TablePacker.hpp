#pragma once

#include <vector>
#include <unordered_map>

namespace codegen {

/**
 * @brief Packed DFA transition tables for compressed scanner output.
 * 
 * Replaces the dense yytable[state][256] with four 1D arrays.
 * Lookup: if check[base[s] + c] == s, next state is next[base[s] + c],
 * otherwise the transition is invalid (-1).
 */
struct PackedTables {
	std::vector<int>	base_;	///< Per-state offset into next/check.
	std::vector<int>	next_;	///< Transition destinations, compacted.
	std::vector<int>	check_;	///< Owner state of each next[] slot.
	std::vector<int>	def_;	///< Default fallback per state.
};

/**
 * @brief Packs a sparse DFA transition table into compressed 1D arrays.
 * 
 * Uses a greedy offset-search algorithm: states are stored by number of
 * outgoiing transitions (densets first), then each state is placed at the
 * lowest offset in next[]/check[] where none of its transitions collide
 * with already-placed strates.
 */
class TablePacker {
public:
	TablePacker();

	/**
	 * @brief Run the packing algorithm.
	 * @param transitions DFA transition table (state -> char -> next_state).
	 * @return PackedTables ready for codegen serialisation
	 */
	PackedTables	pack(const std::vector<std::unordered_map<char, int>>& transitions);

private:
	/// Ordered list of (unsigned_char_value, destination) for one state.
	using Profile = std::vector<std::pair<int, int>>;

	/// Build the profile of a single state, casting chars to unsigned.
	Profile	build_profile(const std::unordered_map<char, int>& row) const;

	/// Return true if placing `profile`, starting at `offset` would collide
	/// with an already_placed state in check_[].
	bool	collides(int offset, const Profile& profile) const;

	/// Find the lowest valid offset for `profile`, starting from low_water_.
	int		find_offset(const Profile& profile);

	/// Write a state's transitions into next_[]/check_[] at the given offset,
	/// record base_[state], and advance low_water_.
	void	place(int state, int offset, const Profile& profile);

	/// Grow next_[] and check_[] to at least `needed` entries, filling new
	/// slots with -1.
	void	ensure_size(size_t needed);

	int					low_water_;
	std::vector<int>	base_;
	std::vector<int>	next_;
	std::vector<int>	check_;
};

} // namespace codegen