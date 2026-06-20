#include "CodegenRust.hpp"
#include <algorithm>
#include <limits>
#include <tuple>

using namespace codegen;

/**
 * @brief Generate the full Rust scanner source by assembling all sections.
 * @param dfa Deterministic automaton.
 * @param lexfile Parsed lexer file.
 * @param out Output stream.
 * @param stats Optional statistics struct (updated in place).
 * @return Number of bytes written to @p out.
 */
size_t CodegenRust::generate(const automata::DFA& dfa, const lexer_file::LexFile& lexfile, std::ostream& out, automata::Stats* stats)
{
	std::ostringstream	oss;
	write_prologue(lexfile, oss);
	write_tables(dfa, lexfile, oss, stats);
	write_generated_lexer(dfa, lexfile, oss);
	write_epilogue(lexfile, oss);
	std::string generated = oss.str();
	if (stats) {
		stats->table_size_raw = static_cast<size_t>(dfa.transitions_.size()) * 256;
		stats->table_size_packed = stats->table_size_raw;
		stats->compression_ratio = 0.0f;
		stats->output_bytes = generated.size();
	}
	out << generated;
	return generated.size();
}

/**
 * @brief Emit `use` imports and the top verbatim block from the lexer file.
 */
void CodegenRust::write_prologue(const lexer_file::LexFile& lexfile, std::ostringstream& oss)
{
	oss << "use ftlex_runtime::{LexerDef, Scanner};" << std::endl;
	oss << "use std::io::Write;\n" << std::endl;
	
	oss << lexfile.verbatim_top_ << std::endl;
}

/**
 * @brief Emit start-condition constants, transition table, and accept tables as Rust statics.
 * @param dfa Deterministic automaton.
 * @param lexfile Parsed lexer file (provides rule trailing info).
 * @param oss Output string stream being assembled.
 * @param stats Optional statistics struct updated with raw table size.
 */
