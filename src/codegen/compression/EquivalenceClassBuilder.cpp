#include "EquivalenceClassBuilder.hpp"
#include <map>

namespace codegen {

EquivalenceClasses build_equivalence_classes(const DenseRows& dense)
{
	const size_t	nb_states	= dense.rows.size();
	const size_t	nb_symbols	= dense.symbols.size();

	// 1. Group columns by identical transition vector.
	std::vector<int>				column_class(nb_symbols, -1);
	std::map<std::vector<int>, int>	columns;

	for (size_t col = 0; col < nb_symbols; ++col) {
		std::vector<int>	comp;
		comp.reserve(nb_states);
		for (size_t s = 0; s < nb_states; ++s)
			comp.push_back(dense.rows[s][col]);

		auto it	= columns.find(comp);
		if (it == columns.end()) {
			int new_id	= static_cast<int>(columns.size());
			columns.insert({comp, new_id});
			column_class[col]	= new_id;
		} else {
			column_class[col]	= it->second;
		}
	}

	const int	nb_classes	= static_cast<int>(columns.size());
	const int	phantom		= nb_classes;

	// 2. class_of[256], full byte range, defaulting to the phantom class.
	std::vector<int>	class_of(256, phantom);
	for(size_t col = 0; col < nb_symbols; ++col)
		class_of[static_cast<unsigned char>(dense.symbols[col])]	= column_class[col];
		
	// 3. Reduced DenseRows: one column per class, symbols[] = identity.
	DenseRows	reduced;
	reduced.symbols.resize(nb_classes);
	for (int k = 0; k < nb_classes; ++k)
		reduced.symbols[k] = static_cast<char>(k);

	reduced.rows.assign(nb_states, std::vector<int>(nb_classes));
	for (size_t col = 0; col < nb_symbols; ++col) {
		int k = column_class[col];
		for (size_t s = 0; s < nb_states; ++s)
			reduced.rows[s][k] = dense.rows[s][col];
	}

	return EquivalenceClasses { std::move(class_of), std::move(reduced) };
}

} // namespace codegen