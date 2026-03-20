#pragma once

#include <vector>

#include "NFA.hpp"
#include "Token.hpp"


namespace automata {

struct DanglingOut {
	int		state_;
	bool	is_epsilon_;
	char	c_;
};

struct Fragment {
	int							start_;
	std::vector<DanglingOut>	out_;
};

class Thompson {
public:
	NFA compile(const std::vector<Token>& postfixe);

private:
	NFA	nfa_;

	int		add_state();
	void	patch(std::vector<DanglingOut>& out, int target);

	Fragment	make_literal(char c);
	Fragment	make_concat(Fragment a, Fragment b);
	Fragment	make_union(Fragment a, Fragment b);
	Fragment	make_star(Fragment a);
	Fragment	make_plus(Fragment a);
	Fragment	make_question(Fragment a);
};

} // namespace automata