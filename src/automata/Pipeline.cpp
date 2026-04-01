/**
 * @file Pipeline.cpp
 * @brief Implementation of the high-level regex-to-DFA pipeline.
 */
#include "Pipeline.hpp"
#include <stdexcept>

using namespace automata;

/**
 * @brief Run parser, Thompson construction, subset construction, and completion.
 * @param regex Regex expression in infix notation.
 * @return Fully constructed DFA.
 */
DFA ParsingPipeline::execute(const std::vector<lexer_file::Rule>& rules) {
	thompson_ = Thompson();
	subset_construction_ = SubsetConstruction();
	std::vector<NFA>	nfas;
	int	indice = 0;

	for (auto rule: rules) {
		std::vector<Token> postfixe = parser_.parse(rule.pattern_);
		NFA n	= thompson_.compile(postfixe, indice);
		nfas.push_back(n);
		indice++;
	}

	NFA nfa	= merge(nfas);
	DFA dfa = subset_construction_.build(nfa);
	subset_construction_.complete(dfa, nfa);

	return dfa;
}

NFA ParsingPipeline::merge(const std::vector<NFA>& nfas)
{
	if (nfas.empty())
		throw std::runtime_error("No NFA to merge");

	NFA merged;
	merged.initial_state_ = 0;
	merged.transitions_.push_back({});
	merged.epsilon_transitions_.push_back({});

	int offset = 1;
	for (const NFA& nfa : nfas) {
		const int state_count = static_cast<int>(nfa.transitions_.size());

		merged.transitions_.resize(offset + state_count);
		merged.epsilon_transitions_.resize(offset + state_count);

		for (int state = 0; state < state_count; ++state) {
			for (const auto& transition : nfa.transitions_[state]) {
				char symbol = transition.first;
				std::vector<int> destinations;
				for (int destination : transition.second)
					destinations.push_back(destination + offset);
				merged.transitions_[state + offset][symbol] = destinations;
			}

			for (int epsilon_destination : nfa.epsilon_transitions_[state])
				merged.epsilon_transitions_[state + offset].push_back(epsilon_destination + offset);
		}

		for (const auto& final_state : nfa.final_states_)
			merged.final_states_[final_state.first + offset] = final_state.second;

		for (char symbol : nfa.alphabet_)
			merged.alphabet_.insert(symbol);

		merged.epsilon_transitions_[merged.initial_state_].push_back(nfa.initial_state_ + offset);
		offset += state_count;
	}

	return merged;
}