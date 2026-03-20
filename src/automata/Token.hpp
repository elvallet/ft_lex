/**
 * @file Token.hpp
 * @brief Token definitions used by the regex parser and automata builders.
 */
#pragma once

namespace automata {

/**
 * @brief Token categories recognized in regex expressions.
 */
enum TokenType {
	CHAR,
	UNION,
	CONCAT,
	STAR,
	PLUS,
	QUESTION,
	LPAREN,
	RPAREN,
};

/**
 * @brief Single regex token.
 */
struct Token {
	/** @brief Token kind. */
	TokenType	type_;
	/** @brief Raw character value when relevant (for example for CHAR). */
	char		value_;
};

} // namespace automata