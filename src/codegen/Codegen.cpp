#include "Codegen.hpp"
#include "compression/DenseRows.hpp"
#include "compression/DefaultChainBuilder.hpp"
#include "compression/DiffProfileBuilder.hpp"
#include "yylex_template.h"
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <tuple>
#include <unordered_set>

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

/**
 * @brief Emit "static int {name}[] = { v0, v1, ... };". 
 * 
 * Used for every flat int array in the generated tables -- start states,
 * packed table arrays, accept offsets/counts, trailing size -- so the
 * comma-joining logic exists in exactly one place.
 */
void emit_int_array(std::ostringstream& oss, const std::string& name, const std::vector<int>& values)
{
	oss << "static int " << name << "[] = {";
	for (size_t i = 0; i < values.size(); ++i)
		oss << (i == 0 ? " " : ", ") << values[i];
	oss << " };\n";
}

/**
 * @brief Emit "static int {name}[S][256] = { {...}, ... };" from a sparse
 * 		  per-state transition map.
 */
void emit_dense_table(std::ostringstream& oss, const std::string& name,
	const std::vector<std::unordered_map<char, int>>& transitions)
{
	oss << "static int " << name << "[" << transitions.size() << "][256] = {\n";
	for (size_t s = 0; s < transitions.size(); ++s) {
		oss << "\t{";
		for (int c = 0; c < 256; c++) {
			auto it = transitions[s].find(static_cast<char>(c));
			oss << (c == 0 ? " " : ", ")
				<< (it != transitions[s].end() ? it->second : -1);
		}
		oss << " },\n";
	}
	oss << "};\n";
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
 * @brief Run dense-rows -> default[]-chain -> diff-profiles -> packing.
 * 
 * See DenseRows.hpp / DefaultChainBuilder.hpp / DiffProfileBuilder.hpp for
 * each stage. 
 */
PackedTables Codegen::compress(const std::vector<std::unordered_map<char, int>>& transitions, int sink, int initial_state) const
{
	DenseRows	dense	= build_dense_rows(transitions, sink);

	DefaultChainBuilder	builder(dense.rows, dense.rows.size());
	std::vector<int>	parent	= builder.prim(initial_state);

	std::vector<Profile>	diffs	= build_diff_profiles(dense, parent);

	TablePacker	tp;
	PackedTables	tables	= tp.pack_profiles(diffs);
	tables.def_	= std::move(parent);

	return tables;
}

/**
 * @brief Emit one DFA's transition table, compressed or dense.
 */
void Codegen::emit_transition_table(std::ostringstream& oss, const automata::DFA& dfa, bool compression,
	const std::string& table_root, automata::Stats* stats) const
{
	const size_t	nb_states	= dfa.transitions_.size();

	if (compression) {
		PackedTables	tables	= compress(dfa.transitions_, dfa.sink_, dfa.initial_state_);

		if (stats) {
			stats->table_size_raw		= nb_states * 256;
			stats->table_size_packed	= tables.next_.size() + tables.check_.size() + tables.base_.size() + tables.def_.size();
			stats->compression_ratio	= stats->table_size_raw == 0 ? 0.0f
				: static_cast<float>(stats->table_size_packed) / static_cast<float>(stats->table_size_raw);
			stats->comp_base_size		= tables.base_.size();
			stats->comp_next_size		= tables.next_.size();
			stats->comp_empty_entries	= static_cast<size_t>(
				std::count(tables.next_.begin(), tables.next_.end(), -1));
			std::unordered_set<int> protos;
			for (int p : tables.def_) if (p >= 0) protos.insert(p);
			stats->comp_prototype_states = static_cast<int>(protos.size());
		}

		emit_int_array(oss, table_root + "base", tables.base_);
		emit_int_array(oss, table_root + "check", tables.check_);
		emit_int_array(oss, table_root + "next", tables.next_);
		emit_int_array(oss, table_root + "default", tables.def_);
	} else {
		emit_dense_table(oss, table_root + "table", dfa.transitions_);

		if (stats) {
			stats->table_size_raw		= nb_states * 256;
			stats->table_size_packed	= nb_states * 256;
			stats->compression_ratio	= 0.0f;
		}
	}
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
 * and set to -1 when the rule has no trailing context or a fixed-length one.
 * 
 * Each entry in dfa.trailing_dfas_ gets its own transition table (compressed
 * or dense, same as the main DFA) plus an accept[] flag array, dispatched at
 * runtime through yy_trailing_tables[] / yytrailing_accepts[] / yytrailing_sizes[].
 * 
 * @param dfa Deterministic automaton.
 */
void Codegen::write_tables(const automata::DFA& dfa, const lexer_file::LexFile& lexfile, std::string& tmpl, automata::Stats* stats)
{
	const size_t	nb_states	= dfa.transitions_.size();
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

	emit_int_array(oss, "yystart_states", ids);

	// ------------------------------------------------------------------------
	// Main DFA transition table -- compressed (packing + default[]) or dense
	// ------------------------------------------------------------------------
	emit_transition_table(oss, dfa, lexfile.compression_, "yy", stats);

	// ------------------------------------------------------------------------
	// yyaccept_data[] - flat array of {rule_id, tlen, tdfa_id} entries.
	//
	// For each state, rules are stored by ascending rule index
	// (index 0 = highest priority). tlen is -1 when the rule carries
	// no trailing context and -2 when the tlen is variable. tdfa_id is -1 when
	// the rule carries no trailing context or when the tlen is fixed.
	// ------------------------------------------------------------------------
	std::vector<int>	offsets(nb_states, -1);
	std::vector<int>	counts(nb_states, 0);
	std::vector<std::tuple<int, int, int>>	flat;	// {rule_id, tlen, tdfa_id}

	for (size_t s = 0; s < nb_states; s++) {
		auto found	= dfa.final_states_.find(static_cast<int>(s));
		if (found == dfa.final_states_.end() || found->second.empty())
			continue;

		std::vector<int>	sorted	= found->second;
		std::sort(sorted.begin(), sorted.end());

		offsets[s]	= static_cast<int>(flat.size());
		counts[s]	= static_cast<int>(sorted.size());

		for (int rule_id : sorted) {
			int	tlen	= -1;
			int tdfa_id	= -1;
			if (rule_id >= 0 && static_cast<size_t>(rule_id) < lexfile.rules_.size()
				&& !lexfile.rules_[rule_id].trailing_.empty()) {
					tlen	= lexfile.rules_[rule_id].trailing_length_;
					tdfa_id	= lexfile.rules_[rule_id].trailing_dfa_id_;
				}
				flat.push_back({rule_id, tlen, tdfa_id});
		}
	}

	oss << "static YYAcceptEntry yyaccept_data[] = {\n";
	if (flat.empty()) {
		// Avoid a zero-length array (not valid C89, guard for safety).
		oss << "\t{ -1, -1, -1}\n";
	} else {
		for (auto& [rid, tlen, tdfa_id] : flat) {
			oss << "\t{ " << rid << ", " << tlen <<  ", " << tdfa_id << " },\n";
		}
	}
	oss <<"};\n";


	emit_int_array(oss, "yyaccept_offset", offsets);
	emit_int_array(oss, "yyaccept_count", counts);

	// ------------------------------------------------------------------------
	// Per-DFA tables for variable-length trailing context (one set per entry
	// in DFA::trailing_dfas_).
	// ------------------------------------------------------------------------
	for (size_t i = 0; i < dfa.trailing_dfas_.size(); ++i) {
		auto		dfa_curr	= dfa.trailing_dfas_[i];
		size_t		size_curr	= dfa_curr.transitions_.size();
		std::string	root		= "yytrailing_" + std::to_string(i) + "_";

		// Trailing-context tables don't feed the top-level compression
		// stats -- those tarck only the main scanner table.
		emit_transition_table(oss, dfa_curr, lexfile.compression_, root, nullptr);

		oss << "static int yytrailing_" << i << "_accept[" << size_curr << "] = {";
		for (size_t s = 0; s < size_curr; ++s) {
			auto it = dfa_curr.final_states_.find(s);
			oss << (s == 0 ? " " : ", ")
				<< (it != dfa_curr.final_states_.end() ? 1 : 0);
		}
		oss << " };\n";
	}

	// Pointer arrays indexed by trailing_dfa_id, used by the runtime dispatcher.
	size_t nb_trailing = dfa.trailing_dfas_.size();

	if (lexfile.compression_) {
		auto emit_ptr_array = [&](const std::string& name, const std::string& suffix) {
			oss << "static int *" << name << "[] = {";
			for (size_t s = 0; s < nb_trailing; ++s) {
				oss << (s == 0 ? " " : ", ")
					<< "yytrailing_" << s << "_" << suffix;
			}
			if (!nb_trailing) oss << " NULL ";
			oss << "};\n";
		};
		emit_ptr_array("yytrailing_bases",    "base");
		emit_ptr_array("yytrailing_checks",   "check");
		emit_ptr_array("yytrailing_nexts",    "next");
		emit_ptr_array("yytrailing_defaults", "default");
	} else {
		oss << "static int (*yytrailing_tables[])[256] = {";
		for (size_t s = 0; s < nb_trailing; ++s) {
			oss << (s == 0 ? " " : ", ")
				<< "yytrailing_" << s << "_table";
		}
		if (!nb_trailing) oss << " NULL ";
		oss << "};\n";
	}


	oss << "static int *yytrailing_accepts[] = {";
	for (size_t s = 0; s < nb_trailing; ++s) {
		oss << (s == 0 ? " " : ", ")
			<< "yytrailing_" << s << "_accept";
	}
	if (!nb_trailing) oss << " NULL ";
	oss << "};\n";

	std::vector<int> trailing_sizes;
	for (size_t s = 0; s < nb_trailing; ++s)
		trailing_sizes.push_back(static_cast<int>(dfa.trailing_dfas_[s].transitions_.size()));
	if (!nb_trailing)
		trailing_sizes.push_back(-1);
	emit_int_array(oss, "yytrailing_sizes", trailing_sizes);


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