#pragma once

#include <cstddef>
#include <string>

namespace automata {

struct Stats {
	bool	compression_enabled = false;

	int		macros_count = 0;
	int		rules_count = 0;
	int		start_conditions_count = 0;

	int		nfa_states = 0;

	int		dfa_states = 0;
	int		dfa_sink_states = 0;

	size_t	table_size_raw = 0;
	size_t	table_size_packed = 0;
	float	compression_ratio = 0.0f;

	std::string	output_file;
	size_t		output_bytes = 0;
};

} // namespace automata