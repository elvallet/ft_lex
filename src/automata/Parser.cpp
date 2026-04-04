/**
 * @file Parser.cpp
 * @brief Implementation of regex parsing into postfix token streams.
 */

#include "Parser.hpp"
#include <stdexcept>
#include <stack>
#include <map>

using namespace automata; using namespace std;

namespace {

	void add_ascii_char(vector<bool>& allowed, int c) {
		if (c < 0 || c > 127)
			throw runtime_error("Character class only supports ASCII characters");
		allowed[c] = true;
	}

	void add_named_class(vector<bool>& allowed, const string& name) {
		if (name == "alpha") {
			for (int c = 'a'; c <= 'z'; ++c) add_ascii_char(allowed, c);
			for (int c = 'A'; c <= 'Z'; ++c) add_ascii_char(allowed, c);
		} else if (name == "digit") {
			for (int c = '0'; c <= '9'; ++c) add_ascii_char(allowed, c);
		} else if (name == "alnum") {
			add_named_class(allowed, "alpha");
			add_named_class(allowed, "digit");
		} else if (name == "upper") {
			for (int c = 'A'; c <= 'Z'; ++c) add_ascii_char(allowed, c);
		} else if (name == "lower") {
			for (int c = 'a'; c <= 'z'; ++c) add_ascii_char(allowed, c);
		} else if (name == "space") {
			add_ascii_char(allowed, ' ');
			add_ascii_char(allowed, '\t');
			add_ascii_char(allowed, '\n');
			add_ascii_char(allowed, '\r');
			add_ascii_char(allowed, '\f');
			add_ascii_char(allowed, '\v');
		} else if (name == "blank") {
			add_ascii_char(allowed, ' ');
			add_ascii_char(allowed, '\t');
		} else if (name == "xdigit") {
			for (int c = '0'; c <= '9'; ++c) add_ascii_char(allowed, c);
			for (int c = 'a'; c <= 'f'; ++c) add_ascii_char(allowed, c);
			for (int c = 'A'; c <= 'F'; ++c) add_ascii_char(allowed, c);
		} else if (name == "punct") {
			for (int c = 33; c <= 126; ++c) {
				if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')))
					add_ascii_char(allowed, c);
			}
		} else if (name == "print") {
			for (int c = 32; c <= 126; ++c) add_ascii_char(allowed, c);
		} else if (name == "graph") {
			for (int c = 33; c <= 126; ++c) add_ascii_char(allowed, c);
		} else if (name == "cntrl") {
			for (int c = 0; c <= 31; ++c) add_ascii_char(allowed, c);
			add_ascii_char(allowed, 127);
		} else {
			throw runtime_error("Unknown character class: " + name);
		}
	}

	vector<int> parse_char_class_atom(const string& regex, size_t& pos) {
		if (pos >= regex.size())
			throw runtime_error("Unclosed character class");

		if (regex[pos] == '\\') {
			if (++pos >= regex.size())
				throw runtime_error("Invalid escape sequence in character class");
			return {static_cast<unsigned char>(translate_escaped_char(regex[pos++]))};
		}

		if (regex[pos] == '[' && pos + 1 < regex.size() && regex[pos + 1] == ':') {
			size_t close = regex.find(":]", pos + 2);
			bool legacy = false;

			if (close == string::npos) {
				close = regex.find(']', pos + 2);
				legacy = true;
			}
			if (close == string::npos)
				throw runtime_error("Unclosed named character class");

			string name = regex.substr(pos + 2, close - (pos + 2));
			if (name.empty())
				throw runtime_error("Empty named character class");

			vector<bool> allowed(128, false);
			add_named_class(allowed, name);
			pos = legacy ? close + 1 : close + 2;

			vector<int> chars;
			for (int c = 0; c < 128; ++c) {
				if (allowed[c])
					chars.push_back(c);
			}
			return chars;
		}

		return {static_cast<unsigned char>(regex[pos++])};
	}

