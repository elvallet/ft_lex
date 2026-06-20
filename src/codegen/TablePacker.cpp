#include "TablePacker.hpp"
#include <algorithm>
#include <numeric>
#include <stdexcept>

using namespace codegen;


// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TablePacker::TablePacker()
	: low_water_(0)
{}


// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

PackedTables TablePacker::pack(const std::vector<std::unordered_map<char, int>>& transitions, int sink_state)
{
	const size_t	nb_states	= transitions.size();

	// Initialise member tables.
	low_water_	= 0;
	base_.assign(nb_states, -1);
	next_.assign(256, -1);		// grows on demand
	check_.assign(256, -1);

	// 1. Build profiles (char cast to unsigned to avoid negative indices).
	std::vector<Profile>	profiles;
	profiles.reserve(nb_states);
	for (size_t s = 0; s < nb_states; ++s)
		profiles.push_back(build_profile(transitions[s], sink_state));

	// 2. Sort states by descending profile size (densest first).
	std::vector<int> order(nb_states);
	std::iota(order.begin(), order.end(), 0);
	std::sort(order.begin(), order.end(), [&](int a, int b) {
		return profiles[a].size() > profiles[b].size();
	});

	// 3. Place each state.
	for (int s : order) {
		if (profiles[s].empty()) {
			// State with no outgoing transitions: no slots needed, offset 0 is fine.
			base_[s]	= 0;
			continue;
		}
		int offset	= find_offset(profiles[s]);
		place(s, offset, profiles[s]);
	}

	// 4. Build def.
	std::vector<int> def(nb_states, -1);

	return PackedTables{ base_, next_, check_, def };
}


// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

TablePacker::Profile
TablePacker::build_profile(const std::unordered_map<char, int>& row, int sink_state) const
{
	Profile p;
	p.reserve(row.size());
	for (auto& [c, dst] : row) {
		if (dst == sink_state) continue;
		// Cast to unsigned char so that values are always in [0, 255].
		p.emplace_back(static_cast<unsigned char>(c), dst);
	}
	// Sort by character value for deterministic placement.
	std::sort(p.begin(), p.end());
	return p;
}

bool TablePacker::collides(int offset, const Profile& profile) const
{
	for (auto& [c, dst] : profile) {
		size_t idx = static_cast<size_t>(offset + c);
		// If within current bounds and already owned by another state -> collision.
		if (idx < check_.size() && check_[idx] != -1)
			return true;
	}

	return false;
}

int TablePacker::find_offset(const Profile& profile)
{
	int offset	= low_water_;
	while (collides(offset, profile))
		++offset;
	return offset;
}

void TablePacker::place(int state, int offset, const Profile& profile)
{
	// Ensure next_/check_ are large enough for the highest character slot.
	// Worst case : offset + 255 + 1.
	ensure_size(static_cast<size_t>(offset) + 256);

	for (auto& [c, dst] : profile) {
		size_t idx	= static_cast<size_t>(offset + c);
		check_[idx]	= state;
		next_[idx]	= dst;
	}

	base_[state]	= offset;

	// Advance low_water_ past any now-occupied leading slots.
	while (static_cast<size_t>(low_water_) < check_.size()
			&& check_[low_water_] != -1)
		++low_water_;
}

void TablePacker::ensure_size(size_t needed)
{
	if (needed > next_.size()) {
		next_.resize(needed, -1);
		check_.resize(needed, -1);
	}
}