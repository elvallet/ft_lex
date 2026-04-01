#pragma once

#include <string>
#include <vector>

namespace lexer_file {

struct Rule {
	std::string	pattern_;
	std::string	action_;
	bool		is_pipe_;
};

struct LexFile {
	std::string											verbatim_top_;
	std::vector<std::pair<std::string, std::string>>	macros_;
	std::vector<Rule>									rules_;
	std::string											verbatim_bottom_;
	std::vector<std::string>							verbatim_rules_;
};

} // namespace lexer_file