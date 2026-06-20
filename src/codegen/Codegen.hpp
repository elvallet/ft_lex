#pragma once

#include <string>
#include <fstream>
#include <cstddef>

#include "../automata/DFA.hpp"
#include "../automata/Stats.hpp"
#include "../lexer_file/LexFile.hpp"

namespace codegen {

/**
 * @brief Generates the final scanner C source from lexer data and DFA tables.
 */
class Codegen {
public:
	/**
	 * @brief Generate the full scanner source file.
	 * @param dfa Deterministic automaton used for runtime transitions.
	 * @param lexfile Parsed lexer file sections and rule actions.
	 * @param out Output stream for the generated C code.
	 * @param stats Optional statistics struct updated with table sizes and output byte count.
	 * @return Number of bytes written to @p out.
	 */
	size_t	generate(const automata::DFA& dfa, const lexer_file::LexFile& lexfile, std::ostream& out, automata::Stats* stats = nullptr);

private:
	/**
	 * @brief Write the prologue section.
	 * @param lexfile Parsed lexer file.
	 */
	void	write_prologue(const lexer_file::LexFile& lexfile, std::string& tmpl);
	/**
	 * @brief Write DFA tables consumed by yylex().
	 * @param dfa Deterministic automaton.
	 */
	void	write_tables(const automata::DFA& dfa, const lexer_file::LexFile& lexfile, std::string& tmpl, automata::Stats* stats);
	/**
	 * @brief Materialize yylex() from the embedded template and substitutions.
	 * @param lexfile Parsed lexer file.
	 */
	void	write_yylex(const lexer_file::LexFile& lexfile, std::string& tmpl);
	/**
	 * @brief Write the epilogue section.
	 * @param lexfile Parsed lexer file.
	 */
	void	write_epilogue(const lexer_file::LexFile& lexfile, std::string& tmpl);
};

} // namespace codegen