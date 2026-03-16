#pragma once

namespace automata {

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

struct Token {
	TokenType	type_;
	char		value_;
};

} // namespace automata