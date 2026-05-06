#include <exception>
#include <iostream>
#include <string>
#include <fstream>

#include "automata/Pipeline.hpp"
#include "codegen/Codegen.hpp"
#include "lexer_file/LexParser.hpp"

struct Arguments
{
	std::string	input_file;
	std::string	output_file;
	bool		output_to_file	= true;
	bool		compression		= false;
};

static void print_usage(const std::string &prog_name)
{
	std::cerr << "Usage: " << prog_name << " <input.l> [-nt] [output.c]" << std::endl;
	std::cerr << "  -n : (unused)" << std::endl;
	std::cerr << "  -t : output to stdout instead of file" << std::endl;
	std::cerr << "	-c : activate compression mode" << std::endl;
}

static Arguments parse_arguments(int argc, char **argv)
{
	Arguments args;

	if (argc < 2) {
		print_usage(argv[0]);
		throw std::invalid_argument("Missing required argument: <input.l>");
	}

	args.input_file = argv[1];

	for (int i = 2; i < argc; i++) {
		const std::string arg = argv[i];
		
		if (arg == "-n") {
			continue;
		} else if (arg == "-t") {
			args.output_to_file = false;
		} else if (arg == "-c") {
			args.compression	= true;
		} else if (arg[0] == '-') {
			print_usage(argv[0]);
			throw std::invalid_argument("Unknown flag: " + arg);
		} else {
			args.output_file = arg;
		}
	}

	if (args.output_to_file && args.output_file.empty())
		args.output_file = "lex.yy.c";

	return args;
}

int main(int argc, char **argv)
{
	Arguments args;

	try {
		args = parse_arguments(argc, argv);
	} catch (const std::invalid_argument &err) {
		std::cerr << "Error: " << err.what() << std::endl;
		return 1;
	}

	try {
		lexer_file::LexParser		parser(args.input_file);
		lexer_file::LexFile			lex_file = parser.parse();
		automata::ParsingPipeline	pipeline;
		automata::DFA				dfa = pipeline.execute(lex_file.rules_, lex_file.conditions_);
		codegen::Codegen			generator;
		std::ofstream				ofs;

		if (args.output_to_file)
			ofs.open(args.output_file.c_str(), std::ios::out | std::ios::trunc);
		else
			ofs.open("/dev/stdout", std::ios::out);

		if (args.compression)
			lex_file.compression_	= true;

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
