/**
 * @file Pipeline.cpp
 * @brief Implementation of the high-level regex-to-DFA pipeline.
 */
#include "Pipeline.hpp"

using namespace automata;

/**
 * @brief Run parser, Thompson construction, subset construction, and completion.
 * @param regex Regex expression in infix notation.
 * @return Fully constructed DFA.
 */
DFA ParsingPipeline::execute(const std::string& regex) {
	thompson_ = Thompson();
	subset_construction_ = SubsetConstruction();

	std::vector<Token> postfixe = parser_.parse(regex);
	NFA nfa = thompson_.compile(postfixe);
	DFA dfa = subset_construction_.build(nfa);
	subset_construction_.complete(dfa, nfa);

	return dfa;
}
