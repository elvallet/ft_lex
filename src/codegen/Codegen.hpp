#pragma once

#include <string>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <sstream>

#include "../automata/DFA.hpp"
#include "../automata/Stats.hpp"
#include "../lexer_file/LexFile.hpp"
#include "compression/TablePacker.hpp"

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

	/**
	 * @brief Run the full table-compression pipeline for one DFA's transitions.
	 * 
	 * Pipeline: dense rows -> max spanning-tree default[] chain (rooted at 
	 * initial_state) -> diff_only_profiles -> greedy offset packing.
	 * 
	 * @param transitions Per-state transition map (post DFA completion).
	 * @param sink Sink state id, or -1 if the DFA needed none.
	 * @param initial_state Root of the default[] spanning tree.
	 * @return Packed tables, def_ populated with the default[] chain.
	 */
	PackedTables	compress(const std::vector<std::unordered_map<char, int>>& transitions, int sink, int initial_state) const;

	/**
	 * @brief Emit one DFA's transition table -- compressed or dense depending
	 * 		  on lexfile.compression_ -- under a given name root.
	 * 
	 * Compressed: "{root}base[]", "{root}check[]", "{root}next[]", "{root}default[]".
	 * Dense:	   "{root}table[S][256]".
	 * 
	 * stats is only updated when non-null -- callers pass nullptr for
	 * trailing-context DFAs, matching the existing behaviour where stats
	 * track only the main scanner table.
	 * 
	 * @param oss Output stream to append generated C declarations to.
	 * @param dfa DFA whose transitions to emit.
	 * @param compression Whether to use the compressed representation.
	 * @param table_root Name prefix, e.g. "yy" or "yytrailing_2_".
	 * @param stats Optionnal stats sink.
	 */
	void	emit_transition_table(std::ostringstream& oss, const automata::DFA& dfa, bool compression, const std::string& table_root, automata::Stats* stats) const;
};

} // namespace codegen