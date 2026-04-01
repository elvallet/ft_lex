/**
 * @file Pipeline.hpp
 * @brief High-level multi-rule regex parsing/build pipeline.
 */
#pragma once

#include <string>

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
	 * @return Complete DFA generated from all rules.
	 */
	DFA execute(const std::vector<lexer_file::Rule>& rules);

private:
	/**
	 * @brief Merge multiple per-rule NFAs into one NFA with a shared start state.
	 * @param nfas Compiled NFAs, one per rule.
	 * @return Merged NFA preserving per-rule accepting-state indices.
	 */
	NFA merge(const std::vector<NFA>& nfas);

	/** @brief Regex parser instance. */
	Parser				parser_;
	/** @brief Thompson NFA builder instance. */
	Thompson			thompson_;
	/** @brief Subset-construction DFA builder instance. */
	SubsetConstruction	subset_construction_;
};

} // namespace automata
