#include "DefaultChainBuilder.hpp"
#include <cassert>

using namespace codegen;
using namespace std;

DefaultChainBuilder::DefaultChainBuilder(std::vector<std::vector<int>> rows, size_t size)
	: rows_(std::move(rows)),
	nb_states_(size),
	alphabet_size_(rows_.empty() ? 0 : rows_[0].size()),
	in_tree_(size, false),
	best_score_(size, -1),
	best_source_(size, -1)
{ }

std::vector<int> DefaultChainBuilder::prim(int initial_state) {
	vector<int>	parent(nb_states_, -1);

	in_tree_[initial_state]	= true;
	size_t	placed	= 1;

	for (size_t s = 0; s < nb_states_; ++s) {
		if (in_tree_[s]) continue;
		update_state(initial_state, static_cast<int>(s));
	}

	while (placed < nb_states_) {
		int	best	= -1;
		int	curr	= -1;

		for (size_t s = 0; s < nb_states_; ++s) {
			if (in_tree_[s]) continue;
			if (best_score_[s] > best) {
				best	= best_score_[s];
				curr	= static_cast<int>(s);
			}
		}

		// The similarity graph is complete (every pair has a score, even 0),
		// so curr is always found once placed < nb_states_.
		assert(curr != -1 && "no candidate found: similarity graph should be complete");

		parent[curr]	= best_source_[curr];
		in_tree_[curr]	= true;
		++placed;

		for (size_t s = 0; s < nb_states_; ++s) {
			if (in_tree_[s]) continue;
			update_state(curr, static_cast<int>(s));
		}
	}

	return parent;
}

int DefaultChainBuilder::sim(int A, int B) {
	vector<int>&	a_row	= rows_[A];
	vector<int>&	b_row	= rows_[B];
	int				score	= 0;

	for (size_t c = 0; c < alphabet_size_; c++) {
		if (a_row[c] == b_row[c]) {
			++score;
		}
	}

	return score;
}

void DefaultChainBuilder::update_state(int last_added, int state)
{
	int score	= sim(last_added, state);

	if (score > best_score_[state]) {
		best_score_[state]	= score;
		best_source_[state]	= last_added;	 
	}
}
