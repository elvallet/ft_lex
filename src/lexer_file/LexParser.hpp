/**
 * @file LexParser.hpp
 * @brief Parser for lex/flex-like source files.
 */
#pragma once

#include "LexFile.hpp"
#include "LineReader.hpp"
#include "ParseError.hpp"

#include <string>
#include <vector>
#include <optional>

namespace lexer_file {

/**
 * @brief Parser for lex/flex-like source files.
 *
 * It reads definitions, rules, and trailing user code, then expands macros
 * in both macro definitions and rule patterns.
 */
class LexParser {
public:
	/**
	 * @brief Build a parser from an input file path.
	 * @param filename Path to the lexer source file.
	 */
	LexParser(const std::string& filename);

	/**
	 * @brief Parse the whole file and return a structured representation.
	 * @return Parsed lexer file.
	 * @throw ParseError If the source is malformed.
	 */
	LexFile	parse();

private:

	/**
	 * @brief Parse all rules until a `%%` separator or EOF.
	 */
	std::vector<Rule>	parse_rules();

	/** @brief Parse one rule line and complete its action if needed. */
	Rule								parse_single_rule(const std::string& line);
	/** @brief Parse `<COND1,COND2>` prefix and return normalized condition names. */
	std::vector<std::string>			extract_conditions(const std::string& line, size_t* i);
	/** @brief Split a raw rule into `<pattern, action>`. */
	std::pair<std::string, std::string>	split_pattern_action(const std::string& raw);
	/** @brief Split a pattern string at the first unescaped `/` or `$`, returning `<pattern, trailing>`. */
	std::pair<std::string, std::string>	detect_trailing(const std::string& pattern);
	/** @brief Continue reading lines until an action block is syntactically closed. */
	std::string							complete_action(const std::string& partial);

	/** @brief Parse the definitions section (before the first `%%`). */
	void 	parse_definitions();
	/** @brief Parse `%s/%x` start-condition directives declared in section 1. */
	void	parse_conditions(const std::string& line);

	/** @brief Parse `%{ ... %}` block content (without delimiters). */
	std::string							parse_verbatim_block(const std::string& line);
	/** @brief Parse a macro definition line into `<name, regex>`. */
	std::pair<std::string, std::string>	parse_macro_line(const std::string& line);

	/** @brief Parse trailing user code section (after the second `%%`). */
	void parse_user_code();

	/** @brief Expand earlier macro references inside later macro definitions. */
	void expand_macros();
	/** @brief Expand macros in rule patterns and reject unresolved macro-like tokens. */
	void expand_rules();
	/**
	 * @brief Compute trailing-context lengths for all rules after macro expansion.
	 *
	 * For each rule with a non-empty `trailing_` field, attempts to determine a
	 * fixed length by analyzing the trailing postfix token stream. If the length
	 * is variable (e.g., alternation with unequal branches), `trailing_is_variable_`
	 * is set and a separate DFA is built for runtime simulation.
	 */
	void compile_trailing_length();

	/** Streaming line reader with lookahead support. */
	LineReader	reader_;
	/** Accumulated parsing output. */
	LexFile		lex_file_;
};

} // namespace lexer_file