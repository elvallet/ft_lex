#pragma once

#include "LexFile.hpp"
#include "LineReader.hpp"
#include "ParseError.hpp"

#include <string>
#include <vector>

namespace lexer_file {

class LexParser {
public:
	LexParser(const std::string& filename);

	LexFile	parse();

private:

	std::vector<Rule>	parse_rules();

	Rule								parse_single_rule(const std::string& line);
	std::pair<std::string, std::string>	split_pattern_action(const std::string& raw);
	std::string							complete_action(const std::string& partial);

	void parse_definitions();

	std::string							parse_verbatim_block(const std::string& line);
	std::pair<std::string, std::string>	parse_macro_line(const std::string& line);

	void parse_user_code();

	void expand_macros();
	void expand_rules();

	LineReader	reader_;
	LexFile		lex_file_;
};

} // namespace lexer_file