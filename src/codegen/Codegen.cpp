#include "Codegen.hpp"
#include "TablePacker.hpp"
#include "yylex_template.h"
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <algorithm>

using namespace codegen;

namespace {

/**
 * @brief Replace all occurrences of a marker in a template string.
 * @param text Mutable template content.
 * @param from Marker to replace.
 * @param to Replacement content.
 */
void replace_all(std::string& text, const std::string& from, const std::string& to)
{
	if (from.empty())
		return;

	size_t pos = 0;
	while ((pos = text.find(from, pos)) != std::string::npos) {
		text.replace(pos, from.size(), to);
		pos += to.size();
	}
}

/**
 * @brief Build code injected near the start of yylex().
 * @param lexfile Parsed lexer file.
 * @return Concatenated verbatim rule snippets.
 */
std::string build_verbatim_rules(const lexer_file::LexFile& lexfile)
{
	std::string out;

	for (auto& line : lexfile.verbatim_rules_) {
		out += line;
		out += "\n";
	}
	return out;
}

/**
 * @brief Build the switch body dispatching rule indices to user actions.
 * 
 * Trailing context is handled entirely by the runtime (committed_len
 * already excludes the trailing part before the action runs), so no
 * yyless() is emitted here.
 * 
 * @param lexfile Parsed lexer file.
 * @return Generated C switch cases.
 */
std::string build_rules_switch(const lexer_file::LexFile& lexfile)
{
	std::string out;
	for (size_t i = 0; i < lexfile.rules_.size(); ++i) {
		out += "\t\t\tcase ";
		out += std::to_string(i);
		out += ":\n\t\t\t\t";
		out += lexfile.rules_[i].action_;
		out += "\n\t\t\t\tbreak;\n";
	}
	out += "\t\t\tdefault:\n\t\t\t\tbreak;\n";
	return out;
}

} // namespace

/**
 * @brief Generate the full scanner source by substituting all template markers.
 * @param dfa Deterministic automaton.
 * @param lexfile Parsed lexer file.
 * @param out Output stream.
 */
size_t Codegen::generate(const automata::DFA& dfa, const lexer_file::LexFile& lexfile, std::ostream& out, automata::Stats* stats)
{
	// yylex.template.c is embedded as a byte array in yylex_template.h.
	std::string tmpl(
		reinterpret_cast<const char*>(yylex_template_c),
		yylex_template_c_len
	);

	std::string compression	= lexfile.compression_ ? "#define MODE_COMPRESSION" : "";
	std::string mode		= lexfile.array_mode_ ? "YYARRAY_MODE" : "YYPOINTER_MODE";

	replace_all(tmpl, "@@COMPRESSION@@", compression);
	replace_all(tmpl, "@@YYTEXT_MODE@@", mode);
	replace_all(tmpl, "@@SINK@@", std::to_string(dfa.sink_));

	write_prologue(lexfile, tmpl);
	write_tables(dfa, lexfile, tmpl, stats);
	write_yylex(lexfile, tmpl);
	write_epilogue(lexfile, tmpl);

	out << tmpl;
	if (stats) {
		stats->output_bytes = tmpl.size();
	}
	return tmpl.size();
}

/**
 * @brief Emit the top verbatim block from the lexer file.
 * @param lexfile Parsed lexer file.
 */
void Codegen::write_prologue(const lexer_file::LexFile& lexfile, std::string& tmpl)
{
	replace_all(tmpl, "@@PROLOGUE@@", lexfile.verbatim_top_);
}

/**
 * @brief Emit all DFA tables into the @@TABLES@@ marker.
 * 
 * Format:
 *  yyaccept_data[]		- flat array of YYAcceptEntry {rule_id, trailing_len}
 *  yyaccept_offset[S]	- index into yyaccept_data for state S (-1 if non-accepting)
 *  yyaccept_count[S]	- number of entries for state S
 * 
 * Rules are sorted by priority (ascending rule index = higher priority),
 * so index 0 for a given state is always the highest-priority rule.
 * The runtime pushes them in reverse order so the best rule ends up on top.
 * 
 * trailing_len for each rule is read from lexfile.rules_[rule_id].trailing_length_
 * and set to -1 when the rule has no trailing context.
 * 
 * @param dfa Deterministic automaton.
 */
