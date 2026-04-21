/**
 * @file Pipeline.cpp
 * @brief Implementation of the high-level multi-rule regex-to-DFA pipeline.
 */
#include "Pipeline.hpp"
#include <stdexcept>
#include <algorithm>

using namespace automata; using namespace std;

/**
 * @brief Compile all rules, merge NFAs, build DFA, then complete transitions.
 * @param rules Ordered lexer rules (priority = lowest index).
 * @return Fully constructed DFA for the full rule set.
 */
DFA ParsingPipeline::execute(
	const vector<lexer_file::Rule>& rules,
	const map<string, bool>& conditions)
{
	thompson_ = Thompson();
	subset_construction_ = SubsetConstruction();

	std::vector<NFA>	nfas;
	for (int i = 0; i < (int)rules.size(); ++i) {
		vector<Token> postfix	= parser_.parse(rules[i].pattern_ + rules[i].trailing_);
		nfas.push_back(thompson_.compile(postfix, i));
	}

	map<string, bool>	all_conditions	= conditions;
	all_conditions.insert({"INITIAL", false});

	vector<pair<string, vector<NFA>>>	groups;

	for (auto& [cond_name, is_exclusive] : all_conditions) {
		vector<NFA>	normal;
		vector<NFA>	bol;
		bool			has_bol_rule = false;

		for (int i = 0; i < (int)rules.size(); ++i) {
			auto&	rule_conds		= rules[i].conditions_;
			bool	rule_has_cond	= find(rule_conds.begin(), rule_conds.end(), cond_name) != rule_conds.end();
			bool	is_unqualified	= rule_conds.size() == 1 && rule_conds[0] == "INITIAL";
			bool	applies			= rule_has_cond || (!is_exclusive && is_unqualified && cond_name != "INITIAL");

			if (!applies)
				continue;

			if (rules[i].is_bol_) {
				has_bol_rule = true;
				bol.push_back(nfas[i]);
			}
			else {
				normal.push_back(nfas[i]);
				bol.push_back(nfas[i]);
			}
		}

		if (!normal.empty())
			groups.push_back({cond_name, normal});
		if (has_bol_rule && !bol.empty())
			groups.push_back({cond_name + "_BOL", bol});
	}

	auto [global_nfa, nfa_entry_points]	= merge_keyed(groups);

	DFA	dfa	= subset_construction_.build(global_nfa, nfa_entry_points);
	subset_construction_.complete(dfa, global_nfa);

	return dfa;
}

pair<NFA, map<string, int>> ParsingPipeline::merge_keyed(const vector<pair<string, vector<NFA>>>& groups)
{
	NFA	merged;
	map<string, int>	entry_points;
	int	offset	= 0;

	for (auto& [cond_name, nfas] : groups) {
		// One synthetic epsilon-only entry state is created per start condition.
		int	super_initial	= offset;
		merged.transitions_.push_back({});
		merged.epsilon_transitions_.push_back({});
		offset++;

		entry_points[cond_name]	= super_initial;

		for (const NFA& nfa : nfas) {
			int state_count	= (int)nfa.transitions_.size();

			merged.transitions_.resize(offset + state_count);
			merged.epsilon_transitions_.resize(offset + state_count);

			for (int state = 0; state < state_count; ++state) {
				for (auto& [sym, dests] : nfa.transitions_[state]) {
					for (int dest: dests)
						merged.transitions_[state + offset][sym].push_back(dest + offset);
				}
				for (int eps : nfa.epsilon_transitions_[state])
					merged.epsilon_transitions_[state + offset].push_back(eps + offset);
			}

			for (auto& [final_state, rule_idx] : nfa.final_states_)
				merged.final_states_[final_state + offset] = rule_idx;
			
			for (char c : nfa.alphabet_)
				merged.alphabet_.insert(c);

			// Wire condition entry to the shifted NFA initial state.
			merged.epsilon_transitions_[super_initial].push_back(nfa.initial_state_ + offset);

			offset += state_count;
		}
	}

	merged.initial_state_	= 0;

	return {merged, entry_points};
}

NFA ParsingPipeline::merge(const vector<NFA>& nfas)
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