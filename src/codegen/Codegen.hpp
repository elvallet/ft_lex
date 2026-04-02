#pragma once

#include <string>
#include <fstream>

#include "../automata/DFA.hpp"
#include "../lexer_file/LexFile.hpp"

namespace codegen {

class Codegen {
public:
	void	generate(const automata::DFA& dfa, const lexer_file::LexFile& lexfile, const std::string& out);

private:
	std::ofstream	out_;

	void	write_prologue(const lexer_file::LexFile& lexfile);
	void	write_tables(const automata::DFA& dfa);
	void	write_yylex(const automata::DFA& dfa, const lexer_file::LexFile& lexfile);
	void	write_epilogue(const lexer_file::LexFile& lexfile);
};

} // namespace codegen