void Codegen::write_tables(const automata::DFA& dfa, const lexer_file::LexFile& lexfile, std::string& tmpl, automata::Stats* stats)
{
	const size_t	nb_states	= dfa.transitions_.size();
	const size_t	raw_table_size	= nb_states * 256;
	std::ostringstream	oss;

	// ------------------------------------------------------------------------
	// Start condition defines and yystart_states[]
	// ------------------------------------------------------------------------
	std::vector<int>			ids;
	std::vector<std::string>	base_conditions;

	oss << "#define INITIAL 0\n";
	ids.push_back(dfa.initial_state_);
	base_conditions.push_back("INITIAL");
	int	count	= 1;

	for (auto& [name, id] : dfa.start_states_) {
		if (name == "INITIAL" || name.find("_BOL") != std::string::npos)
			continue;
		oss << "#define " << name << " " << count << "\n";
		ids.push_back(id);
		base_conditions.push_back(name);
		count++;
	}

	oss << "#define YYNB_CONDITIONS " << count << "\n";

	for (const std::string& cond : base_conditions) {
		std::string	bol	= cond + "_BOL";
		auto it	= dfa.start_states_.find(bol);
		ids.push_back(it != dfa.start_states_.end()
			? it->second
			: dfa.start_states_.at(cond));
	}

	oss << "static int yystart_states[] = {";
	for (size_t i = 0; i < ids.size(); i++) {
		oss << (i == 0 ? " " : ", ") << ids[i];
	}
	oss << " };\n";

	if (lexfile.compression_) {
		TablePacker	tp;

		auto tables = tp.pack(dfa.transitions_, dfa.sink_);
		if (stats) {
			stats->table_size_raw = raw_table_size;
			stats->table_size_packed = tables.next_.size();
			stats->compression_ratio = raw_table_size == 0 ? 0.0f : static_cast<float>(stats->table_size_packed) / static_cast<float>(raw_table_size);
		}

		oss << "static int yybase[] = {";
		for (size_t i = 0; i < tables.base_.size(); i++) {
			oss << (i == 0 ? " " : ", ") << tables.base_[i];
		}
		oss << " };\n";

		oss << "static int yycheck[] = {";
		for (size_t i = 0; i < tables.check_.size(); i++) {
			oss << (i == 0 ? " " : ", ") << tables.check_[i];
		}
		oss << " };\n";

		oss << "static int yynext[] = {";
		for (size_t i = 0; i < tables.next_.size(); i++) {
			oss << (i == 0 ? " " : ", ") << tables.next_[i];
		}
		oss << " };\n";
	} else {
		// ------------------------------------------------------------------------
		// yytable[S][256] - transition table
		// ------------------------------------------------------------------------
		oss << "static int yytable[" << nb_states << "][256] = {\n";
		for (size_t s = 0; s < nb_states; s++) {
			oss << "\t{";
			for (int c = 0; c < 256; c++) {
				auto it = dfa.transitions_[s].find(static_cast<char>(c));
				oss	<< (c == 0 ? " " : ", ")
					<< (it != dfa.transitions_[s].end() ? it->second : -1);
			}
			oss << " },\n";
		}
		oss << "};\n";
		if (stats) {
			stats->table_size_raw = raw_table_size;
			stats->table_size_packed = raw_table_size;
			stats->compression_ratio = 0.0f;
		}
	}

	// ------------------------------------------------------------------------
	// yyaccept_data[] - flat array of {rule_id, trailing_len} entries.
	//
	// For each state, rules are stored by ascending rule index
	// (index 0 = highest priority). trailing_len is -1 when the rule
	// carries no trailing context.
	// ------------------------------------------------------------------------
	std::vector<int>	offsets(nb_states, -1);
	std::vector<int>	counts(nb_states, 0);
	std::vector<std::pair<int, int>>	flat;	// {rule_id, trailing_len}

	for (size_t s = 0; s < nb_states; s++) {
		auto found	= dfa.final_states_.find(static_cast<int>(s));
		if (found == dfa.final_states_.end() || found->second.empty())
			continue;

		std::vector<int>	sorted	= found->second;
		std::sort(sorted.begin(), sorted.end());

		offsets[s]	= static_cast<int>(flat.size());
		counts[s]	= static_cast<int>(sorted.size());

		for (int rule_id : sorted) {
			// trailing_length_ is only meaningful when trailing_ is non-empty.
			int	tlen	= -1;
			if (rule_id >= 0 && static_cast<size_t>(rule_id) < lexfile.rules_.size()
				&& !lexfile.rules_[rule_id].trailing_.empty()) {
					tlen	= lexfile.rules_[rule_id].trailing_length_;
				}
				flat.push_back({rule_id, tlen});
		}
	}
	oss << "static YYAcceptEntry yyaccept_data[] = {\n";
	if (flat.empty()) {
		// Avoid a zero-length array (not valid C89, guard for safety).
		oss << "\t{ -1, -1 }\n";
	} else {
		for (auto& [rid, tlen] : flat) {
			oss << "\t{ " << rid << ", " << tlen << " },\n";
		}
	}
	oss <<"};\n";

	oss << "static int yyaccept_offset[" << nb_states << "] = {";
	for (size_t s = 0; s < nb_states; s++) {
		oss << (s == 0 ? " " : ", ") << offsets[s];
	}
	oss << " };\n";

	oss << "static int yyaccept_count[" << nb_states << "] = {";
	for (size_t s = 0; s < nb_states; s++) {
		oss << (s == 0 ? " " : ", ") << counts[s];
	}
	oss << " };\n";

	replace_all(tmpl, "@@TABLES@@", oss.str());
}

/**
 * @brief Emit yylex() from the embedded template.
 * @param dfa Deterministic automaton.
 * @param lexfile Parsed lexer file.
 */
void Codegen::write_yylex(const lexer_file::LexFile& lexfile, std::string& tmpl)
{
	replace_all(tmpl, "@@VERBATIM_RULES@@", build_verbatim_rules(lexfile));
	replace_all(tmpl, "@@RULES@@", build_rules_switch(lexfile));
}

/**
 * @brief Emit the trailing user C code section.
 * @param lexfile Parsed lexer file.
 */
void Codegen::write_epilogue(const lexer_file::LexFile& lexfile, std::string& tmpl)
{
	replace_all(tmpl, "@@EPILOGUE@@", lexfile.verbatim_bottom_);
}