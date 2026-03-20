/**
 * @file Parser.hpp
 * @brief Regex parser that converts infix expressions into postfix tokens.
 */
#pragma once

#include <vector>
#include <string>

#include "Token.hpp"

namespace automata {

/**
 * @brief Parses regex strings into postfix token streams.
 */
struct Parser {
public:
	/**
	 * @brief Parse a regex into postfix notation.
	 * @param regex Input regex in infix form.
	 * @return Postfix token sequence.
	 */
	std::vector<Token> parse(const std::string& regex);

private:
	/**
	 * @brief Tokenize input and insert explicit concatenation tokens.
	 * @param regex Input regex.
	 * @return Tokenized expression with CONCAT operators inserted.
	 */
	std::vector<Token> tokenize_and_insert_concat(const std::string& regex);
	/**
	 * @brief Convert infix tokens to postfix using the shunting-yard algorithm.
	 * @param infix Infix token sequence.
	 * @return Postfix token sequence.
	 */
	std::vector<Token> shunting_yard(const std::vector<Token>& infix);

	/**
	 * @brief Check whether a CONCAT token is needed between two tokens.
	 * @param left Left token.
	 * @param right Right token.
	 * @return True when explicit concatenation must be inserted.
	 */
	bool should_insert_concat(const Token& left, const Token& right);
	/**
	 * @brief Compare precedence between stack top and current token.
	 * @param top Operator on stack.
	 * @param curr Current operator.
	 * @return True when @p top has higher or equal priority.
	 */
	bool stack_has_prio(const Token& top, const Token& curr);
};

}