#include <algorithm>
#include <array>
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

[[noreturn]] void print_help(const char* prog_name, int status = EXIT_FAILURE) {
	std::print("Usage: {0} <options>\n"
		"Options:\n"
		"  -i, --input <file>                           Headerless VAG file (Required).\n"
		"  -o, --output <file>                          Output VAG file (Required).\n"
		"  -t, --type <type> <interleave_if_'i'>        Type of \"VAG(?)\", valid values: p (default), 1, 2 or i.\n"
		"  -v, --version <ver>                          Header version (Default: 32 / 0x20).\n"
		"  -sr, --sample_rate <hz>                      Sample rate (Default: 44100).\n"
		"  --channels, -c                               Number of channels for versions: 3, 0x20001 and 0x30000\n"
		"  --name <track_name>                          Track name (Max 16 chars).\n"
		"  --yes, -y                                    Overwrite output file if it exists without prompting.\n"
		"  --no, -n                                     Do not overwrite output file if it exists.\n"
		"  --force, -f                                  Ignores non-critical errors without prompting.\n"
		"  --no-force, -nf                              Do not proceed when non-critical errors are met.\n"
		"  -h, --help                                   Show this help message.\n\n"
		"Note: if you really don't want to be prompted at all you must combine either --yes/-y or --no/-n with either --force/-f or --no-force/-nf\n\n"
		"Note: only VAGp and VAGi with version 32 are supported, support of other formats is experimental.\n\n"
		"e.g:\n    {0} -i in.vab -o out.vag -sr 48000 -t i 0x18000\n"
		"e.g:\n    {0} -i in.vab -o out.vag -sr 48000 -t 2\n"
		"e.g:\n    {0} -i in.vab -o out.vag\n"
		, prog_name);
	std::exit(status);
}

template <typename... T>
[[noreturn]] void print_error(std::format_string<T...> fmt, T&&... args) {
	std::print(std::cerr, fmt, std::forward<T>(args)...);
	std::exit(EXIT_FAILURE);
}

#pragma pack(1)
// 2-byte union
union version_2_and_3_overlap {
	std::array<uint8_t, 2> v2_unknown;
	uint8_t v3_num_channels;
	uint8_t v_other_force_mono;
};
struct vag_header {
	char magic[4] = {'V', 'A', 'G', 'p'}; // 0x00 - 0x03
	uint32_t version = 0x20; //0x04 - 0x07
	uint32_t interleave = 0; // 0x08 - 0x0B Little-endian unlike the reset
	uint32_t channel_size = 0; // 0x0C -0x0F
	uint32_t sample_rate = 44100; // 0x10 - 0x13
	// Version 2, 3, 0x20001 and 0x30000 stuff
	std::array<uint8_t, 2> v2_vol_left = {}; // 0x14 - 0x15
	std::array<uint8_t, 2> v2_vol_right = {}; // 0x16 - 0x17
	std::array<uint8_t, 2> v2_pitch = {}; // 0x18 - 0x19
	std::array<uint8_t, 2> v2_ADSR1 = {}; // 0x1A - 0x1B
	std::array<uint8_t, 2> v2_ADSR2 = {}; // 0x1C - 0x1D
	version_2_and_3_overlap overlap = {}; // 0x1E - 0x1F
	// End version 2, 3, 0x20001 and 0x30000 stuff
	char name[16] = {}; // 0x20 - 0x2F
};
#pragma pack()

