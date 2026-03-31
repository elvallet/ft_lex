#pragma once

#include <string>

namespace lexer_file {

struct ParseContext {
	std::string	filename_;
	int			line_number_;
};

class ParseError: public std::exception {
public:
	ParseError(const std::string& msg, const std::string& file, int line = -1, int column = -1);
	ParseError(const std::string& msg, const ParseContext& context, int column = -1);
	virtual ~ParseError() throw() {}

	int 				line() const { return line_; }
	int 				column() const { return column_; }
	const std::string&	filename() const { return filename_; }

	const char* what() const throw() { return msg_.c_str(); }

private:
	std::string	msg_;
	std::string	filename_;
	int			line_;
	int			column_;
};

} // namespace lexer_file