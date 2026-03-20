/**
 * @file Parser.cpp
 * @brief Implementation of regex parsing into postfix token streams.
 */

#include "Parser.hpp"
#include <stdexcept>
#include <stack>

using namespace automata; using namespace std;

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

	for (char c : regex) {
		Token	t;

		t.value_	= c;
		t.type_		=
			isalnum(c) ? CHAR :
				c == '|' ? UNION :
					c == '*' ? STAR :
						c == '+' ? PLUS :
							c == '?' ? QUESTION :
								c == '(' ? LPAREN :
									c == ')' ? RPAREN :
										throw runtime_error("Invalid character :" + c);

		if (!tokens.empty() && should_insert_concat(tokens.back(), t))
			tokens.push_back(Token{CONCAT});

		tokens.push_back(t);
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