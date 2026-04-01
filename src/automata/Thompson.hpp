/**
 * @file Thompson.hpp
 * @brief Thompson construction from postfix regex tokens to NFA.
 */

#pragma once

#include <vector>

#include "NFA.hpp"
#include "Token.hpp"

namespace automata {

/**
 * @brief Pending transition endpoint to patch during Thompson construction.
 */
struct DanglingOut {
	/** @brief Source state containing the dangling transition. */
	int		state_;
	/** @brief True when dangling edge is epsilon, false when symbol-based. */
	bool	is_epsilon_;
	/** @brief Symbol used when the edge is not epsilon. */
	char	c_;
};

/**
 * @brief Partial NFA fragment with start state and unresolved outputs.
 */
struct Fragment {
	/** @brief Fragment entry state. */
	int							start_;
	/** @brief Unpatched outgoing transitions. */
	std::vector<DanglingOut>	out_;
};

/**
 * @brief Builds an NFA from postfix regex tokens using Thompson construction.
 */
class Thompson {
public:
	/**
	 * @brief Compile a postfix token sequence into an NFA.
	 * @param postfix Regex tokens in postfix form.
	 * @return Constructed NFA.
	 */
	NFA compile(const std::vector<Token>& postfix, int index);

private:
	/** @brief NFA being built incrementally. */
	NFA	nfa_;

	/**
	 * @brief Append a new state to the current NFA.
	 * @return Newly created state id.
	 */
	int		add_state();
	/**
	 * @brief Patch all dangling outputs to a given target state.
	 * @param out Dangling outputs to patch.
	 * @param target Destination state id.
	 */
	void	patch(std::vector<DanglingOut>& out, int target);

	/** @brief Build a literal fragment for a single symbol. */
	Fragment	make_literal(char c);
	/** @brief Concatenate two fragments. */
	Fragment	make_concat(Fragment a, Fragment b);
	/** @brief Build alternation between two fragments. */
	Fragment	make_union(Fragment a, Fragment b);
	/** @brief Apply Kleene star to a fragment. */
	Fragment	make_star(Fragment a);
	/** @brief Apply one-or-more operator to a fragment. */
	Fragment	make_plus(Fragment a);
	/** @brief Apply zero-or-one operator to a fragment. */
	Fragment	make_question(Fragment a);
};

} // namespace automata