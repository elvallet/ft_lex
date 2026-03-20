/**
 * @file Thompson.cpp
 * @brief Implementation of Thompson NFA construction.
 */
#include "Thompson.hpp"
#include <stack>
#include <stdexcept>

using namespace automata; using namespace std;

/**
 * @brief Compile postfix regex tokens into an NFA.
 * @param postfix Regex tokens in postfix notation.
 * @return Constructed NFA.
 */
NFA Thompson::compile(const vector<Token>& postfix) {
	stack<Fragment>	worklist;

	for (const Token& t : postfix) {
		switch (t.type_)
		{
		case CHAR:
		{
			worklist.push(make_literal(t.value_));
			break;
		}
		case CONCAT:
		{
			if (worklist.size() < 2) throw runtime_error("Invalid regex");

			Fragment b	= worklist.top();
			worklist.pop();

			Fragment a	= worklist.top();
			worklist.pop();

			worklist.push(make_concat(a, b));
			break;
		}
		case UNION:
		{
			if (worklist.size() < 2) throw runtime_error("Invalid regex");

			Fragment a	= worklist.top();
			worklist.pop();

			Fragment b	= worklist.top();
			worklist.pop();

			worklist.push(make_union(a, b));
			break;
		}
		case STAR:
		{
			if (worklist.empty()) throw runtime_error("Invalid regex");

			Fragment a = worklist.top();
			worklist.pop();

			worklist.push(make_star(a));
			break;
		}
		case PLUS:
		{
			if (worklist.empty()) throw runtime_error("Invalid regex");

			Fragment a = worklist.top();
			worklist.pop();

			worklist.push(make_plus(a));
			break;
		}
		case QUESTION:
		{	
			if (worklist.empty()) throw runtime_error("Invalid regex");

			Fragment a = worklist.top();
			worklist.pop();

			worklist.push(make_question(a));
			break;
		}
		default:
			throw runtime_error("Invalid token");
		}
	}

	if (worklist.empty()) throw runtime_error("Invalid regex");
	Fragment	frag	= worklist.top();
	int 		final	= add_state();

	patch(frag.out_, final);
	nfa_.initial_state_	= frag.start_;
	nfa_.final_states_	= {final};

	return nfa_;
}

/**
 * @brief Add one state to the current NFA.
 * @return Newly allocated state id.
 */
int Thompson::add_state() {
	int	id = nfa_.transitions_.size();

	nfa_.transitions_.push_back({});
	nfa_.epsilon_transitions_.push_back({});

	return id;
}

/**
 * @brief Resolve dangling outputs to a target state.
 * @param out Outputs to patch.
 * @param target Target state id.
 */
void Thompson::patch(vector<DanglingOut>&out, int target) {
	for (const auto &o : out) {
		if (o.is_epsilon_) {
			nfa_.epsilon_transitions_[o.state_].push_back(target);
		} else {
			nfa_.transitions_[o.state_][o.c_].push_back(target);
		}
	}
}

/**
 * @brief Create an NFA fragment for a literal symbol.
 * @param c Input symbol.
 * @return Literal fragment.
 */
Fragment Thompson::make_literal(char c) {
	int first_state		= add_state();
	int second_state	= add_state();
	vector<int>	v;

	v.push_back(second_state);
	nfa_.transitions_[first_state][c]	= v;
	nfa_.alphabet_.insert(c);

	Fragment frag;
	frag.start_	= first_state;
	frag.out_.push_back({second_state, true, 0});

	return frag;
}

/**
 * @brief Concatenate two fragments.
 * @param a Left fragment.
 * @param b Right fragment.
 * @return Concatenated fragment.
 */
Fragment Thompson::make_concat(Fragment a, Fragment b) {
	patch(a.out_, b.start_);

	return Fragment{a.start_, b.out_};
}

/**
 * @brief Build alternation between two fragments.
 * @param a First branch fragment.
 * @param b Second branch fragment.
 * @return Union fragment.
 */
Fragment Thompson::make_union(Fragment a, Fragment b) {
	int s	= add_state();

	nfa_.epsilon_transitions_[s].push_back(a.start_);
	nfa_.epsilon_transitions_[s].push_back(b.start_);

	a.out_.insert(a.out_.end(), b.out_.begin(), b.out_.end());
	return Fragment{s, std::move(a.out_)};
}

/**
 * @brief Apply Kleene star to a fragment.
 * @param a Input fragment.
 * @return Star fragment.
 */
Fragment Thompson::make_star(Fragment a) {
	int s	= add_state();

	nfa_.epsilon_transitions_[s].push_back(a.start_);
	patch(a.out_, s);

	vector<DanglingOut>	v;
	v.push_back({s, true, 0});

	return Fragment{s, v};
}

/**
 * @brief Apply one-or-more operator to a fragment.
 * @param a Input fragment.
 * @return Plus fragment.
 */
Fragment Thompson::make_plus(Fragment a) {
	int s	= add_state();

	nfa_.epsilon_transitions_[s].push_back(a.start_);
	patch(a.out_, s);

	vector<DanglingOut>	v;
	v.push_back({s, true, 0});

	return Fragment{a.start_, v};
}

/**
 * @brief Apply zero-or-one operator to a fragment.
 * @param a Input fragment.
 * @return Question-mark fragment.
 */
Fragment Thompson::make_question(Fragment a) {
	int s	= add_state();

	nfa_.epsilon_transitions_[s].push_back(a.start_);

	a.out_.push_back({s, true, 0});
	return Fragment{s, std::move(a.out_)};
}