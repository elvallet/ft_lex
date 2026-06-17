#pragma once

#include <ostream>
#include <fstream>
#include <sstream>
#include <cstddef>

#include "../automata/DFA.hpp"
#include "../automata/Stats.hpp"
#include "../lexer_file/LexFile.hpp"

namespace codegen {

class CodegenRust {
public:
	size_t	generate(const automata::DFA&, const lexer_file::LexFile& lexfile, std::ostream& out, automata::Stats* stats = nullptr);

private:
	void	write_prologue(const lexer_file::LexFile& lexfile, std::ostringstream& oss);
	void	write_tables(const automata::DFA& dfa, const lexer_file::LexFile& lexfile, std::ostringstream& oss, automata::Stats* stats);
	void	write_generated_lexer(const automata::DFA& dfa, const lexer_file::LexFile& lexfile, std::ostringstream& oss);
	void	write_epilogue(const lexer_file::LexFile& lexfile, std::ostringstream& oss);
};

} // namespace codegen