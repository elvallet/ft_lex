#include "LexParser.hpp"

using namespace lexer_file; using namespace std;

/**
 * @brief Construct a parser bound to a lexer source file.
 */
LexParser::LexParser(const std::string& filename)
	: reader_(filename)
{

}

/**
 * @brief Parse the full lexer file in section order.
 */
LexFile LexParser::parse()
{
	parse_definitions();

	auto peeked	= reader_.peek();
	if (!peeked)
		throw ParseError("Missing rules definitions", reader_.context(), 0);
	reader_.next();

	lex_file_.rules_	= parse_rules();

	peeked = reader_.peek();
	if (peeked) {
		reader_.next();
		parse_user_code();
	}

	expand_macros();
	expand_rules();
	
	return lex_file_;
}

namespace {

/**
 * @brief Check whether a character is ASCII whitespace.
 */
bool is_whitespace(char c)
{
	return (c >= 9 && c <= 13) || c == 32;
}

/** @brief Trim trailing characters in-place. */
string& rtrim(string& s, const char* t)
{
	s.erase(s.find_last_not_of(t) + 1);
	return s;
}

/** @brief Trim leading characters in-place. */
string& ltrim(string& s, const char* t)
{
	s.erase(0, s.find_first_not_of(t));
	return s;
}

/** @brief Trim leading and trailing characters in-place. */
string& trim(string& s, const char* t)
{
	return (ltrim(rtrim(s, t), t));
}

} // namespace

pair<string, string> LexParser::split_pattern_action(const std::string& raw)
{
	if (raw.empty())
		return {"", ""};

	bool	bracket	= false;
	bool	quote	= false;
	string	pattern;
	size_t	i		= 0;

	while (i < raw.size())
	{
		char c	= raw[i];

		if (c == '\\') {
			pattern.push_back(c);
			i++;
			if (i < raw.size()) {
				pattern.push_back(raw[i]);
				i++;
			}
			continue;
		} else if (c == '[') {
			bracket = true;
		} else if (c == ']') {
			bracket = false;
		} else if (c == '"') {
			quote = !quote;
		} else if (is_whitespace(c) && !quote && !bracket) {
			break;
		}
		pattern.push_back(c);
		i++;
	}
	

	if (quote || bracket) {
		throw ParseError("Brackets or quotes should always be closed", reader_.context(), int(i));
	}

	string action = raw.substr(i);

	return {pattern, trim(action, " \t\n\r\f\v") };
}

/**
 * @brief Lexical state used while scanning C/C++ action blocks.
 */
enum class State {
	NORMAL,
	STRING,
	CHAR,
	LINE_COMMENT,
	BLOCK_COMMENT
};

/**
 * @brief Read continuation lines until action braces are balanced.
 */
string LexParser::complete_action(const string& partial)
{
	string	result	= partial;
	State	state	= State::NORMAL;
	int		depth	= 0;

	auto process_line	= [&](const string& s) {
		if (state == State::LINE_COMMENT)
			state = State::NORMAL;
		size_t	i = 0;
		while (i < s.size()) {
			char c	= s[i];
			switch (state) {
				case State::NORMAL:
					if (c == '"')		{ state = State::STRING; break; }
					if (c == '\'')		{ state = State::CHAR; break; }
					if (c == '/' && i+1 < s.size() && s[i+1] == '/') { state = State::LINE_COMMENT; i++; break; }
					if (c == '/' && i+1 < s.size() && s[i+1] == '*') { state = State::BLOCK_COMMENT; i++; break; }
					if (c == '{')		depth++;
					if (c == '}')		depth--;
					break;
				case State::STRING:
					if (c == '\\')		{ i++; break; }
					if (c == '"')		state = State::NORMAL;
					break;
				case State::CHAR:
					if (c == '\\')		{ i++; break; }
					if (c == '\'')		state = State::NORMAL;
					break;
				case State::BLOCK_COMMENT:
					if (c == '*' && i+1 < s.size() && s[i+1] == '/') { state = State::NORMAL; i++; }
					break;
			}
			i++;
		}
	};

	process_line(result);
	while (depth > 0) {
		auto line = reader_.next();
		if (!line)
			throw ParseError("Unclosed action block", reader_.context(), 0);
		process_line(line->content_);
		result += "\n" + line->content_;
	}
	return result;
}

/**
 * @brief Parse one rule and resolve its action body.
 */
Rule LexParser::parse_single_rule(const string& line)
{
	auto split = split_pattern_action(line);

	if (split.second == "|") {
		return Rule{split.first, split.second, true};
	}

	string completed_action;
	if (split.second.empty()) {
		auto next	= reader_.next();
		if (!next || next->content_.empty() || next->content_ == "%%")
			throw ParseError("Empty action block", reader_.context(), 0);
		completed_action = complete_action(next->content_);
	} else {
		completed_action = complete_action(split.second);
	}

	return Rule{split.first, completed_action, false};
}

/**
 * @brief Parse all rule entries from the rules section.
 */
