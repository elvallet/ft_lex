#pragma once

#include <vector>
#include <string>

#include "Token.hpp"

namespace automata {

struct Parser {
public:
	std::vector<Token> parse(const std::string& regex);

private:
	std::vector<Token> tokenize_and_insert_concat(const std::string& regex);
	std::vector<Token> shunting_yard(const std::vector<Token>& infixe);

	bool should_insert_concat(const Token& left, const Token& right);
	bool stack_has_prio(const Token& top, const Token& curr);
};

}