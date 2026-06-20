/**
 * @file Stats.hpp
 * @brief Pipeline statistics collected during lexer compilation.
 */
#pragma once

#include <cstddef>
#include <string>

namespace automata {

/**
 * @brief Counters and measurements gathered by the compilation pipeline.
 *
 * Fields are populated incrementally: lexer-file counts first, then NFA and
 * DFA counts after each build step, and table sizes after codegen.
 */
struct Stats {
	bool	compression_enabled = false;

	/** @brief Number of macro definitions parsed from the lexer file. */
	int		macros_count = 0;
	/** @brief Number of rules parsed from the lexer file. */
	int		rules_count = 0;
	/** @brief Number of declared start conditions (excluding INITIAL). */
	int		start_conditions_count = 0;

	/** @brief Total NFA states after merging all per-rule NFAs. */
	int		nfa_states = 0;

	/** @brief Total DFA states after subset construction. */
	int		dfa_states = 0;
	/** @brief Number of sink states added during DFA completion (0 or 1). */
	int		dfa_sink_states = 0;

	/** @brief Raw transition table size in entries (states × 256). */
	size_t	table_size_raw = 0;
	/** @brief Packed table size in entries (equals raw when compression is off). */
	size_t	table_size_packed = 0;
	/** @brief Ratio packed/raw (1.0 when compression is off). */
	float	compression_ratio = 0.0f;

	/** @brief Path of the generated output file, or "/dev/stdout". */
	std::string	output_file;
	/** @brief Size in bytes of the generated source file. */
	size_t		output_bytes = 0;
};

} // namespace automata