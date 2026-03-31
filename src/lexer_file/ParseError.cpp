#include "ParseError.hpp"
#include <sstream>

using namespace lexer_file; using namespace std;

ParseError::ParseError(const string& msg, const string& file, int line, int column)
	: filename_(file), line_(line), column_(column)
{
	ostringstream	oss;

	if (line != -1 && column != -1 && !file.empty()) {
		oss << file << ":" << line << ":" << column << " " << msg;
		msg_ = oss.str();
	} else  {
		msg_ = msg;
	}
}

ParseError::ParseError(const string& msg, const ParseContext& context, int column)
	: filename_(context.filename_), line_(context.line_number_), column_(column)
{
	ostringstream	oss;

	if (line_ != -1 && column_ != -1 && !filename_.empty()) {
		oss << filename_ << ":" << line_ << ":" << column_ << " " << msg;
		msg_ = oss.str();
	} else  {
		msg_ = msg;
	}	
}