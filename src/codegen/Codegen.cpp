#include "Codegen.hpp"
#include "yylex_template.h"
#include <iostream>
#include <stdexcept>
#include <sstream>

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

	for (size_t i = 0; i < lexfile.verbatim_rules_.size(); ++i) {
		out += lexfile.verbatim_rules_[i];
		out += "\n";
	}
	return out;
}

/**
 * @brief Build the switch body dispatching matched rule indices to actions.
 * @param lexfile Parsed lexer file.
 * @return Generated C switch cases.
 */
std::string build_rules_switch(const lexer_file::LexFile& lexfile)
{
	std::string out;

	for (size_t i = 0; i < lexfile.rules_.size(); ++i) {
		out += "\t\t\tcase ";
		out += std::to_string(i);
		out += ":\n";
		if (lexfile.rules_[i].trailing_length_ > 0) {
			// Keep only the left side of r/s in yytext and push trailing chars back.
			out += "\t\t\t\t";
			out += "yyless(yyleng - ";
			out	+= std::to_string(lexfile.rules_[i].trailing_length_);
			out += ");\n";
		}
		out += "\t\t\t\t";
		out += lexfile.rules_[i].action_;
		out += "\n\t\t\t\tbreak;\n";
	}
	out += "\t\t\tdefault:\n\t\t\t\tbreak;\n";
	return out;
}

} // namespace

/**
 * @brief Generate scanner output by writing all sections in order.
 * @param dfa Deterministic automaton.
 * @param lexfile Parsed lexer file.
 * @param out Output file path.
 */
void Codegen::generate(const automata::DFA& dfa, const lexer_file::LexFile& lexfile, std::ostream& out)
{
	// yylex.template.c is embedded as a byte array in yylex_template.h.
	std::string tmpl(
		reinterpret_cast<const char*>(yylex_template_c),
		yylex_template_c_len
	);

	std::string mode = lexfile.array_mode_ ? "YYARRAY_MODE" : "YYPOINTER_MODE";
	replace_all(tmpl, "@@YYTEXT_MODE@@", mode);

	// Generated file layout: prologue + tables + yylex + epilogue.
	write_prologue(lexfile, tmpl);
	write_tables(dfa, tmpl);
	write_yylex(lexfile, tmpl);
	write_epilogue(lexfile, tmpl);

	out << tmpl;
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
 * @brief Emit transition and accept tables used by generated yylex().
 * @param dfa Deterministic automaton.
 */
void Codegen::write_tables(const automata::DFA& dfa, std::string& tmpl)
{
	const size_t		nb_states	= dfa.transitions_.size();
	std::vector<int>	ids;
	std::vector<std::string> base_conditions;
	std::ostringstream	oss;

	oss << "#define INITIAL 0\n";
	ids.push_back(dfa.initial_state_);
	base_conditions.push_back("INITIAL");
	int count = 1;

	// Export condition names as BEGIN() indices used by generated scanner code.
	for (auto& [name, id] : dfa.start_states_) {
		if (name == "INITIAL" || (name.find("_BOL") != std::string::npos)) continue;
		oss << "#define " << name << " " << count << "\n";
		ids.push_back(id);
		base_conditions.push_back(name);
		count++;
	}

	oss << "#define YYNB_CONDITIONS " << count << "\n";

	for (const std::string& cond_name : base_conditions) {
		std::string bol_name = cond_name + "_BOL";
		auto bol_it = dfa.start_states_.find(bol_name);
		if (bol_it != dfa.start_states_.end())
			ids.push_back(bol_it->second);
		else
			ids.push_back(dfa.start_states_.at(cond_name));
	}

	// BEGIN(x) selects a DFA entry state through this indirection table.
	oss << "static int yystart_states[] = {";
	bool first = true;
	for (int i : ids) {
		if (first) {
			oss << " " << i;
			first = false;
		}
		else 
			oss << ", " << i;
	}
	oss << " };" << std::endl;

	// Dense transition table indexed by [state][unsigned char]. Missing edges -> -1.
	oss << "static int yytable[" << nb_states << "][256] = {" << std::endl;
	for (size_t i = 0; i < nb_states; i++) {
		oss << "\t{ ";
		bool first2 = true;
		for (int c = 0; c < 256; c++) {
			if (!first2) {
				oss << ", ";
			} else {
				first2 = false;
			}
			auto found	= dfa.transitions_[i].find(static_cast<char>(c));
			if (found != dfa.transitions_[i].end()) {
				oss << found->second;
			} else {
				oss << "-1";
			}
		}
		oss << "}," << std::endl;
	}
	oss << "};" << std::endl;

	// yyaccept[state] stores winning rule index, or -1 if non-accepting.
	oss << "static int yyaccept[" << nb_states << "] = { ";
	bool first3 = true;
	for (size_t i = 0; i < nb_states; i++) {
		if (!first3) {
			oss << ", ";
		} else {
			first3 = false;
		}
		auto found = dfa.final_states_.find(static_cast<int>(i));
		if (found != dfa.final_states_.end()) {
			oss << found->second;
		} else {
			oss << "-1";
		}
	}
	oss << "};" << std::endl;

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