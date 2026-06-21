#include "DiffProfileBuilder.hpp"


namespace codegen {
	
std::vector<Profile>	build_diff_profiles(const DenseRows& dense, const std::vector<int>& parent)
{
	std::vector<Profile>	profils(dense.rows.size());

	for (size_t s = 0; s < dense.rows.size(); ++s) {
		int			p		= parent[s];
		Profile&	profile	= profils[s];

		for (size_t col = 0; col < dense.symbols.size(); ++col) {
			int		dest	= dense.rows[s][col];
			bool	keep	= (p == -1) ? (dest != -1)
										: (dest != dense.rows[p][col]);
										
			if (keep)
				profile.emplace_back(static_cast<unsigned char>(dense.symbols[col], dest));
		}
	}

	return profils;
}

} //namespace codegen