#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <string>
#include <string_view>
#include <vector>

void print_help(const char* prog_name, int status = EXIT_FAILURE) {
	std::print("Usage: {0} <options>\n"
		"Options:\n"
		"  -i, --input <file>                           Headerless VAG file (Required).\n"
		"  -o, --output <file>                          Output VAG file (Required).\n"
		"  -t, --type <type> <interleave_if_'i'>        Type of \"VAG(?)\", valid values: p (default), 1, 2 or i.\n"
		"  -sr, --sample_rate <hz>                      Sample rate (Default: 44100).\n"
		"  -n, --name <track_name>                      Track name (Max 16 chars).\n"
		"  -v, --version <ver>                          Header version (Default: 32 / 0x20).\n"
		"  --yes, -y                                    Overwrite output file if it exists without prompting.\n"
		"  --force, -f                                  Ignores non-critical errors without prompting.\n"
		"  -h, --help                                   Show this help message\n\n"
		"Note: if you really don't want to prompted at all you must combine --force/-f with --yes/-y\n\n"
		"e.g:\n    {0} -i in.vab -o out.vag -sr 48000 -t i 0x18000\n"
		"e.g:\n    {0} -i in.vab -o out.vag -sr 48000 -t 2\n"
		"e.g:\n    {0} -i in.vab -o out.vag\n"
		, prog_name);
	std::exit(status);
}

#pragma pack(1)
struct vag_header {
	char magic[4] = {'V', 'A', 'G', 'p'};
	uint32_t version = 0x20;
	uint32_t interleave = 0; // Little-endian unlike the reset
	uint32_t channel_size = 0;
	uint32_t sample_rate = 44100;
	char reserved[12] = {};
	char name[16] = {};
};
#pragma pack()

int main(int argc, char** argv) {

	const char* input = nullptr;
	const char* output = nullptr;
	bool overwrite_flag = false;
	bool force_flag = false;
	std::string line_buffer;

	vag_header header;

	try {
		for (int i =1; i < argc; ++i) {
			if (*argv[i] == '-' && i + 1 == argc || *argv[i] != '-') {
				if (std::string_view{argv[i]} == "--yes" || std::string_view{argv[i]} == "-y") {
					overwrite_flag = true;
					continue;
				}
				if (std::string_view{argv[i]} == "--force" || std::string_view{argv[i]} == "-f") {
					force_flag = true;
					continue;
				}
				print_help(*argv);
			}

			if (std::string_view{argv[i]} == "--help" || std::string_view{argv[i]} == "-h") {
				print_help(*argv, EXIT_SUCCESS);
				return 0;
			}

			if (std::string_view{argv[i]} == "--type" || std::string_view{argv[i]} == "-t") {
				if ((*argv[i + 1] == 'p' || *argv[i + 1] == '1' || *argv[i + 1] == '2' || *argv[i + 1] == 'i') && argv[i + 1][1] == 0 ) {
					header.magic[3] = *argv[++i];
					if (header.magic[3] == 'i') {
						header.interleave = std::stoi(argv[++i], nullptr, 0);
					}
				} else {
					print_help(*argv);
				}
			} else if (std::string_view{argv[i]} == "--version" || std::string_view{argv[i]} == "-v") {
				header.version = std::stoi(argv[++i], nullptr, 0);
			} else if (std::string_view{argv[i]} == "--sample_rate" || std::string_view{argv[i]} == "-sr") {
				header.sample_rate = std::stoi(argv[++i], nullptr, 0);
			} else if (std::string_view{argv[i]} == "--name" || std::string_view{argv[i]} == "-n") {
				if (std::strlen(argv[i + 1]) <= 16) {
					std::strcpy(header.name, argv[++i]);
				} else {
					print_help(*argv);
				}
			} else if (std::string_view{argv[i]} == "--input" || std::string_view{argv[i]} == "-i") {
				input = argv[++i];
			} else if (std::string_view{argv[i]} == "--output" || std::string_view{argv[i]} == "-o") {
				output = argv[++i];
			} else {
				print_help(*argv);
			}
		}
	}
	catch (...) {
		print_help(*argv);
	}
	if (header.magic[3] == 'i' && header.interleave == 0) {
		print_help(*argv);
	}

	if (!input || !output) {
		print_help(*argv);
	}

	std::ifstream input_handle{input, std::ios::binary};

	auto input_size = std::filesystem::file_size(input);
	if (input_size < 0x10) {
		std::print("Error: size of input file must be at least 16 bytes, which is the size of one psx adpcm chunk.\n");
		return EXIT_FAILURE;
	}
	if (input_size % 0x10) {
		std::print("Warning: Input file \"{}\" is potentially invalid\n"
			"Size of input file is: {} which is not divisble by 16\n"
			"File sizes should be divisible by 16 due to 16 byte alignment of vag format.\n"
			, input, input_size);
		if (!force_flag) {
			std::print("Do you still want to proceed? [y/N]: ");
			std::getline(std::cin, line_buffer);
			std::ranges::transform(line_buffer, line_buffer.begin(), tolower);
			if (line_buffer != "y") {
				return EXIT_FAILURE;
			}
		}
	}

	// Determine padding
	size_t padding_size;
	uint64_t buff1;
	uint64_t buff2;
	input_handle.read(reinterpret_cast<char*>(&buff1), 8);
	input_handle.read(reinterpret_cast<char*>(&buff2), 8);
	input_handle.seekg(0, std::ios::beg);
	if (buff1 == 0 && buff2 == 0) {
		padding_size = 0;
	} else {
		std::print("Input file: \"{}\" could be invalid because it doesn't have padding.\n", input);
		if (!force_flag) {
			std::print("Do you still want to proceed? [y/N]: ", input);
			std::getline(std::cin, line_buffer);
			std::ranges::transform(line_buffer, line_buffer.begin(), tolower);
			if (line_buffer != "y") {
				return EXIT_FAILURE;
			}
		}
		padding_size = 0x10;
		input_size += 0x10;
	}
	if (header.magic[3] == 'i') {
		padding_size += 0x800 - 0x30;
	}
	std::vector<char>padding_buffer(padding_size, 0);


	if (header.magic[3] == 'i') {
		header.channel_size = input_size / 2;
	} else {
		header.channel_size = input_size;
	}

	// Correct endianness
	header.version = std::byteswap(header.version);
	header.channel_size = std::byteswap(header.channel_size);
	header.sample_rate = std::byteswap(header.sample_rate);

	if (std::filesystem::exists(output) && !overwrite_flag) {
		std::print("Output file: \"{}\" exists, do you want to replace it? [y/N]: ", output);
		std::getline(std::cin, line_buffer);
		std::ranges::transform(line_buffer, line_buffer.begin(), tolower);
		if (line_buffer != "y") {
			return EXIT_FAILURE;
		}
	}

	std::fstream output_handle{output, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc};

	output_handle.write(reinterpret_cast<const char*>(&header), sizeof(header));
	output_handle.write(padding_buffer.data(), padding_buffer.size());
	output_handle << input_handle.rdbuf();
}