int main(int argc, char** argv) {

	enum {
		UNSET,
		YES,
		NO
	};

	const char* input = nullptr;
	const char* output = nullptr;
	int overwrite_flag = UNSET;
	int force_flag = UNSET;
	std::string line_buffer;

	vag_header header;

	{
		int i;
		try {
			for (i = 1; i < argc; ++i) {
				if (*argv[i] != '-') {
					std::print(std::cerr, "Argument \"{}\" was not expected here\n\n", argv[i]);
					print_help(*argv);
				}

				if (std::string_view{argv[i]} == "--yes" || std::string_view{argv[i]} == "-y") {
					overwrite_flag = YES;
					continue;
				}
				if (std::string_view{argv[i]} == "--no" || std::string_view{argv[i]} == "-n") {
					overwrite_flag = NO;
					continue;
				}
				if (std::string_view{argv[i]} == "--force" || std::string_view{argv[i]} == "-f") {
					force_flag = YES;
					continue;
				}
				if (std::string_view{argv[i]} == "--no-force" || std::string_view{argv[i]} == "-nf") {
					force_flag = NO;
					continue;
				}
				if (std::string_view{argv[i]} == "--help" || std::string_view{argv[i]} == "-h") {
					print_help(*argv, EXIT_SUCCESS);
				}

				if (i == argc - 1) {
					print_error("Last option \"{}\" either requires an argument or is invalid.\n", argv[i]);
				}

				if (std::string_view{argv[i]} == "--input" || std::string_view{argv[i]} == "-i") {
					input = argv[++i];
				} else if (std::string_view{argv[i]} == "--output" || std::string_view{argv[i]} == "-o") {
					output = argv[++i];
				} else if (std::string_view{argv[i]} == "--type" || std::string_view{argv[i]} == "-t") {
					if ((*argv[i + 1] == 'p' || *argv[i + 1] == '1' || *argv[i + 1] == '2' || *argv[i + 1] == 'i') && argv[i + 1][1] == 0 ) {
						header.magic[3] = *argv[++i];
						if (header.magic[3] == 'i') {
							if (i == argc - 1) {
								++i;
								throw 0;
							} else {
								header.interleave = std::stoi(argv[++i], nullptr, 0);
							}
						}
					} else {
						print_error("Argument for --type, -t could only be either \"p\", \"1\", \"2\" or \"i\".\n");
					}
				} else if (std::string_view{argv[i]} == "--version" || std::string_view{argv[i]} == "-v") {
					header.version = std::stoi(argv[++i], nullptr, 0);
				} else if (std::string_view{argv[i]} == "--sample_rate" || std::string_view{argv[i]} == "-sr") {
					header.sample_rate = std::stoi(argv[++i], nullptr, 0);
				} else if (std::string_view{argv[i]} == "--channels" || std::string_view{argv[i]} == "-c") {
					header.overlap.v3_num_channels = std::stoi(argv[++i], nullptr, 0);
				} else if (std::string_view{argv[i]} == "--name") {
					if (std::strlen(argv[i + 1]) <= 16) {
						std::strcpy(header.name, argv[++i]);
					} else {
						print_error("Name cannot be longer than 16 characters in ASCII.\n");
					}
				} else {
					print_error("Invalid argument \"{}\".\n\n", argv[i]);
				}
			}
		} catch (...) {
			print_error("A number must come after \"{}\" argument.\n", argv[i - 1]);
		}
	}

	if (header.magic[3] == 'i' && header.interleave == 0) {
		print_error("The value that comes after the \"i\" must be not a zero.");
	}

	if (header.overlap.v3_num_channels != 0 && (header.version != 3 && header.version != 0x20001 && header.version != 0x30000)) {
		print_error("--channels, -c option requires the version to be set to 3, 0x20001 or 0x30000\n"
			"If you want stereo vag you might were supposed to use VAG2 or VAGi types.\n");
	}

	if (!input) {
		print_error("An input file needs to be specified.\n");
	}
	if (!output) {
		print_error("An output file needs to be specified.\n");
	}

	if (header.version == 3) {
		// Initialize with default values according to NoCash / problemkaputt.de
		header.v2_vol_left = {0x4E, 0x82};
		header.v2_vol_right = {0x4E, 0x82};
		header.v2_pitch = {0xA8, 0x88};
		header.v2_ADSR1 = {0x00, 0x00};
		header.v2_ADSR2 = {0x00, 0xE1};
		header.overlap.v2_unknown = {0xA0, 0x23};
	}

	if (!std::filesystem::exists(input)) {
		print_error("Error: input file \"{}\" doesn't exist.\n", input);
	}

	std::ifstream input_handle{input, std::ios::binary};

	auto input_size = std::filesystem::file_size(input);
	if (input_size < 0x10) {
		print_error("Error: size of input file must be at least 16 bytes, which is the size of one psx adpcm chunk.\n");
	}
	if (input_size % 0x10) {
		std::print("Warning: Input file \"{}\" is potentially invalid\n"
			"Size of input file is {} which is not divisble by 16\n"
			"File sizes should be divisible by 16 due to 16 byte alignment of vag format.\n"
			, input, input_size);
		if (force_flag == UNSET) {
			std::print("Do you still want to proceed? [y/N]: ");
			std::getline(std::cin, line_buffer);
			std::ranges::transform(line_buffer, line_buffer.begin(), tolower);
			if (line_buffer != "y") {
				print_error("Aborting.\n");
			}
		} else if (force_flag == NO) {
			print_error("Aborting.\n");
		}
		std::print("Proceeding.\n");
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
		std::print("Input file \"{}\" could be invalid because it doesn't have padding.\n", input);
		if (force_flag == UNSET) {
			std::print("Do you still want to proceed? [y/N]: ", input);
			std::getline(std::cin, line_buffer);
			std::ranges::transform(line_buffer, line_buffer.begin(), tolower);
			if (line_buffer != "y") {
				print_error("Aborting.\n");
			}
		} else if (force_flag == NO) {
			print_error("Aborting.\n");
		}
		std::print("Proceeding.\n");
		padding_size = 0x10;
		input_size += 0x10;
	}
	if (header.magic[3] == 'i') {
		padding_size += 0x800 - 0x30;
	} else if (header.magic[3] == '1' || header.magic[3] == '2' || header.version == 0x02000000 || header.version == 0x40000000) {
		padding_size += 0x10; // At 0x30 offset there is additional 0x10-byte data, can be left blank as padding.
	}
	std::vector<char>padding_buffer(padding_size, 0);


	if (header.magic[3] == 'i' || header.magic[3] == '2') {
		header.channel_size = input_size / 2;
	} else {
		header.channel_size = input_size;
	}

	// Correct endianness
	if constexpr (std::endian::native == std::endian::little) {
		header.version = std::byteswap(header.version);
		header.channel_size = std::byteswap(header.channel_size);
		header.sample_rate = std::byteswap(header.sample_rate);
	} else if constexpr (std::endian::native == std::endian::big) {
		header.interleave = std::byteswap(header.interleave);
	}

	if (std::filesystem::exists(output)) {
		std::print("Output file \"{}\" exists\n", output);
		if (overwrite_flag == UNSET) {
			std::print("Do you want to replace it? [y/N]: ");
			std::getline(std::cin, line_buffer);
			std::ranges::transform(line_buffer, line_buffer.begin(), tolower);
			if (line_buffer != "y") {
				print_error("Aborting.\n");
			}
		} else if (overwrite_flag == NO) {
			print_error("Aborting.\n");
		}
		std::print("Proceeding.\n");
	}

	std::fstream output_handle{output, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc};

	output_handle.write(reinterpret_cast<const char*>(&header), sizeof(header));
	output_handle.write(padding_buffer.data(), padding_buffer.size());
	output_handle << input_handle.rdbuf();
	std::print("File done: \"{}\"\n", output);
}
