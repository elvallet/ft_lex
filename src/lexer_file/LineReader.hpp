#pragma once

#include <string>
#include <optional>
#include <fstream>
#include "ParseError.hpp"

namespace lexer_file {

/**
 * @brief A logical line and its first physical line index in the source file.
 */
struct PhysicalLine {
    /** Logical line content (line continuations already merged). */
    std::string content_;
	/** 1-based index of the first physical line. */
	int         line_number_;
};

/**
 * @brief Read lexer source lines with support for lookahead and continuations.
 *
 * A trailing backslash (`\`) joins the next physical line into the same
 * logical line.
 */
class LineReader {
public:
	/**
	 * @brief Open a source file for reading.
	 * @param filename Path to file.
	 * @throw std::runtime_error If the file cannot be opened.
	 */
	LineReader(const std::string &filename);

    /** @brief Consume and return the next logical line, if any. */
    std::optional<PhysicalLine> next();
    /** @brief Peek the next logical line without consuming it. */
    std::optional<PhysicalLine> peek();

	/** @brief Current parse context (filename + current line index). */
	ParseContext    context() const { return context_; }

private:
    /** @brief Read and assemble one logical line from the underlying stream. */
    std::optional<PhysicalLine> read_logical_line();
	
    /** Input stream bound to the lexer source file. */
    std::ifstream               file_;
    /** Optional cached line used by `peek()`. */
    std::optional<PhysicalLine> lookahead_;
	/** Current parsing context used for diagnostics. */
	ParseContext                context_;
};

} // namespace lexer_file