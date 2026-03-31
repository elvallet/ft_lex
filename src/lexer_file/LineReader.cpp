#include "LineReader.hpp"

using namespace lexer_file; using namespace std;

LineReader::LineReader(const string &filename)
    : file_(filename)
{
	if (!file_)
		throw runtime_error("Fatal error: Could not open file: " + filename);
	context_.filename_      = filename;
	context_.line_number_   = 0;
}

optional<PhysicalLine> LineReader::next()
{
	if (lookahead_.has_value()) {
		PhysicalLine	line	= *lookahead_;
		lookahead_.reset();
		context_.line_number_	= line.line_number_;
		return line;
	}

	return read_logical_line();
}

optional<PhysicalLine> LineReader::peek()
{
	if (lookahead_.has_value())
		return lookahead_;

	lookahead_	= read_logical_line();
	return lookahead_;
}

optional<PhysicalLine> LineReader::read_logical_line()
{
	string	logical_content;
	string	physical_content;
	int		first_line_number	= context_.line_number_ + 1;
	bool	has_read_line		= false;

	while (getline(file_, physical_content)) {
		has_read_line = true;
		context_.line_number_ += 1;

		if (!physical_content.empty() && physical_content.back() == '\\') {
			physical_content.pop_back();
			logical_content += physical_content;
			continue;
		}

		logical_content += physical_content;
		return PhysicalLine{logical_content, first_line_number};
	}

	if (!has_read_line)
		return nullopt;

	return PhysicalLine{logical_content, first_line_number};
}