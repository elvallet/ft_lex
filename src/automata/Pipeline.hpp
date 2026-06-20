/**
 * @file Pipeline.hpp
 * @brief High-level multi-rule regex parsing/build pipeline.
 */
#pragma once

#include <string>
#include <map>

#include "Stats.hpp"
#include "Parser.hpp"
#include "Thompson.hpp"
#include "SubsetConstruction.hpp"
#include "../lexer_file/LexFile.hpp"

namespace automata {

/**
 * @brief Wraps parser and automata builders into a single execution API.
 *
 * Each rule pattern is compiled to an NFA, all NFAs are merged under a fresh
 * common start state, then converted to one DFA.
 */
class ParsingPipeline {
public:
	/**
	 * @brief Execute the full pipeline from rules to complete DFA.
	 * @param rules Ordered lexer rules (priority = lowest index).
	 * @param conditions Declared start conditions (`%s` and `%x`) from lexer file.
	 * @return Complete DFA generated from all rules.
	 */
	DFA execute(std::vector<lexer_file::Rule>& rules, const std::map<std::string, bool>& conditions);

	/** @brief Access mutable pipeline statistics. */
	Stats& stats();
	/** @brief Access read-only pipeline statistics. */
	const Stats& stats() const;

	/** @brief Print a human-readable statistics summary to stderr. */
	void print_stats();

private:
	/**
	 * @brief Merge multiple per-rule NFAs into one NFA with a shared start state.
	 * @param nfas Compiled NFAs, one per rule.
	 * @return Merged NFA preserving per-rule accepting-state indices.
	 */
	NFA merge(const std::vector<NFA>& nfas);
	/**
	 * @brief Merge NFA groups by start condition and expose each condition entry point.
	 * @param groups Pairs of `<condition_name, list of NFAs active in that condition>`.
	 * @return Merged NFA and per-condition NFA entry-state ids.
	 */
	std::pair<NFA, std::map<std::string, int>>	merge_keyed(const std::vector<std::pair<std::string, std::vector<NFA>>>& groups);

	/** @brief Regex parser instance. */
	Parser				parser_;
	/** @brief Thompson NFA builder instance. */
	Thompson			thompson_;
	/** @brief Subset-construction DFA builder instance. */
	SubsetConstruction	subset_construction_;
	/** @brief Execution statistics accumulated during the pipeline. */
	Stats				stats_;
};

} // namespace automata
