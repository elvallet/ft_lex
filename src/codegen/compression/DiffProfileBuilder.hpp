#pragma once

#include <vector>
#include "TablePacker.hpp"
#include "DenseRows.hpp"

namespace codegen {

/**
 * @brief Build a diff-only Profile per state, relative to its default[] parent.
 * 
 * The root (parent == -1) is diffed against an implicit "all -1" row --
 * i.e. it keeps every non-sink transition, exactly like the legacy
 * used without default[]. Non-root states keep only the
 * symbols where they disagree with their parent, sink (-1) included.
 * 
 * @param dense Dense, position-indexed rows (see build_dense_rows).
 * @param parent default[] chain, parent[s] = -1 for the root.
 * @return One Profile per state, ready for TablePacker::pack_profiles().
 */
std::vector<Profile>	build_diff_profiles(const DenseRows& dense, const std::vector<int>& parent);

} // namespace codegen