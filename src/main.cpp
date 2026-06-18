#include <exception>
#include <iostream>
#include <string>
#include <fstream>

#include "automata/Pipeline.hpp"
#include "codegen/Codegen.hpp"
#include "codegen/CodegenRust.hpp"
#include "lexer_file/LexParser.hpp"

struct Arguments
{
	std::string	input_file;
	std::string	output_file;
	bool		output_to_file	= true;
	bool		compression		= false;
	bool		backend_rust	= false;
	bool		verbose			= false;
	bool		silent			= true;
	bool		help			= false;
};

static void print_usage(const std::string &prog_name)
{
	std::cerr << "Usage: " << prog_name << " [OPTIONS] <input.l> [output.c]" << std::endl;
	std::cerr << "  -v     : verbose - print statistics" << std::endl;
	std::cerr << "  -n     : disable stats (default)" << std::endl;
	std::cerr << "  -t     : output to stdout instead of file" << std::endl;
	std::cerr << "  -c     : activate compression mode" << std::endl;
	std::cerr << "  --rust : generate output in Rust (lex.yy.rs by default)" << std::endl;
	std::cerr << "           cannot be used with compression mode and cannot output to stdout" << std::endl;
}

static Arguments parse_arguments(int argc, char **argv)
{
	Arguments args;
	bool input_seen = false;

	if (argc < 2) {
		print_usage(argv[0]);
		throw std::invalid_argument("Missing required argument: <input.l>");
	}

	for (int i = 1; i < argc; i++) {
		const std::string arg = argv[i];
		
		if (arg == "--help") {
			args.help = true;
			print_usage(argv[0]);
			return args;
		} else if (arg == "--rust") {
			args.backend_rust = true;
		} else if (!arg.empty() && arg[0] == '-' && arg.size() > 1) {
			for (size_t j = 1; j < arg.size(); ++j) {
				char flag = arg[j];
				if (flag == 'n') {
					continue;
				} else if (flag == 't') {
					args.output_to_file = false;
				} else if (flag == 'c') {
					args.compression = true;
				} else if (flag == 'v') {
					args.verbose = true;
				} else {
					print_usage(argv[0]);
					throw std::invalid_argument(std::string("Unknown flag: -") + flag);
				}
			}
		} else if (!input_seen) {
			args.input_file = arg;
			input_seen = true;
		} else {
			args.output_file = arg;
		}
	}

	if (!input_seen) {
		print_usage(argv[0]);
		throw std::invalid_argument("Missing required argument: <input.l>");
	}

	if ((args.backend_rust && !args.output_to_file) || (args.backend_rust && args.output_file.empty())) {
		args.output_to_file = true;
		args.output_file = "lex.yy.rs";
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

	if (args.help) {
		return 0;
	}

	try {
		lexer_file::LexParser		parser(args.input_file);
		lexer_file::LexFile			lex_file = parser.parse();
		automata::ParsingPipeline	pipeline;
		pipeline.stats().macros_count = static_cast<int>(lex_file.macros_.size());
		pipeline.stats().rules_count = static_cast<int>(lex_file.rules_.size());
		pipeline.stats().start_conditions_count = static_cast<int>(lex_file.conditions_.size());
		pipeline.stats().output_file = args.output_to_file ? args.output_file : "/dev/stdout";
		automata::DFA				dfa = pipeline.execute(lex_file.rules_, lex_file.conditions_);
		std::ofstream				ofs;

		if (args.output_to_file)
			ofs.open(args.output_file.c_str(), std::ios::out | std::ios::trunc);
		else
			ofs.open("/dev/stdout", std::ios::out);

		if (args.compression)
			lex_file.compression_	= true;
		pipeline.stats().compression_enabled = lex_file.compression_;

		if (!ofs.is_open())
			throw std::runtime_error("Failed to open output stream");

		if (args.backend_rust) {
			codegen::CodegenRust	generator;
			generator.generate(dfa, lex_file, ofs, &pipeline.stats());
		} else {
			codegen::Codegen	generator;
			generator.generate(dfa, lex_file, ofs, &pipeline.stats());
		}
		if (pipeline.stats().table_size_raw != 0) {
			pipeline.stats().compression_ratio = static_cast<float>(pipeline.stats().table_size_packed) / static_cast<float>(pipeline.stats().table_size_raw);
		}
		if (args.verbose) {
			pipeline.print_stats();
		}
		return 0;
	} catch (const lexer_file::ParseError& err) {
		std::cerr << err.what() << std::endl;
		return 1;
	} catch (const std::exception& err) {
		std::cerr << "Error: " << err.what() << std::endl;
		return 1;
	}
}