void CodegenRust::write_tables(const automata::DFA& dfa, const lexer_file::LexFile& lexfile, std::ostringstream& oss, automata::Stats* stats)
{
	const size_t	nb_states	= dfa.transitions_.size();
	if (stats) {
		stats->table_size_raw = nb_states * 256;
		stats->table_size_packed = stats->table_size_raw;
		stats->compression_ratio = 0.0f;
	}

	std::vector<int>			ids;
	std::vector<std::string>	base_conditions;

	oss << "pub const INITIAL: usize = 0;\n";
	ids.push_back(dfa.initial_state_);
	base_conditions.push_back("INITIAL");
	int	count	= 1;

	for (auto& [name, id] : dfa.start_states_) {
		if (name == "INITIAL" || name.find("_BOL") != std::string::npos)
			continue;
		oss << "pub const " << name << ": usize = " << count << ";\n";
		ids.push_back(id);
		base_conditions.push_back(name);
		count++;
	}

	oss << "pub const YYNB_CONDITIONS: usize = " << count << ";\n";

	for (const std::string& cond : base_conditions) {
		std::string bol = cond + "_BOL";
		auto it = dfa.start_states_.find(bol);
		ids.push_back(it != dfa.start_states_.end()
			? it->second
			: dfa.start_states_.at(cond));
	}

	oss << std::endl;

	oss << "static YYSTART_STATES: [usize; " << ids.size() << "] = [ ";
	for (size_t i = 0; i < ids.size(); i++) {
		oss << (i == 0 ? " " : ", ") << ids[i];
	}
	oss << " ];\n\n";

	oss << "static YYTABLE: [[i32; 256]; " << nb_states << "] = [\n";
	for (size_t s = 0; s < nb_states; s++) {
		oss << "\t[";
		for (int c = 0; c < 256; c++) {
			auto it = dfa.transitions_[s].find(static_cast<char>(c));
			oss << (c == 0 ? " " : ", ")
				<< (it != dfa.transitions_[s].end() ? it->second : -1);
		}
		oss << " ],\n";
	}
	oss << "];\n\n";

	std::vector<int>	offsets(nb_states, -1);
	std::vector<int>	counts(nb_states, 0);
	std::vector<std::tuple<int, int, int>>	flat;

	for (size_t s = 0; s < nb_states; s++) {
		auto found = dfa.final_states_.find(static_cast<int>(s));
		if (found == dfa.final_states_.end() || found->second.empty())
			continue;

		std::vector<int>	sorted = found->second;
		std::sort(sorted.begin(), sorted.end());

		offsets[s]	= static_cast<int>(flat.size());
		counts[s]	= static_cast<int>(sorted.size());

		for (int rule_id : sorted) {
			int tlen	= -1;
			int tdfa_id	= -1;
			if (rule_id >= 0 && static_cast<size_t>(rule_id) < lexfile.rules_.size()
				&& !lexfile.rules_[rule_id].trailing_.empty()) {
					tlen	= lexfile.rules_[rule_id].trailing_length_;	
					if (lexfile.rules_[rule_id].trailing_is_variable_)
						tdfa_id = lexfile.rules_[rule_id].trailing_dfa_id_;
				}
				flat.push_back({rule_id, tlen, tdfa_id});
		}
	}

	oss << "static YYACCEPT_DATA: &[(i32, i32, i32)] = &[\n";
	if (flat.empty()) {
		oss << "\t(-1, -1),\n";
	} else {
		for (auto& [rid, tlen, tdfa_id] : flat) {
			oss << "\t( " << rid << ", " << tlen << ", " << tdfa_id << "),\n";
		}
	}
	oss << "];\n";

	oss << "static YYACCEPT_OFFSET: [i32; " << nb_states << "] = [";
	for (size_t s = 0; s < nb_states; s++) {
		oss << (s == 0 ? " " : ", ") << offsets[s];
	}
	oss << "];\n";

	oss << "static YYACCEPT_COUNT: [usize; " << nb_states << "] = [";
	for (size_t s = 0; s < nb_states; s++) {
		oss << (s == 0 ? " " : ", ") << counts[s];
	}
	oss << " ];\n";

	for (size_t i = 0; i < dfa.trailing_dfas_.size(); ++i) {
		const automata::DFA&	tdfa		= dfa.trailing_dfas_[i];
		size_t					tsize		= tdfa.transitions_.size();

		oss << "\nstatic YYTRAILING_" << i << "_TABLE: [[i32; 256]; " << tsize << "] = [\n";
		for (size_t s = 0; s < tsize; ++s) {
			oss << "\t[";
			for (int c = 0; c < 256; c++) {
				auto it = tdfa.transitions_[s].find(static_cast<char>(c));
				oss << (c == 0 ? " " : ", ")
					<< (it != tdfa.transitions_[s].end() ? it->second : -1);
			}
			oss << " ],\n";
		}
		oss << "];\n";

		oss << "static YYTRAILING_" << i << "_ACCEPT: [bool; " << tsize << "] = [";
		for (size_t s = 0; s < tsize; ++s) {
			bool is_accepting = tdfa.final_states_.find(static_cast<int>(s)) != tdfa.final_states_.end()
				&& !tdfa.final_states_.at(static_cast<int>(s)).empty();
			oss << (s == 0 ? " " : ", ") << (is_accepting ? "true" : "false");
		}
		oss << " ];\n";
	}
}

/**
 * @brief Emit the `GeneratedLexer` struct and its `LexerDef` trait implementation.
 *
 * Generates `transition()`, `accept_entries()`, `start_state()`, `sink()`,
 * and `execute_action()`. Optional trailing-context methods are only emitted
 * when the DFA contains variable-length trailing context DFAs.
 */
