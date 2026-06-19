/**
 * @file Parser.cpp
 * @brief Implementation of regex parsing into postfix token streams.
 */

#include "Parser.hpp"
#include <stdexcept>
#include <stack>
#include <map>
#include <bitset>

using namespace automata; using namespace std;

namespace {
	char translate_escape_sequence(const string& regex, size_t& pos);

	void add_ascii_char(bitset<128>& allowed, int c) {
		if (c < 0 || c > 127)
			throw runtime_error("Character class only supports ASCII characters");
		allowed.set(static_cast<size_t>(c));
	}

	void add_named_class(bitset<128>& allowed, const string& name) {
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
			return {static_cast<unsigned char>(translate_escape_sequence(regex, pos))};
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

			bitset<128> allowed;
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

	vector<Token> build_char_class_tokens(const bitset<128>& allowed) {
		if (allowed.none())
			throw runtime_error("Empty character class");

		return {Token(CHARCLASS, allowed)};
	}

	vector<Token> build_wildcard_tokens() {
		bitset<128> allowed;
		allowed.set();
		allowed.reset('\n');
		return build_char_class_tokens(allowed);
	}

	char translate_escape_sequence(const string& regex, size_t& pos) {
		if (pos >= regex.size())
			throw runtime_error("Invalid escape sequence: trailing '\\'");

		char c = regex[pos];

		if (c == 'x') {
			++pos;
			if (pos >= regex.size() || !isxdigit((unsigned char)regex[pos]))
				throw runtime_error("Invalid hex escape sequence");
			int val = 0;
			for (int k = 0; k < 2 && pos < regex.size() && isxdigit((unsigned char)regex[pos]); ++k, ++pos)
				val = val * 16 + (isdigit((unsigned char)regex[pos])
					? regex[pos] - '0'
					: tolower((unsigned char)regex[pos]) - 'a' + 10);
			return static_cast<char>(val);
		}

		if (c >= '0' && c <= '7') {
			int val = 0;
			for (int k = 0; k < 3 && pos < regex.size() && regex[pos] >= '0' && regex[pos] <= '7'; ++k, ++pos)
				val = val * 8 + (regex[pos] - '0');
			return static_cast<char>(val);
		}

		++pos;
		static const map<char, char> ESCAPED = {
			{'n', '\n'}, {'t', '\t'}, {'r', '\r'}, {'a', '\a'}, {'b', '\b'},
			{'f', '\f'}, {'v', '\v'}, {'0', '\0'},
			{'\\', '\\'}, {'[', '['}, {']', ']'}, {'(', '('}, {')', ')'},
			{'*', '*'},  {'+', '+'}, {'?', '?'}, {'|', '|'},  {'.', '.'},
			{'"', '"'},  {'/', '/'}, {'^', '^'}, {'-', '-'},  {'{', '{'},
			{'}', '}'}, {'<', '<'}, {'>', '>'},  {'$', '$'},
		};
		auto it = ESCAPED.find(c);
		if (it == ESCAPED.end())
			return c;	/* unknown escape: pass char through, as flex does */
		return it->second;
	}

	vector<Token> parse_char_class(const string& regex, size_t* i) {
		bitset<128> allowed;
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
			size_t dash_pos = pos;
			++pos;
			vector<int> end_atom = parse_char_class_atom(regex, pos);
			if (end_atom.size() == 1 && atom[0] <= end_atom[0]) {
				for (int c = atom[0]; c <= end_atom[0]; ++c)
					add_ascii_char(allowed, c);
			} else {
				for (int c : atom)
					add_ascii_char(allowed, c);
				add_ascii_char(allowed, '-');
				pos = dash_pos + 1;
			}
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
				allowed.flip(static_cast<size_t>(c));
		}

		return build_char_class_tokens(allowed);
	}

	vector<Token> get_last_fragment(const vector<Token>& v)
	{
		vector<Token>	res;
		int i = (int)v.size() - 1;

		// Repetitions apply to the previous atom/group, not to trailing unary quantifiers.

		while (i >= 0 && (v[i].type_ == STAR || v[i].type_ == PLUS || v[i].type_ == QUESTION))
			i--;

		if (i < 0)
			throw runtime_error("invalid repetition sequence");

		if (v[i].type_ == CHAR || v[i].type_ == CHARCLASS) {
			res.push_back(v[i]);
			return res;
		}

		if (v[i].type_ == RPAREN) {
			int end = i;
			int depth = 0;
			while (i >= 0) {
				if (v[i].type_ == RPAREN) depth++;
				if (v[i].type_ == LPAREN) depth--;
				if (depth == 0) break;
				i--;
			}
			if (i < 0 || v[i].type_ != LPAREN)
				throw runtime_error("invalid repetition sequence");
			for (int j = i; j <= end; j++)
				res.push_back(v[j]);
			return res;
		}

		throw runtime_error("invalid repetition sequence");
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
				++i;
				t = Token{CHAR, translate_escape_sequence(regex, i)};
				--i;
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
				++i;
				t = Token{CHAR, translate_escape_sequence(regex, i)};
				--i;
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
			} else if (c == '{') {
				// Expand bounded repetitions into CONCAT/QUESTION/PLUS over the previous fragment:
				// {n}   => fragment repeated n times
				// {n,m} => n mandatory + (m - n) optional copies
				// {n,}  => n mandatory + one-or-more for the last mandatory copy
				vector<Token> to_copy = get_last_fragment(tokens);
				i++;

				string	mandatory;
				while (i < regex.size() && regex[i] != ',' && regex[i] != '}') {
					mandatory.push_back(regex[i]);
					i++;
				}
				int n = stoi(mandatory);

				for (int k = 1; k < n; k++) {
					tokens.push_back(Token{CONCAT, '\0'});
					tokens.insert(tokens.end(), to_copy.begin(), to_copy.end());
				}

				if (regex[i] == ',') {
					i++;
					if (regex[i] == '}') {
						tokens.push_back(Token{PLUS, '\0'});
					} else {
						string opt;
						while (i < regex.size() && regex[i] != '}') {
							opt.push_back(regex[i]);
							i++;
						}
						int m = stoi(opt) - n;
						for (int k = 0; k < m; k++) {
							tokens.push_back(Token{CONCAT, '\0'});
							tokens.insert(tokens.end(), to_copy.begin(), to_copy.end());
							tokens.push_back(Token{QUESTION, '\0'});
						}
					}
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
	return (left.type_ == CHAR || left.type_ == CHARCLASS || left.type_ == STAR || left.type_ == PLUS || left.type_ == QUESTION || left.type_ == RPAREN)
		&& (right.type_ == CHAR || right.type_ == CHARCLASS || right.type_ == LPAREN);
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