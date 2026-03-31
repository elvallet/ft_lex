#pragma once

#include <string>
#include <optional>
#include <fstream>
#include "ParseError.hpp"

namespace lexer_file {

struct PhysicalLine {
    std::string content_;
	int         line_number_;
};

class LineReader {
public:
	LineReader(const std::string &filename);

    std::optional<PhysicalLine> next();
    std::optional<PhysicalLine> peek();

	ParseContext    context() const { return context_; }

private:
    std::optional<PhysicalLine> read_logical_line();
	
    std::ifstream               file_;
    std::optional<PhysicalLine> lookahead_;
	ParseContext                context_;
};

} // namespace lexer_file