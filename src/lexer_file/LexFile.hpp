#pragma once

#include <string>
#include <vector>
#include <map>

namespace lexer_file {

/**
 * @brief A lexer rule made of a regular-expression pattern and an action.
 */
struct Rule {
	std::vector<std::string>	conditions_;
	/** Raw or macro-expanded pattern matched by the lexer. */
	std::string					pattern_;
	/** User C/C++ action associated with the pattern. */
	std::string					action_;
	/**
	 * @brief True when the rule action is the pipe operator (`|`).
	 *
	 * Pipe rules inherit the action of the next non-pipe rule.
	 */
	bool		is_pipe_;
};

/**
 * @brief In-memory representation of a parsed `.l`/lex specification file.
 */
struct LexFile {
	/** Verbatim code from the top `%{ ... %}` block (definitions section). */
	std::string											verbatim_top_;
	/** User macros as pairs: `<name, regex>`. */
	std::vector<std::pair<std::string, std::string>>	macros_;
	/** Parsed rules from the rules section. */
	std::vector<Rule>									rules_;
	/** Verbatim user code found after the second `%%`. */
	std::string											verbatim_bottom_;
	/** Verbatim/indented lines inside the rules section. */
	std::vector<std::string>							verbatim_rules_;
	std::map<std::string, bool>							conditions_;	// true if exclusive (%x) false if inclusive (%s)
};

} // namespace lexer_file