#pragma once

#include <string>
#include <vector>
#include <map>
#include <exception>

namespace lexer_file {

class TrailingIsVariableException: public std::exception {
public:
	TrailingIsVariableException() {}
	virtual ~TrailingIsVariableException() throw() {}

private:
	const char* what() const throw() { return std::string("Trailing is variable").c_str(); }
};

/**
 * @brief A lexer rule made of a regular-expression pattern and an action.
 */
struct Rule {
	/** Optional start conditions attached to the rule (`<STATE1,STATE2>`). */
	std::vector<std::string>	conditions_;
	/** Raw or macro-expanded pattern matched by the lexer. */
	std::string					pattern_;
	/** Optional trailing-context regex (right side of `pattern/trailing`). */
	std::string					trailing_;
	/** User C/C++ action associated with the pattern. */
	std::string					action_;
	/** Fixed trailing length used by codegen for `yyless(yyleng - n)`. */
	int							trailing_length_;
	bool						trailing_is_variable_ = false;
	int							trailing_dfa_id_	= -1;
	/**
	 * @brief True when the rule action is the pipe operator (`|`).
	 *
	 * Pipe rules inherit the action of the next non-pipe rule.
	 */
	bool		is_pipe_;
	bool		is_bol_;
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
	/** Declared start conditions: key=name, value=true for `%x`, false for `%s`. */
	std::map<std::string, bool>							conditions_;	// true if exclusive (%x) false if inclusive (%s)
	bool												array_mode_				= false;
	bool												compression_			= false;
	std::string											rust_user_data_type_	= "()";
};

} // namespace lexer_file