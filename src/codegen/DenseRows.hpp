#pragma once

#include <vector>
#include <unordered_map>

namespace codegen {

/**
 * @brief Dense, position-indexed view of a DFA transition table.
 * 
 * Converts the sparse per-state `unordered_map<char, int>` rows into
 * uniform `vector<int>` rows indexed by column position (not by raw
 * character value), with sink transition normalised to -1. This is
 * the shared input format for DefaultChainBuilder (similarity/Prim)
 * and the diff-profile computation that follows it.
 */
struct DenseRows {
	std::vector<std::vector<int>>	rows;		///< rows[s][col] = destination state, or -1.
	std::vector<char>				symbols;	///< symbols[col] = the real character for that column.  
};

/**
 * @brief Build a DenseRows view from a DFA's transition table.
 * @param transitions per-state transition maps (post SubsetConstruction::complete()).
 * @param sink DFA's sink state id (or -1 if no sink was needed).
 * @return DenseRows with sink transitions normalised to -1.
 * 
 * Precondition: every non-empty row share the same key set (true after
 * complete() has run). Throws std::out_of_range if a row is missing a
 * column that another row defines -- this signals an inconsistent DFA
 * rather than something to silently paper over.
 */
DenseRows	build_dense_rows(const std::vector<std::unordered_map<char, int>>& transitions, int sink);

} // namespace codegen