	vector<Token> build_char_class_tokens(const vector<bool>& allowed) {
		vector<Token> tokens;
		bool first = true;

		for (int c = 0; c < 128; ++c) {
			if (!allowed[c])
				continue;
			if (!first)
				tokens.push_back(Token{UNION, '\0'});
			tokens.push_back(Token{CHAR, static_cast<char>(c)});
			first = false;
		}

		if (tokens.empty())
			throw runtime_error("Empty character class");

		if (tokens.size() == 1)
			return tokens;

		tokens.insert(tokens.begin(), Token{LPAREN, '\0'});
		tokens.push_back(Token{RPAREN, '\0'});
		return tokens;
	}

	vector<Token> build_wildcard_tokens() {
		vector<bool> allowed(128, true);
		allowed['\n'] = false;
		return build_char_class_tokens(allowed);
	}

	const map<char, char>	ESCAPED = {
		{'n', '\n'},
		{'t', '\t'},
		{'r', '\r'},
		{'\\', '\\'},
		{'[', '['},
		{']', ']'},
		{'(', '('},
		{')', ')'},
		{'*', '*'},
		{'+', '+'},
		{'?', '?'},
		{'|', '|'},
		{'.', '.'},
		{'\"', '\"'}
	};

	char translate_escaped_char(char c) {
		auto res = ESCAPED.find(c);
		if (res == ESCAPED.end())
			throw runtime_error(string("Invalid escape sequence: ") + c);
		
		return res->second;
	}

	vector<Token> parse_char_class(const string& regex, size_t* i) {
		vector<bool> allowed(128, false);
		size_t pos = *i + 1;
		bool negate = false;
		bool first = true;

		if (pos >= regex.size())
			throw runtime_error("Unclosed character class");
		if (regex[pos] == '^') {
			negate = true;
			++pos;
		}

		while (pos < regex.size()) {
			if (regex[pos] == ']' && !first)
				break;

			vector<int> atom;

			if (first && regex[pos] == ']') {
				atom = {']'};
				++pos;
			} else if (first && regex[pos] == '-') {
				atom = {'-'};
				++pos;
			} else {
				atom = parse_char_class_atom(regex, pos);
			}

			if (pos < regex.size() && regex[pos] == '-' && pos + 1 < regex.size() && regex[pos + 1] != ']' && atom.size() == 1) {
				++pos;
				vector<int> end_atom = parse_char_class_atom(regex, pos);
				if (end_atom.size() != 1)
					throw runtime_error("Invalid range in character class");
				if (atom[0] > end_atom[0])
					throw runtime_error("Invalid range in character class");
				for (int c = atom[0]; c <= end_atom[0]; ++c)
					add_ascii_char(allowed, c);
			} else {
				for (int c : atom)
					add_ascii_char(allowed, c);
			}

			first = false;
		}

		if (pos >= regex.size() || regex[pos] != ']')
			throw runtime_error("Unclosed character class");

		*i = pos;

		if (negate) {
			for (int c = 0; c < 128; ++c)
				allowed[c] = !allowed[c];
		}

		return build_char_class_tokens(allowed);
	}

} // namespace

/**
 * @brief Parse a regex by tokenizing then applying shunting-yard.
 * @param regex Input regex in infix form.
 * @return Postfix token sequence.
 */
vector<Token> Parser::parse(const string& regex) {
	return shunting_yard(tokenize_and_insert_concat(regex));
}

/**
 * @brief Tokenize regex and inject explicit CONCAT operators.
 * @param regex Input regex.
 * @return Infix token list with explicit concatenation operators.
 */
