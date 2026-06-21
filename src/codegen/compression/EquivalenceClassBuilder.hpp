#pragma once

#include <vector>
#include "DenseRows.hpp"

namespace codegen {

/**
 * @brief Result of grouping equivalent alphabet symbols into classes.
 */
struct EquivalenceClasses {
	std::vector<int>	class_of;	///< class_of[c] for every byte value 0..255.
	DenseRows			reduced;	///< Dense rows re-indexed by class instead of raw symbol.
};

/**
 * @brief Group alphabet symbols that transition identically across every state.
 * 
 * Two columns (symbols) are equivalent if dense.rows[s][col] is identical
 * for every state s. Equivalent columns collapse into a single class,
 * shrinking next[]/check[] proportionally to the alphabet reduction.
 * 
 * Bytes that never appear in the grammar's alphabet (dense.symbols) are
 * mapped to an extra "phantom" class -- id == numb er of real classes --
 * that no state's profile ever references. This preserves the existing runtime
 * guarantee that an unmatched check[] slot always falls through to -1,
 * for every possible input byte, not juste the ones the grammar uses.
 * 
 * @param dense dense rows, one column per grammar symbol.
 * @return class_of[256] (covers the full byte range) and a DenseRows
 * 		   reduced to one column per equivalence class.
 */
EquivalenceClasses build_equivalence_classes(const DenseRows& dense);

} // namespace codegen