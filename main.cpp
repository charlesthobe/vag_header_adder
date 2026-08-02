#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <vector>

[[noreturn]] void print_help(const char* prog_name, int status = EXIT_FAILURE) {
	std::print("Usage: {0} [probe | reverse] -i <file> -o <file> [<options>...]\n\n"
		"Modes:\n"
		"  probe <file>                                 Tries to get information about a provided vag file.\n"
		"  reverse                                      Experimental: get headerless body (VB) out of vag, only accepts --input/-i or --output/-o\n\n"
		"Options:\n"
		"  --input, -i <file>                           Headerless VAG file (Required).\n"
		"  --output, -o <file>                          Output VAG file (Required).\n"
		"  --type, -t <type> [<interleave>]             Type of \"VAG(?)\", valid values: \"p\" (default), \"1\", \"2\" or \"i\" <interleave>.\n"
		"  --version, -v <ver>                          Header version (Default: 32 / 0x20).\n"
		"  --sample_rate, -sr <hz>                      Sample rate (Default: 44100).\n"
		"  --channels, -c                               Number of channels for versions: 3, 0x20001 and 0x30000\n"
		"  --name <track_name>                          Track name (Max 16 chars).\n"
		"  --yes, -y                                    Overwrite output file if it exists without prompting.\n"
		"  --no, -n                                     Do not overwrite output file if it exists.\n"
		"  --force, -f                                  Ignores non-critical errors without prompting.\n"
		"  --no-force, -nf                              Do not proceed when non-critical errors are met.\n"
		"  -h, --help                                   Show this help message.\n\n"
		"Note: if you really don't want to be prompted at all you must combine either --yes/-y or --no/-n with either --force/-f or --no-force/-nf\n\n"
		"Note: only VAGp and VAGi with version 32 are supported, support of other formats is experimental.\n\n"
		"e.g:\n    {0} -i in.VB -o out.vag -sr 48000 -t i 0x18000\n"
		"e.g:\n    {0} -i in.VB -o out.vag -sr 48000 -t 2\n"
		"e.g:\n    {0} -i in.VB -o out.vag\n"
		, prog_name);
	std::exit(status);
}

std::optional<std::string_view> get_arg(int argc, char** argv, int pos) {
	if (pos < argc) {
		return std::string_view{argv[pos]};
	}
	return std::nullopt;
}

template <typename... T>
[[noreturn]] void print_error(std::format_string<T...> fmt, T&&... args) {
	std::print(std::cerr, fmt, std::forward<T>(args)...);
	std::exit(EXIT_FAILURE);
}

template <typename T>
concept uint_or_less_type = std::unsigned_integral<T> && sizeof(T) <= sizeof(int32_t);

template<uint_or_less_type T>
bool stoi_wrapper (const char* cstring, T* output, std::function<void()> error_invalid, std::function<void()> error_range) {
	uint32_t value;
	try {
		*output = std::bit_cast<unsigned uint32_t>(std::stoi(cstring, nullptr, 0));
	} catch (std::invalid_argument) {
		error_invalid();
		return false;
	} catch (std::out_of_range) {
		error_range();
		return false;
	}
	if (value > std::numeric_limits<T>::max()) {
		error_range();
		return false;
	}
	return true;
}