vector<Token> Parser::tokenize_and_insert_concat(const string& regex) {
	vector<Token>	tokens;

	if (regex.empty())
		throw runtime_error("String regex should not be empty");

	bool in_string = false;
	for (size_t i = 0; i < regex.size(); i++) {
		char c	= regex[i];

		if (c == '\"') {
			in_string = !in_string;
			continue;
		}
		if (in_string) {
			Token t;
			if (c == '\\') {
				if (++i >= regex.size())
					throw runtime_error("Invalid escape sequence: trailing '\\'");
				t = Token{CHAR, translate_escaped_char(regex[i])};
			} else {
				t = Token{CHAR, regex[i]};
			}
			if (!tokens.empty() && should_insert_concat(tokens.back(), t))
				tokens.push_back(Token{CONCAT, '\0'});
			tokens.push_back(t);
			continue;
		} else {
			Token t;
			if (c == '\\') {
				if (++i >= regex.size())
					throw runtime_error("Invalid escape sequence: trailing '\\'");
				t = Token{CHAR, translate_escaped_char(regex[i])};
			} else if (c == '.') {
				vector<Token> wildcard_tokens = build_wildcard_tokens();
				for (const Token& wildcard_token : wildcard_tokens) {
					if (!tokens.empty() && should_insert_concat(tokens.back(), wildcard_token))
						tokens.push_back(Token{CONCAT, '\0'});
					tokens.push_back(wildcard_token);
				}
				continue;
			} else if (c == '[') {
				vector<Token> class_tokens = parse_char_class(regex, &i);
				for (const Token& class_token : class_tokens) {
					if (!tokens.empty() && should_insert_concat(tokens.back(), class_token))
						tokens.push_back(Token{CONCAT, '\0'});
					tokens.push_back(class_token);
				}
				continue;
			} else {
				t.type_		=
					c == '|' ? UNION :
						c == '*' ? STAR :
							c == '+' ? PLUS :
								c == '?' ? QUESTION :
									c == '(' ? LPAREN :
										c == ')' ? RPAREN : CHAR;
				t.value_ = (t.type_ == CHAR) ? c : '\0';
			}

			if (!tokens.empty() && should_insert_concat(tokens.back(), t))
				tokens.push_back(Token{CONCAT, '\0'});

			tokens.push_back(t);
		}
	}

	return tokens;
}

/**
 * @brief Convert infix tokens to postfix notation.
 * @param infi Infix token stream.
 * @return Postfix token stream.
 */
vector<Token> Parser::shunting_yard(const vector<Token>& infix) {
	vector<Token>	postfixe;
	stack<Token>	worklist;

	for (const Token& t : infix) {
		if (t.type_ == CHAR) {
			postfixe.push_back(t);
		} else if (t.type_ == LPAREN) {
			worklist.push(t);
		} else if (t.type_ == RPAREN) {
			while (!worklist.empty() && worklist.top().type_ != LPAREN) {
				postfixe.push_back(worklist.top());
				worklist.pop();
			}
			if (!worklist.empty() && worklist.top().type_ == LPAREN)
				worklist.pop();
			else
				throw runtime_error("Parenthesis not closed");
		} else {
			while (!worklist.empty() && stack_has_prio(worklist.top(), t)) {
				postfixe.push_back(worklist.top());
				worklist.pop();
			}
			worklist.push(t);
		}
	}

	while (!worklist.empty()) {
		if (worklist.top().type_ == LPAREN)
			throw runtime_error("Parenthesis not closed");
		postfixe.push_back(worklist.top());
		worklist.pop();
	}

	return postfixe;
}

/**
 * @brief Determine whether implicit concatenation exists between two tokens.
 * @param left Left token.
 * @param right Right token.
 * @return True when a CONCAT token should be inserted.
 */
bool Parser::should_insert_concat(const Token& left, const Token& right) {
	return (left.type_ == CHAR || left.type_ == STAR || left.type_ == PLUS || left.type_ == QUESTION || left.type_ == RPAREN)
		&& (right.type_ == CHAR || right.type_ == LPAREN);
}

/**
 * @brief Compare operator precedence for shunting-yard stack handling.
 * @param top Operator currently on stack.
 * @param curr Operator currently being processed.
 * @return True when @p top should be popped before @p curr is pushed.
 */
bool Parser::stack_has_prio(const Token& top, const Token& curr) {
	int t	= top.type_ == LPAREN ? 0 : top.type_ == UNION ? 1 : top.type_ == CONCAT ? 2 : 3;
	int c	= curr.type_ == LPAREN ? 0 : curr.type_ == UNION ? 1 : curr.type_ == CONCAT ? 2 : 3;

	return t >= c;
}