void CodegenRust::write_generated_lexer(const automata::DFA& dfa, const lexer_file::LexFile& lexfile, std::ostringstream& oss)
{
	oss << "pub struct GeneratedLexer;\n\n";

	oss << "impl LexerDef for GeneratedLexer {\n";
	oss << "\ttype UserData = " << lexfile.rust_user_data_type_ << ";\n\n";

	if (lexfile.verbatim_bottom_.find("fn user_yywrap") != std::string::npos) {
		oss << "\tfn yywrap(scanner: &mut Scanner<Self>) -> bool { user_yywrap(scanner) }\n\n";
	}
	oss << "\tfn transition(state: usize, c: u8) -> i32 { YYTABLE[state][c as usize] }\n";

	oss << "\tfn accept_entries(state: usize) -> &'static [(i32, i32, i32)] {\n";
	oss << "\t\tlet offset = YYACCEPT_OFFSET[state];\n";
	oss << "\t\tif offset < 0 {\n";
	oss << "\t\t\treturn &[];\n";
	oss << "\t\t}\n";
	oss << "\t\tlet count = YYACCEPT_COUNT[state];\n";
	oss << "\t\t&YYACCEPT_DATA[offset as usize .. offset as usize + count]\n";
	oss << "\t}\n\n";

	oss << "\tfn start_state(condition: usize, bol: bool) -> usize {\n";
	oss << "\t\tif bol {\n";
	oss << "\t\t\tYYSTART_STATES[condition + YYNB_CONDITIONS]\n";
	oss << "\t\t} else {\n";
	oss << "\t\t\tYYSTART_STATES[condition]\n";
	oss << "\t\t}\n";
	oss << "\t}\n\n";

	int sink_val = dfa.sink_ < 0 ? std::numeric_limits<int>::max() : dfa.sink_;
	oss << "\tfn sink() -> usize { " << sink_val << " }\n";

	if (!dfa.trailing_dfas_.empty()) {
		oss << "\n\tfn yytrailing_transition(dfa_id: usize, state: usize, c: u8) -> i32 {\n";
		oss << "\tmatch dfa_id {\n";
		for (size_t i = 0; i < dfa.trailing_dfas_.size(); ++i) {
			oss << "\t\t\t" << i << " => YYTRAILING_" << i << "_TABLE[state][c as usize],\n";
		}
		oss << "\t\t\t_ => -1,\n";
		oss << "\t\t}\n";
		oss << "\t}\n\n";

		oss << "\tfn yytrailing_accept(dfa_id: usize, state: usize) -> bool {\n";
		oss << "\t\tmatch dfa_id {\n";
		for (size_t i = 0; i < dfa.trailing_dfas_.size(); ++i) {
			oss << "\t\t\t" << i << " => state < YYTRAILING_" << i << "_ACCEPT.len() && YYTRAILING_" << i << "_ACCEPT[state],\n";
		}
		oss << "\t\t\t_ => false,\n";
		oss << "\t\t}\n";
		oss << "\t}\n";
	}

	oss << "\t#[allow(unreachable_code)]\n";
	oss << "\tfn execute_action(scanner: &mut Scanner<Self>, rule_id: i32) -> Option<i32> {\n";

	for (size_t i = 0; i < lexfile.verbatim_rules_.size(); ++i) {
		oss << "\t\t" << lexfile.verbatim_rules_[i] << "\n";
	}

	oss << "\t\tmatch rule_id {\n";
	for (size_t i = 0; i < lexfile.rules_.size(); ++i) {
		oss << "\t\t\t" << i << " => { "<< lexfile.rules_[i].action_ << " None }\n";
	}
	oss << "\t\t\t_ => None,\n";
	oss << "\t\t}\n";
	oss << "\t}\n";
	oss << "}\n\n";
}

/**
 * @brief Emit the bottom verbatim block and a default `fn main` when not user-provided.
 */
void CodegenRust::write_epilogue(const lexer_file::LexFile& lexfile, std::ostringstream& oss)
{
	oss << lexfile.verbatim_bottom_ << std::endl;
	if (lexfile.verbatim_bottom_.find("fn main") == std::string::npos) {
		oss << "fn main() { ftlex_runtime::ftlex_main::<GeneratedLexer>(); }\n";
	}
}