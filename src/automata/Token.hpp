/**
 * @file Token.hpp
 * @brief Token definitions used by the regex parser and automata builders.
 */
#pragma once

#include <bitset>

namespace automata {

/**
 * @brief Token categories recognized in regex expressions.
 */
enum TokenType {
	CHAR,
	/** @brief Character class over ASCII domain [0..127], stored in Token::charset_. */
	CHARCLASS,
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
	TokenType					type_;
	/** @brief Raw character value when relevant (for example for CHAR). */
	char						value_;
	/**
	 * @brief Character set payload for CHARCLASS tokens.
	 *
	 * Bit i corresponds to ASCII code i (0 <= i < 128).
	 * This keeps character classes compact and avoids desugaring into large alternations.
	 */
	std::bitset<128>			charset_;

	/** @brief Default token: CHAR with '\0' value and empty charset. */
	Token() : type_(CHAR), value_('\0'), charset_() {}
	/** @brief Build an operator-like token (UNION, CONCAT, STAR, ...). */
	explicit Token(TokenType t) : type_(t), value_('\0'), charset_() {}
	/** @brief Build a literal CHAR token. */
	Token(TokenType t, char v) : type_(t), value_(v), charset_() {}
	/** @brief Build a CHARCLASS token from an ASCII bitset. */
	Token(TokenType t, const std::bitset<128>& cs) : type_(t), value_('\0'), charset_(cs) {}
};

} // namespace automata