void proceed_question(const char* message) {
	std::print("{}", message);
	std::string answer;
	std::getline(std::cin, answer);
	std::ranges::transform(answer, answer.begin(), [](char c) { return static_cast<char>(tolower(c)); });
	if (answer != "y") {
		print_error("Aborting.\n");
	}
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

void reverse_header_endianness(vag_header* header) {
	if constexpr (std::endian::native == std::endian::little) {
		header->version = std::byteswap(header->version);
		header->channel_size = std::byteswap(header->channel_size);
		header->sample_rate = std::byteswap(header->sample_rate);
	} else if constexpr (std::endian::native == std::endian::big) {
		header->interleave = std::byteswap(header->interleave);
	}
}

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
	bool probe_mode = false;
	bool reverse_mode = false;

	vag_header header;

	{
		int i = 1;
		if (argc > 1) {
			if (std::string_view{argv[1]} == "probe") {
				probe_mode = true;
				if (argc != 3) {
					print_error("Usage of probe mode is: {} probe <file>\n", argv[0]);
				}
				input = argv[i + 1];
				i = argc;
			} else if (std::string_view{argv[1]} == "reverse") {
				reverse_mode = true;
				i = 2;
			}
		} else {
			print_help(argv[0]);
		}

		auto missing_arg = [&]() {
			print_error("Last option \"{}\" requires an argument.\n", argv[i - 1]);
		};
		auto error_invalid = [&]() {
			print_error("A number must come after \"{}\" argument.\n", argv[i - 1]);
		};
		auto error_range = [&]() {
			print_error("Provided number \"{}\" for argument \"{}\" is too big.\n", argv[i], argv[i - 1]);
		};

		for (; i < argc; ++i) {
			std::string_view arg{argv[i]};

			if (arg[0] != '-') {
				std::print(std::cerr, "Argument \"{}\" was not expected here\n\n", arg);
				print_help(argv[0]);
			}

			if (arg == "--yes" || arg == "-y") {
				overwrite_flag = YES;
				continue;
			}
			if (arg == "--no" || arg == "-n") {
				overwrite_flag = NO;
				continue;
			}
			if (arg == "--force" || arg == "-f") {
				force_flag = YES;
				continue;
			}
			if (arg == "--no-force" || arg == "-nf") {
				force_flag = NO;
				continue;
			}
			if (arg == "--help" || arg == "-h") {
				print_help(argv[0], EXIT_SUCCESS);
			}

			// Multi arg section
			auto arg_next = get_arg(argc, argv, ++i);
			if (arg == "--input" || arg == "-i") {
				if (!arg_next) {
					missing_arg();
				}
				input = (*arg_next).data();
			} else if (arg == "--output" || arg == "-o") {
				if (!arg_next) {
					missing_arg();
				}
				output = (*arg_next).data();
			} else if (reverse_mode) {
					print_error("\"reverse\" mode doesn't accept this parameter: \"{}\"\n", arg);
			} else if (arg == "--type" || arg == "-t") {
				if (!arg_next) {
					missing_arg();
				}
				if (*arg_next == "p" || *arg_next == "1" || *arg_next == "2" || *arg_next == "i") {
					header.magic[3] = (*arg_next)[0];
				} else {
					print_error("Argument for --type, -t could only be either \"p\", \"1\", \"2\" or \"i\".\n");
				}
				if (header.magic[3] == 'i') {
					arg_next = get_arg(argc, argv, ++i);
					if (!arg_next) {
						missing_arg();
					}
					stoi_wrapper((*arg_next).data(), &header.interleave, error_invalid, error_range);
				}
			} else if (arg == "--version" || arg == "-v") {
				if (!arg_next) {
					missing_arg();
				}
				stoi_wrapper((*arg_next).data(), &header.version, error_invalid, error_range);
			} else if (arg == "--sample_rate" || arg == "-sr") {
				if (!arg_next) {
					missing_arg();
				}
				stoi_wrapper((*arg_next).data(), &header.sample_rate, error_invalid, error_range);
			} else if (arg == "--channels" || arg == "-c") {
				if (!arg_next) {
					missing_arg();
				}
				stoi_wrapper((*arg_next).data(), &header.overlap.v3_num_channels, error_invalid, error_range);
			} else if (arg == "--name") {
				if (!arg_next) {
					missing_arg();
				}
				if (std::strlen((*arg_next).data()) <= 16) {
					std::strcpy(header.name, (*arg_next).data());
				} else {
					print_error("Name cannot be longer than 16 ASCII characters.\n");
				}
			} else {
				print_error("Invalid argument \"{}\".\n", arg);
			}
		}
	}

	if (header.magic[3] == 'i' && header.interleave == 0) {
		print_error("The value that comes after the \"i\" must not be a zero.");
	}

	if (header.overlap.v3_num_channels != 0 && (header.version != 3 && header.version != 0x20001 && header.version != 0x30000)) {
		print_error("--channels, -c option requires the version to be set to 3, 0x20001 or 0x30000\n"
			"If you want stereo vag you might were supposed to use VAG2 or VAGi types.\n");
	}

	if (!input) {
		print_error("An input file needs to be specified.\n");
	}
	if (!output && !probe_mode) {
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
	try {
		input_handle.exceptions(std::ios::badbit | std::ios::failbit);
	} catch (...) {
		print_error("Couldn't open input file \"{}\"\n", input);
	}

	auto input_size = std::filesystem::file_size(input);
	if (input_size < 0x10) {
		print_error("Error: size of input file must be at least 16 bytes, which is the size of one psx adpcm chunk.\n");
	}

	if (probe_mode || reverse_mode) {
		if (input_size < sizeof(header)) {
			print_error("Input file is smaller than a VAG header!\n");
		}
		input_handle.read(reinterpret_cast<char*>(&header), sizeof(header));
		input_handle.seekg(0, std::ios::beg);
		// To native endianness
		reverse_header_endianness(&header);
		std::string_view header_magic{header.magic, 4};
		if ((header_magic != "VAGp" && header_magic != "VAG1" && header_magic != "VAG2" && header_magic != "VAGi") ||
			(header.version != 0 && header.version != 2 && header.version != 3 && header.version != 32 && header.version != 0x20001 && header.version != 0x30000)) {
				print_error("VAG file not supported or not a VAG file\n");
			}
	}

	if (input_size % 0x10) {
		std::print("Warning: Input file \"{}\" is potentially invalid\n"
			"Size of input file is {} which is not divisble by 16\n"
			"File sizes should be divisible by 16 due to 16 byte alignment of vag format.\n"
			, input, input_size);
		if (force_flag == UNSET) {
			proceed_question("Do you still want to proceed? [y/N]: ");
		} else if (force_flag == NO) {
			print_error("Aborting.\n");
		}
		std::print("Proceeding.\n");
	}

	// Determine padding
	size_t padding_size;
	uint64_t first_chunk[2];
	input_handle.read(reinterpret_cast<char*>(first_chunk), 0x10);
	input_handle.seekg(0, std::ios::beg);
	if (first_chunk[0] == 0 && first_chunk[1] == 0 || probe_mode || reverse_mode) {
		padding_size = 0;
	} else {
		std::print("Input file \"{}\" could be invalid because it doesn't have padding.\n", input);
		if (force_flag == UNSET) {
			proceed_question("Do you still want to proceed? [y/N]: ");
		} else if (force_flag == NO) {
			print_error("Aborting.\n");
		}
		std::print("Proceeding.\n");
		padding_size = 0x10;
		input_size += 0x10;
	}
	if (header.magic[3] == 'i') {
		padding_size += 0x800 - sizeof(header);
	} else if (header.magic[3] == '1' || header.magic[3] == '2' || header.version == 0x02000000 || header.version == 0x40000000) {
		padding_size += 0x10; // At 0x30 offset there is additional 0x10-byte data, can be left blank as padding.
	}
	std::vector<char> padding_buffer;
	if (!probe_mode && !reverse_mode) {
		padding_buffer.resize(padding_size, 0);
	}

	if (!probe_mode && !reverse_mode) {
		if (header.magic[3] == 'i' || header.magic[3] == '2') {
			header.channel_size = input_size / 2;
		} else if (header.version == 3 || header.version == 0x20001 || header.version == 0x30000) {
			header.channel_size = input_size / header.overlap.v3_num_channels;
		} else {
			header.channel_size = input_size;
		}
	}

	if (probe_mode) {
		int num_channels = 1;
		if (header.magic[3] == 'i' || header.magic[3] == '2') {
			num_channels = 2;
		} else if (header.version == 3 || header.version == 0x20001 || header.version == 0x30000) {
			num_channels = header.overlap.v3_num_channels;
		}
		std::print("Probing {}...\n", input);
		std::print("File type: {}\n"
			"Version: {:#X}\n"
			"Sample rate: {}\n"
			"Number of channels: {}\n"
			"Channel size: {}\n"
			"Data size: {}\n"
			"Data start offset: {:#X}\n"
			"Track name: {}\n"
			, std::string_view{header.magic, 4}
			, header.version
			, header.sample_rate
			, num_channels
			, header.channel_size
			, header.channel_size * num_channels // Data size
			, sizeof(header) + padding_size // Data start offset
			, header.name
			);
		return EXIT_SUCCESS;
	}

	if (std::filesystem::exists(output)) {
		std::print("Output file \"{}\" exists\n", output);
		if (overwrite_flag == UNSET) {
			proceed_question("Do you want to replace it? [y/N]: ");
		} else if (overwrite_flag == NO) {
			print_error("Aborting.\n");
		}
		std::print("Proceeding.\n");
	}

	std::fstream output_handle{output, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc};
	try {
		output_handle.exceptions(std::ios::badbit | std::ios::failbit);
	} catch (...) {
		print_error("Couldn't open/create output file \"{}\"\n", output);
	}

	if (reverse_mode) {
		std::print("Warning: extracting VB from VAG is strictly experimental, resulting files can be broken!\n");
		input_handle.seekg(sizeof(header) + padding_size, std::ios::beg);
	} else {
		// Correct endianness
		reverse_header_endianness(&header);
		// Write out header
		output_handle.write(reinterpret_cast<const char*>(&header), sizeof(header));
		// Write out padding
		output_handle.write(padding_buffer.data(), padding_buffer.size());
	}
	// Write out body
	output_handle << input_handle.rdbuf();
	std::print("File done: \"{}\"\n", output);
}
