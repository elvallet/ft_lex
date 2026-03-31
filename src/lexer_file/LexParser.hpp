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

	LineReader	reader_;
	LexFile		lex_file_;
};

} // namespace lexer_file