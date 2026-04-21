#include "LexParser.hpp"
#include "../automata/Parser.hpp"

using namespace automata; using namespace lexer_file; using namespace std;

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
	compile_trailing_length();
	
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

int compute_fixed_length(const vector<Token>& postfix)
{
	vector<int> lengths;

	for (const Token& token : postfix) {
		switch (token.type_) {
			case CHAR:
			case CHARCLASS:
				lengths.push_back(1);
				break;
			case CONCAT:
			{
				// CONCAT combines two fixed-length fragments into one fixed-length result.
				if (lengths.size() < 2)
					throw runtime_error("invalid trailing context");
				int right = lengths.back();
				lengths.pop_back();
				int left = lengths.back();
				lengths.pop_back();
				lengths.push_back(left + right);
				break;
			}
			case UNION:
			{
				// UNION is fixed-length only if both branches have the exact same length.
				if (lengths.size() < 2)
					throw runtime_error("invalid trailing context");
				int right = lengths.back();
				lengths.pop_back();
				int left = lengths.back();
				lengths.pop_back();
				if (left != right)
					throw runtime_error("variable-length trailing context is not supported");
				lengths.push_back(left);
				break;
			}
			default:
				throw runtime_error("variable-length trailing context is not supported");
		}
	}

	if (lengths.size() != 1)
		throw runtime_error("invalid trailing context");

	return lengths.back();
}

} // namespace

vector<string> LexParser::extract_conditions(const std::string& line, size_t* i)
{
	string			conditions;
	vector<string>	split_conditions;
	bool found = false;

	(*i)++;
	while (*i < line.size()) {
		if (line[*i] == '>')
		{
			(*i)++;
			found = true;
			break;
		}
		conditions.push_back(line[*i]);
		(*i)++;
	}

	if (!found)
		throw runtime_error("'<' not closed");

	size_t oldpos = 0;
	size_t pos = conditions.find(',');
	while (pos != string::npos) {
		split_conditions.push_back(conditions.substr(oldpos, pos - oldpos));
		oldpos = pos + 1;
		pos = conditions.find(',', oldpos);
	}
	split_conditions.push_back(conditions.substr(oldpos));

	for (auto& condition : split_conditions) {
		trim(condition, " \t\n\r\f\v");
		if (condition == "*") {
			// `<*>` means: activate this rule in every declared start condition.
			vector<string> all;
			for (auto& [name, _] : lex_file_.conditions_) all.push_back(name);
			return all;
		}
		if (condition != "INITIAL" && lex_file_.conditions_.find(condition) == lex_file_.conditions_.end()) {
			throw ParseError("Undefined condition: " + condition, reader_.context(), 0);
		}
	}

	return split_conditions;
}

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

pair<string, string> LexParser::detect_trailing(const string& raw)
{
	if (raw.empty())
		return {"", ""};

	bool	bracket	= false;
	bool	quote	= false;
	bool	found	= false;
	bool	anchor	= false;
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
		} else if (c == '/' && !quote && !bracket) {
			// Unescaped '/' outside [] and "" splits pattern/trailing.
			found = true;
			break;
		} else if (c == '$' && !quote && !bracket) {
			if (i != raw.size() - 1)
				throw ParseError("`$` is valid only as the last character of a line", reader_.context(), int(i));
			anchor = true;
			break;
		}
		pattern.push_back(c);
		i++;
	}
	

	if (quote || bracket) {
		throw ParseError("Brackets or quotes should always be closed", reader_.context(), int(i));
	}

	if (!found && !anchor) {
		return {raw, ""};
	}

	if (anchor) {
		string trailing = "\n";
		return {pattern, trailing};
	}

	string trailing = raw.substr(i + 1);

	// Trailing context must stay fixed-length because codegen rewinds with yyless().
	size_t e = trailing.find_first_of("*?+");
	if (e != string::npos) {
		throw ParseError("variable-length trailing context is not supported", reader_.context());
	}
	if (trailing.find('{') != string::npos) {
		size_t close = trailing.find('}');
		if (close != string::npos) {
			string content = trailing.substr(trailing.find('{') + 1, close - trailing.find('{') - 1);
			if (content.back() == ',')
				throw ParseError("variable-length trailing context is not supported", reader_.context());
		}
	}
	return {pattern, trailing};
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
				case State::LINE_COMMENT:
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
	vector<string>	conditions;
	size_t	index	= 0;
	bool	bol		= false;

	if (!line.empty() && line[0] == '<') 
		conditions	= extract_conditions(line, &index);
	else
		conditions.push_back("INITIAL");
	
	auto split = split_pattern_action(line.substr(index));

	if (!split.first.empty() && split.first[0] == '^') {
		bol = true;
		split.first = split.first.substr(1);
	}

	auto pattern = detect_trailing(split.first);

	if (split.second == "|") {
		return Rule{conditions, split.first, "", split.second, -1, true, bol};
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

	return Rule{conditions, pattern.first, pattern.second, completed_action, -1, false, bol};
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
			try {
				auto line = reader_.next();
				parse_conditions(line->content_);
			} catch (exception &e) {
				throw ParseError(string("Unsupported directive: ") + e.what(), reader_.context(), 0);
			}
		} else {
			auto line = reader_.next();
			lex_file_.macros_.push_back(parse_macro_line(line->content_));
		}
	}
}

void LexParser::parse_conditions(const string& line)
{
	bool	excl;
	size_t	i		= 1;

	if (i >= line.size()) throw runtime_error("missing directive after '%'");
	if (line[i] == 'p' || line[i] == 'n' || line[i] == 'a' || line[i] == 'e' || line[i] == 'o')
		return;
	if (line[i] != 's' && line[i] != 'x') throw runtime_error(string("unknown directive %") + line[i]);

	// `%x` declares exclusive states, `%s` declares inclusive ones.
	excl = line[i++] == 'x';
	while (i < line.size() && is_whitespace(line[i])) i++;

	size_t pos = line.find_first_of(" \t", i);
	while (pos != string::npos) {
		lex_file_.conditions_.insert({line.substr(i, pos - i), excl});
		i = pos + 1;
		pos = line.find_first_of(" \t", i);
	}
	lex_file_.conditions_.insert({line.substr(i, pos - i), excl});
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
		for (size_t j = i + 1; j < lex_file_.macros_.size(); j++) {
			auto& macro = lex_file_.macros_[i];
			string pattern = "{" + macro.first + "}";
			size_t pos = 0;
			while ((pos = lex_file_.macros_[j].second.find(pattern, pos)) != string::npos) {
				lex_file_.macros_[j].second.replace(pos, pattern.length(), "(" + macro.second + ")");
				pos += macro.second.length() + 2;
			}
		}
	}

	for (auto& rule : lex_file_.rules_) {
		for (const auto& macro : lex_file_.macros_) {
			string pattern = "{" + macro.first + "}";
			size_t pos = 0;
			while ((pos = rule.trailing_.find(pattern, pos)) != string::npos) {
				rule.trailing_.replace(pos, pattern.length(), "(" + macro.second + ")");
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

void LexParser::compile_trailing_length()
{
	automata::Parser parser;

	for (Rule& rule : lex_file_.rules_) {
		if (!rule.trailing_.empty()) {
			// Precompute trailing length so generated code can emit yyless(yyleng - N).
			rule.trailing_length_ = compute_fixed_length(parser.parse(rule.trailing_));
		}
	}
}