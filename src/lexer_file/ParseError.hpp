#pragma once

#include <string>

namespace lexer_file {

/**
 * @brief Source location context used while parsing.
 */
struct ParseContext {
	/** Parsed file path. */
	std::string	filename_;
	/** Current 1-based source line number. */
	int			line_number_;
};

/**
 * @brief Exception thrown when the lexer source is syntactically invalid.
 */
class ParseError: public std::exception {
public:
	/**
	 * @brief Build an error from explicit location fields.
	 * @param msg Human-readable message.
	 * @param file Source file path.
	 * @param line 1-based line number (`-1` if unavailable).
	 * @param column 0-based column (`-1` if unavailable).
	 */
	ParseError(const std::string& msg, const std::string& file, int line = -1, int column = -1);
	/**
	 * @brief Build an error from a parse context.
	 * @param msg Human-readable message.
	 * @param context Current parser context.
	 * @param column 0-based column (`-1` if unavailable).
	 */
	ParseError(const std::string& msg, const ParseContext& context, int column = -1);
	virtual ~ParseError() throw() {}

	/** @brief Return stored line number. */
	int 				line() const { return line_; }
	/** @brief Return stored column number. */
	int 				column() const { return column_; }
	/** @brief Return stored source filename. */
	const std::string&	filename() const { return filename_; }

	/** @brief Return formatted error message. */
	const char* what() const throw() { return msg_.c_str(); }

private:
	std::string	msg_;
	std::string	filename_;
	int			line_;
	int			column_;
};

} // namespace lexer_file