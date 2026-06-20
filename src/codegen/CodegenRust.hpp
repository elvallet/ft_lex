#pragma once

#include <ostream>
#include <fstream>
#include <sstream>
#include <cstddef>

#include "../automata/DFA.hpp"
#include "../automata/Stats.hpp"
#include "../lexer_file/LexFile.hpp"

namespace codegen {

/**
 * @brief Generates a Rust scanner source file from lexer data and DFA tables.
 *
 * The output implements the `LexerDef` trait from `ftlex_runtime` and includes
 * transition tables, accept tables, and the `execute_action` dispatch function.
 */
class CodegenRust {
public:
	/**
	 * @brief Generate the full Rust scanner source.
	 * @param dfa Deterministic automaton used for runtime transitions.
	 * @param lexfile Parsed lexer file sections and rule actions.
	 * @param out Output stream for the generated Rust code.
	 * @param stats Optional statistics struct updated with table sizes and output byte count.
	 * @return Number of bytes written to @p out.
	 */
	size_t	generate(const automata::DFA&, const lexer_file::LexFile& lexfile, std::ostream& out, automata::Stats* stats = nullptr);

private:
	/** @brief Emit `use` imports and the top verbatim block. */
	void	write_prologue(const lexer_file::LexFile& lexfile, std::ostringstream& oss);
	/** @brief Emit DFA transition and accept tables as Rust static arrays. */
	void	write_tables(const automata::DFA& dfa, const lexer_file::LexFile& lexfile, std::ostringstream& oss, automata::Stats* stats);
	/** @brief Emit the `GeneratedLexer` struct and its `LexerDef` implementation. */
	void	write_generated_lexer(const automata::DFA& dfa, const lexer_file::LexFile& lexfile, std::ostringstream& oss);
	/** @brief Emit the bottom verbatim block and `fn main` if not user-provided. */
	void	write_epilogue(const lexer_file::LexFile& lexfile, std::ostringstream& oss);
};

} // namespace codegen