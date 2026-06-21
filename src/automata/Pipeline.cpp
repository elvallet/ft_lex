/**
 * @file Pipeline.cpp
 * @brief Implementation of the high-level multi-rule regex-to-DFA pipeline.
 */
#include "Pipeline.hpp"
#include <stdexcept>
#include <algorithm>
#include <iomanip>
#include <iostream>

using namespace automata; using namespace std;

namespace {

bool is_trivially_empty_action(const std::string& action)
{
	std::string compact;
	for (char c : action) {
		if (!std::isspace(static_cast<unsigned char>(c)))
			compact.push_back(c);
	}
	return compact == "{}";
}

} // namespace

void ParsingPipeline::print_stats()
{
	std::cerr << "Statistics" << std::endl;
	std::cerr << "  LexFile" << std::endl;
	std::cerr << "    macros: " << stats_.macros_count << std::endl;
	std::cerr << "    rules: " << stats_.rules_count << std::endl;
	std::cerr << "    start conditions: " << stats_.start_conditions_count << std::endl;
	std::cerr << "  NFA" << std::endl;
	std::cerr << "    states: " << stats_.nfa_states << std::endl;
	std::cerr << "    epsilon states: " << stats_.nfa_epsilon_states
	          << " / epsilon transitions: " << stats_.nfa_epsilon_transitions << std::endl;
	std::cerr << "    accepting states: " << stats_.nfa_final_states << std::endl;
	std::cerr << "  DFA" << std::endl;
	std::cerr << "    states: " << stats_.dfa_states;
	if (stats_.dfa_sink_states > 0)
		std::cerr << " (including " << stats_.dfa_sink_states << " sink)";
	std::cerr << std::endl;
	std::cerr << "    accepting states: " << stats_.dfa_final_states << std::endl;
	std::cerr << "    alphabet size: " << stats_.dfa_alphabet_size << std::endl;
	std::cerr << "    transitions: " << stats_.dfa_transitions << std::endl;
	std::cerr << "  Tables" << std::endl;
	std::cerr << "    raw size: " << stats_.table_size_raw << std::endl;
	std::cerr << "    packed size: " << stats_.table_size_packed << std::endl;
	if (stats_.compression_enabled) {
		float factor = stats_.table_size_packed == 0
			? 0.0f
			: static_cast<float>(stats_.table_size_raw) / static_cast<float>(stats_.table_size_packed);
		float saved_percent = stats_.table_size_raw == 0
			? 0.0f
			: (1.0f - static_cast<float>(stats_.table_size_packed) / static_cast<float>(stats_.table_size_raw)) * 100.0f;
		std::cerr << std::fixed << std::setprecision(3)
			<< "    compression factor: " << factor << "x (" << saved_percent << "%)" << std::endl;
		std::cerr.unsetf(std::ios::floatfield);
		std::cerr << "      base/def: " << stats_.comp_base_size << " entries each" << std::endl;
		std::cerr << "      next/check: " << stats_.comp_next_size << " entries each" << std::endl;
		std::cerr << "      empty slots: " << stats_.comp_empty_entries << std::endl;
		std::cerr << "      prototype states: " << stats_.comp_prototype_states << std::endl;
	} else {
		std::cerr << "    compression: disabled" << std::endl;
	}
	std::cerr << "  Output" << std::endl;
	std::cerr << "    file: " << (stats_.output_file.empty() ? "(stdout)" : stats_.output_file) << std::endl;
	std::cerr << "    bytes: " << stats_.output_bytes << std::endl;
}

Stats& ParsingPipeline::stats()
{
	return stats_;
}

const Stats& ParsingPipeline::stats() const
{
	return stats_;
}

/**
 * @brief Compile all rules, merge NFAs, build DFA, then complete transitions.
 * @param rules Ordered lexer rules (priority = lowest index).
 * @return Fully constructed DFA for the full rule set.
 */
DFA ParsingPipeline::execute(
	vector<lexer_file::Rule>& rules,
	const map<string, bool>& conditions)
{
	thompson_ = Thompson();
	subset_construction_ = SubsetConstruction();
	stats_.nfa_states = 0;
	stats_.dfa_states = 0;
	stats_.dfa_sink_states = 0;
	stats_.table_size_raw = 0;
	stats_.table_size_packed = 0;
	stats_.compression_ratio = 0.0f;

	vector<DFA>	trailing_dfas;
	int count = 0;
	for (size_t i = 0; i < rules.size(); ++i) {
		if (rules[i].trailing_is_variable_) {
			NFA nfa = thompson_.compile(parser_.parse(rules[i].trailing_), i);
			SubsetConstruction sc;
			DFA dfa = sc.build(nfa, {{"INITIAL", nfa.initial_state_}});
			trailing_dfas.push_back(dfa);
			rules[i].trailing_dfa_id_ = count++;
		}
	}

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
			bool	inherits_global	= is_unqualified && (!is_exclusive || is_trivially_empty_action(rules[i].action_));
			bool	applies			= rule_has_cond || inherits_global;

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
	stats_.nfa_states = static_cast<int>(global_nfa.transitions_.size());
	stats_.nfa_epsilon_states = 0;
	stats_.nfa_epsilon_transitions = 0;
	for (auto& eps : global_nfa.epsilon_transitions_) {
		if (!eps.empty()) {
			++stats_.nfa_epsilon_states;
			stats_.nfa_epsilon_transitions += static_cast<int>(eps.size());
		}
	}
	stats_.nfa_final_states = static_cast<int>(global_nfa.final_states_.size());

	DFA	dfa	= subset_construction_.build(global_nfa, nfa_entry_points);
	subset_construction_.complete(dfa, global_nfa);
	stats_.dfa_states = static_cast<int>(dfa.transitions_.size());
	stats_.dfa_sink_states = dfa.sink_ >= 0 ? 1 : 0;
	stats_.table_size_raw = static_cast<size_t>(stats_.dfa_states) * 256;
	stats_.dfa_final_states = static_cast<int>(dfa.final_states_.size());
	stats_.dfa_alphabet_size = static_cast<int>(global_nfa.alphabet_.size());
	stats_.dfa_transitions = 0;
	for (auto& row : dfa.transitions_) {
		for (auto& [c, dest] : row) {
			if (dest != dfa.sink_)
				++stats_.dfa_transitions;
		}
	}
	dfa.trailing_dfas_ = trailing_dfas;

	return dfa;
}

/**
 * @brief Merge NFA groups for each start condition into one shared NFA.
 * @param groups Pairs of `<condition_name, list of NFAs active in that condition>`.
 * @return Merged NFA and per-condition entry state ids.
 */
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

/**
 * @brief Merge multiple per-rule NFAs into one NFA under a fresh common start state.
 * @param nfas Compiled NFAs, one per rule.
 * @return Merged NFA where state 0 is the shared entry point.
 */
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