vector<Rule> LexParser::parse_rules()
{
	vector<Rule>	v;

	while (true) {
		auto peeked	= reader_.peek();
		if (!peeked || peeked->content_ == "%%")
			break;
		if (peeked->content_.empty()) {
			reader_.next();
			continue;
		}
		if (peeked->content_[0] == ' ' || peeked->content_[0] == '\t') {
			auto line = reader_.next();
			lex_file_.verbatim_rules_.push_back(line->content_);
		} else {
			v.push_back(parse_single_rule(reader_.next()->content_));
		}
	}

	for (size_t i = 0; i < v.size(); i++) {
		if (!v[i].is_pipe_)
			continue;
		if (i == v.size() - 1)
			throw ParseError("Last rule cannot be \"|\"", reader_.context(), 0);
		size_t j = i + 1;
		while (j < v.size() && v[j].is_pipe_)
			j++;
		if (j == v.size())
			throw ParseError("Pipe has no non-pipe rule to follow", reader_.context(), 0);
		v[i].action_ = v[j].action_;
	}
	return v;
}

/**
 * @brief Parse file definitions until first `%%`.
 */
void LexParser::parse_definitions()
{
	while (true) {
		auto peeked	= reader_.peek();
		if (!peeked || peeked->content_ == "%%")
			break;
		if (peeked->content_.empty()) {
			reader_.next();
			continue;
		}

		if (peeked->content_.rfind("%{", 0) == 0) {
			auto line = reader_.next();
			lex_file_.verbatim_top_ += parse_verbatim_block(line->content_);
		} else if (!peeked->content_.empty() && peeked->content_[0] == '%') {
			throw ParseError("Unsupported directive", reader_.context(), 0);
		} else {
			auto line = reader_.next();
			lex_file_.macros_.push_back(parse_macro_line(line->content_));
		}
	}
}

/**
 * @brief Parse `%{ ... %}` verbatim block content.
 */
string LexParser::parse_verbatim_block(const string& line)
{
	string res;

	if (line != "%{") {
		throw ParseError("'%{' should always be followed by a new line", reader_.context(), 3);
	}

	auto peeked = reader_.peek();
	while (peeked && peeked->content_ != "%}") {
		auto next = reader_.next();
		if (res.empty())
			res += next->content_;
		else
			res += "\n" + next->content_;
		peeked = reader_.peek();
	}
	if (!peeked)
		throw ParseError("Unclosed verbatim block '%{", reader_.context(), 0);

	reader_.next();
	return res;
}

/**
 * @brief Parse a macro definition line (`NAME regex`).
 */
pair<string, string> LexParser::parse_macro_line(const string& line)
{
	string	name;
	string	regex;

	char c = line[0];
	if (!(islower(c) || isupper(c) || c == '_')) {
		throw ParseError("Invalid character in macro", reader_.context(), 0);
	}

	size_t	i	= 0;
	while (i < line.size()) {
		c = line[i];

		if (is_whitespace(c)) {
			break;
		}
		if (isalnum(c) || c == '_') {
			name.push_back(c);
		} else {
			throw ParseError("Invalid character in macro", reader_.context(), i);
		}
		i++;
	}

	if (i >= line.size())
		throw ParseError("Invalid macro: regex cannot be empty", reader_.context(), i);

	string	remaining	= line.substr(i);
	regex = trim(remaining, " \t\n\r\f\v");

	if (regex.empty()) 
		throw ParseError("Invalid macro: regex cannot be empty", reader_.context(), i);

	return {name, regex};
}

/**
 * @brief Parse and store remaining user code after rules.
 */
void LexParser::parse_user_code()
{
	string	res;
	auto peeked = reader_.peek();
	while (peeked) {
		auto next = reader_.next();
		if (res.empty())
			res += next->content_;
		else
			res += "\n" + next->content_;
		peeked = reader_.peek();
	}

	lex_file_.verbatim_bottom_	= res;
}

/**
 * @brief Expand macro references across macro definitions.
 */
void LexParser::expand_macros()
{
	for (size_t i = 0; i < lex_file_.macros_.size(); i++) {
		auto& macro = lex_file_.macros_[i];
		string pattern = "{" + macro.first + "}";
		
		for (size_t j = i + 1; j < lex_file_.macros_.size(); j++) {
			size_t pos = 0;
			while ((pos = lex_file_.macros_[j].second.find(pattern, pos)) != string::npos) {
				lex_file_.macros_[j].second.replace(pos, pattern.length(), "(" + macro.second + ")");
				pos += macro.second.length() + 2;
			}
		}
	}
}

/**
 * @brief Expand macros in rule patterns and detect unresolved names.
 */
void LexParser::expand_rules()
{
	for (auto& rule : lex_file_.rules_) {
		for (auto& macro : lex_file_.macros_) {
			string pattern = "{" + macro.first + "}";
			size_t pos = 0;
			while ((pos = rule.pattern_.find(pattern, pos)) != string::npos) {
				rule.pattern_.replace(pos, pattern.length(), "(" + macro.second + ")");
				pos += macro.second.length() + 2;
			}
		}
	}

	for (auto& rule : lex_file_.rules_) {
		size_t pos = rule.pattern_.find('{');
		if (pos != string::npos) {
			size_t end_pos = rule.pattern_.find('}', pos);
			if (end_pos != string::npos) {
				char first = rule.pattern_[pos + 1];
				if (isalpha(first) || first == '_')
					throw ParseError("Undefined macro in pattern", reader_.context(), pos);
			}
		}
	}
}