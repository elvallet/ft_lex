/**
 * @file Pipeline.hpp
 * @brief High-level regex parsing/build pipeline.
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
 */
class ParsingPipeline {
public:
	/**
	 * @brief Execute the full regex pipeline from input regex to complete DFA.
	 * @param regex Regex expression in infix form.
	 * @return Complete DFA generated from the regex.
	 */
	DFA execute(const std::vector<lexer_file::Rule>& rules);

private:
	NFA merge(const std::vector<NFA>& nfas);

	/** @brief Regex parser instance. */
	Parser				parser_;
	/** @brief Thompson NFA builder instance. */
	Thompson			thompson_;
	/** @brief Subset-construction DFA builder instance. */
	SubsetConstruction	subset_construction_;
};

} // namespace automata
