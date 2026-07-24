#include <bit>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <string>
#include <vector>

void print_help(const char* prog_name, int status = EXIT_FAILURE) {
	std::print("Usage: {0} <options>\n"
		"Options:\n"
		"  -i, --input <file>                           Headerless VAG file (Required)\n"
		"  -o, --output <file>                          Output VAG file (Required)\n"
		"  -t, --type <type> <interleave_if_'i'>        Type of \"VAG(?)\", valid values: p (default), 1, 2, i\n"
		"  -sr, --sample_rate <hz>                      Sample rate (Default: 44100)\n"
		"  -n, --name <track_name>                      Track name (Max 16 chars)\n"
		"  -v, --version <ver>                          Header version (Default: 32 / 0x20)\n"
		"  --force, -f                                  Overwrite output file if it exists\n"
		"  -h, --help                                   Show this help message\n\n"
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
	bool force_flag = false;

	vag_header header;

	try {
		for (int i =1; i < argc; ++i) {
			if (*argv[i] == '-' && i + 1 == argc || *argv[i] != '-') {
				if (!std::strcmp(argv[i], "--force") || !std::strcmp(argv[i], "-f")) {
					force_flag = true;
					continue;
				}
				print_help(*argv);
			}

			if (!std::strcmp(argv[i], "--help") || !std::strcmp(argv[i], "-h")) {
				print_help(*argv, EXIT_SUCCESS);
				return 0;
			}

			if (!std::strcmp(argv[i], "--type") || !std::strcmp(argv[i], "-t")) {
				if (*argv[i + 1] == 'p' || *argv[i + 1] == '1' || *argv[i + 1] == '2' || *argv[i + 1] == 'i' && argv[i + 1][1] == 0 ) {
					header.magic[3] = *argv[++i];
					if (header.magic[3] == 'i') {
						header.interleave = std::stoi(argv[++i], nullptr, 0);
					}
				} else {
					print_help(*argv);
				}
			} else if (!std::strcmp(argv[i], "--version") || !std::strcmp(argv[i], "-v")) {
				header.version = std::stoi(argv[++i], nullptr, 0);
			} else if (!std::strcmp(argv[i], "--sample_rate") || !std::strcmp(argv[i], "-sr")) {
				header.sample_rate = std::stoi(argv[++i], nullptr, 0);
			} else if (!std::strcmp(argv[i], "--name") || !std::strcmp(argv[i], "-n")) {
				if (std::strlen(argv[i + 1]) <= 16) {
					std::strcpy(header.name, argv[++i]);
				} else {
					print_help(*argv);
				}
			} else if (!std::strcmp(argv[i], "--input") || !std::strcmp(argv[i], "-i")) {
				input = argv[++i];
			} else if (!std::strcmp(argv[i], "--output") || !std::strcmp(argv[i], "-o")) {
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

	if (std::filesystem::exists(output) && !force_flag) {
		printf("Output file: \"%s\" exists, do you want to replace it? [y/N]:", output);
		if (std::tolower(std::getchar()) == 'y') {
			force_flag = true;
		}
		std::cout << std::endl;
	}

	std::ifstream input_handle{input, std::ios::binary};
	std::fstream output_handle{output, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc};

	auto input_size = std::filesystem::file_size(input);
	if (input_size % 0x10) {
		std::print("Input file \"{}\" is potentially invalid\n"
			"Size of input file is: {} which is not divisble by 16\n"
			"File sizes should be divisible by 16 due to 16 byte alignment of vag format.\n"
			"proceeding anyway...\n"
			, input, input_size);
	}
	if (input_size < 0x10) {
		std::print("Error: size of input file must be at least 16 bytes, which is the size of one psx adpcm chunk.\n");
		return EXIT_FAILURE;
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

	output_handle.write(reinterpret_cast<const char*>(&header), sizeof(header));
	output_handle.write(padding_buffer.data(), padding_buffer.size());
	output_handle << input_handle.rdbuf();
}
