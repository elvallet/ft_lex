#pragma once

#include <string>
#include <fstream>

#include "../automata/DFA.hpp"
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
	 * @param out Output path of the generated C file.
	 */
	void	generate(const automata::DFA& dfa, const lexer_file::LexFile& lexfile, const std::string& out);

private:
	/** @brief Output stream used by all writer helpers during generation. */
	std::ofstream	out_;

	/**
	 * @brief Write the prologue section.
	 * @param lexfile Parsed lexer file.
	 */
	void	write_prologue(const lexer_file::LexFile& lexfile);
	/**
	 * @brief Write DFA tables consumed by yylex().
	 * @param dfa Deterministic automaton.
	 */
	void	write_tables(const automata::DFA& dfa);
	/**
	 * @brief Materialize yylex() from the embedded template and substitutions.
	 * @param dfa Deterministic automaton.
	 * @param lexfile Parsed lexer file.
	 */
	void	write_yylex(const automata::DFA& dfa, const lexer_file::LexFile& lexfile);
	/**
	 * @brief Write the epilogue section.
	 * @param lexfile Parsed lexer file.
	 */
	void	write_epilogue(const lexer_file::LexFile& lexfile);
};

} // namespace codegen