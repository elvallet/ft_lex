#include "Codegen.hpp"
#include "yylex_template.h"

#include <stdexcept>

using namespace codegen;

namespace {

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

std::string build_verbatim_rules(const lexer_file::LexFile& lexfile)
{
	std::string out;
	for (size_t i = 0; i < lexfile.verbatim_rules_.size(); ++i) {
		out += lexfile.verbatim_rules_[i];
		out += "\n";
	}
	return out;
}

std::string build_rules_switch(const lexer_file::LexFile& lexfile)
{
	std::string out;
	for (size_t i = 0; i < lexfile.rules_.size(); ++i) {
		out += "\t\t\tcase ";
		out += std::to_string(i);
		out += ":\n";
		out += "\t\t\t\t";
		out += lexfile.rules_[i].action_;
		out += "\n\t\t\t\tbreak;\n";
	}
	out += "\t\t\tdefault:\n\t\t\t\tbreak;\n";
	return out;
}

} // namespace

void Codegen::generate(const automata::DFA& dfa, const lexer_file::LexFile& lexfile, const std::string& out)
{
	out_.open(out.c_str());
	if (!out_.is_open())
		throw std::runtime_error("Failed to open output file: " + out);

	write_prologue(lexfile);
	write_tables(dfa);
	write_yylex(dfa, lexfile);
	write_epilogue(lexfile);
}

void Codegen::write_prologue(const lexer_file::LexFile& lexfile)
{
	out_ << lexfile.verbatim_top_ << std::endl;
}

void Codegen::write_tables(const automata::DFA& dfa)
{
	const size_t	nb_states	= dfa.transitions_.size();
	const char	tab			= '\t';

	out_ << "static int yy_table[" << nb_states << "][256] = {" << std::endl;
	for (size_t i = 0; i < nb_states; i++) {
		out_ << tab << "{ ";
		bool first = true;
		for (int c = 0; c < 256; c++) {
			if (!first) {
				out_ << ", ";
			} else {
				first = false;
			}
			auto found	= dfa.transitions_[i].find(static_cast<char>(c));
			if (found != dfa.transitions_[i].end()) {
				out_ << found->second;
			} else {
				out_ << "-1";
			}
		}
		out_ << "}," << std::endl;
	}
	out_ << "};" << std::endl;

	out_ << "static int yy_accept[" << nb_states << "] = { ";
	bool first = true;
	for (size_t i = 0; i < nb_states; i++) {
		if (!first) {
			out_ << ", ";
		} else {
			first = false;
		}
		auto found = dfa.final_states_.find(static_cast<int>(i));
		if (found != dfa.final_states_.end()) {
			out_ << found->second;
		} else {
			out_ << "-1";
		}
	}
	out_ << "};" << std::endl;
}

void Codegen::write_yylex(const automata::DFA& dfa, const lexer_file::LexFile& lexfile)
{
	std::string tmpl(
		reinterpret_cast<const char*>(yylex_template_c),
		yylex_template_c_len
	);

	replace_all(tmpl, "@@INITIAL_STATE@@", std::to_string(dfa.initial_state_));
	replace_all(tmpl, "@@VERBATIM_RULES@@", build_verbatim_rules(lexfile));
	replace_all(tmpl, "@@RULES@@", build_rules_switch(lexfile));

	out_ << tmpl;
}

void Codegen::write_epilogue(const lexer_file::LexFile& lexfile)
{
	out_ << lexfile.verbatim_bottom_;
}