#include <exception>
#include <iostream>
#include <string>
#include <fstream>

#include "automata/Pipeline.hpp"
#include "codegen/Codegen.hpp"
#include "lexer_file/LexParser.hpp"

int main(int argc, char **argv)
{
	if (argc < 2) {
		std::cerr << "Usage: " << argv[0] << " <input.l> -[tn] [output.c]" << std::endl;
		return 1;
	}

	const std::string	input_file	= argv[1];
	std::string			output_file;
	bool				outfile		= true;

	for (int i = 2; i < argc; i++) {
		if (argv[i] && argv[i] == "-n")
			continue;
		else if (argv[i] && argv[i] == "-t")
			outfile = false;
		else if (i == argc - 2 && argv[i])
			output_file = argv[i];
		else {
			std::cerr << "Usage: " << argv[0] << " <input.l> -[tn] [output.c]" << std::endl;
			return 1;
		}
	}

	if (outfile && output_file.empty())
		output_file = "lex.yy.c";

	try {
		lexer_file::LexParser		parser(input_file);
		lexer_file::LexFile			lex_file = parser.parse();
		automata::ParsingPipeline	pipeline;
		automata::DFA				dfa = pipeline.execute(lex_file.rules_, lex_file.conditions_);
		codegen::Codegen			generator;
		std::ofstream				ofs;

		if (outfile)
			ofs.open(output_file.c_str(), std::ios::out | std::ios::trunc);
		else
			ofs.open("/dev/stdout", std::ios::out);

		if (!ofs.is_open())
			throw std::runtime_error("Failed to open output stream");

		generator.generate(dfa, lex_file, ofs);
		return 0;
	} catch (const lexer_file::ParseError& err) {
		std::cerr << err.what() << std::endl;
		return 1;
	} catch (const std::exception& err) {
		std::cerr << "Error: " << err.what() << std::endl;
		return 1;
	}
}
