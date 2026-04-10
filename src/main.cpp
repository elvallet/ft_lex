#include <exception>
#include <iostream>
#include <string>

#include "automata/Pipeline.hpp"
#include "codegen/Codegen.hpp"
#include "lexer_file/LexParser.hpp"

int main(int argc, char **argv)
{
	if (argc < 2 || argc > 3) {
		std::cerr << "Usage: " << argv[0] << " <input.l> [output.c]" << std::endl;
		return 1;
	}

	const std::string	input_file = argv[1];
	const std::string	output_file = (argc == 3) ? argv[2] : "lex.yy.c";

	try {
		lexer_file::LexParser		parser(input_file);
		lexer_file::LexFile			lex_file = parser.parse();
		automata::ParsingPipeline	pipeline;
		automata::DFA				dfa = pipeline.execute(lex_file.rules_, lex_file.conditions_);
		codegen::Codegen			generator;

		generator.generate(dfa, lex_file, output_file);
		return 0;
	} catch (const lexer_file::ParseError& err) {
		std::cerr << err.what() << std::endl;
		return 1;
	} catch (const std::exception& err) {
		std::cerr << "Error: " << err.what() << std::endl;
		return 1;
	}
}
