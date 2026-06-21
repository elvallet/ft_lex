#include "DenseRows.hpp"
#include <algorithm>

using namespace codegen;
using namespace std;

DenseRows codegen::build_dense_rows(const vector<unordered_map<char, int>>& transitions, int sink)
{
	DenseRows		ds;

	// 1. Extract the symbol set from the first non-empty row, then stop --
	//	  every row shares the same key set after complete(), one is enough.
	for (const auto& row : transitions) {
		if (!row.empty()) {
			ds.symbols.reserve(row.size());
			for (const auto& [key, _value] : row)
				ds.symbols.push_back(key);
			break;
		}
	}

	sort(ds.symbols.begin(), ds.symbols.end(),
		[](char a, char b){ return static_cast<unsigned char>(a) < static_cast<unsigned char>(b); }
	);

	// 2. Build each row, indexed by COLUMN POSITTION (not raw char value),
	//	  normalising sink transitions to -1.
	ds.rows.resize(transitions.size());
	for (size_t i = 0; i < transitions.size(); ++i) {
		ds.rows[i].resize(ds.symbols.size());
		for (size_t col = 0; col < ds.symbols.size(); ++col) {
			char	c	= ds.symbols[col];
			int		dst	= transitions[i].at(c);		// throws if rows are inconsistent

			ds.rows[i][col]	= (dst == sink) ? -1 : dst;
		}
	}

	return ds;
}