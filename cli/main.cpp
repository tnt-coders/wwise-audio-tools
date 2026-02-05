/**
 * @file main.cpp
 * @brief CLI interface for Wwise audio tools
 * @note Modernized to C++23
 */

#include <algorithm>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <span>
#include <sstream>
#include <string>
#include <string_view>

#include <rang.hpp>
#include "ww2ogg/ww2ogg.h"
#include "wwtools/bnk.h"
#include "wwtools/wwtools.h"

namespace fs = std::filesystem;

// Modern error type for conversion results
enum class ConvertError {
  ConversionFailed,
  FileWriteFailed
};

/**
 * @brief Convert WEM data to OGG and write to file
 * @param indata Input WEM data
 * @param outpath Output file path
 * @return std::expected with void on success or ConvertError on failure
 */
[[nodiscard]] auto convert(std::string_view indata, const fs::path& outpath)
    -> std::expected<void, ConvertError> {
  auto outdata = wwtools::wem_to_ogg(std::string{indata});
  if (outdata.empty()) {
    return std::unexpected(ConvertError::ConversionFailed);
  }

  std::ofstream fout(outpath, std::ios::binary);
  if (!fout) {
    return std::unexpected(ConvertError::FileWriteFailed);
  }
  fout << outdata;
  return {};
}

/**
 * @brief Print help message with optional error
 * @param extra_message Error message to display (optional)
 * @param filename Program name for usage display
 */
void print_help(std::string_view extra_message = {},
                std::string_view filename = "wwtools") {
  if (!extra_message.empty()) {
    std::cout << rang::fg::red << extra_message << rang::fg::reset << "\n\n";
  }
  std::println("Please use the command in one of the following ways:");
  std::println("  {} wem [input.wem] (--info)", filename);
  std::println("  {} bnk [event|extract] (input.bnk) (event ID) (--info) (--no-convert)", filename);
  std::println("Or run it without arguments to find and convert all WEMs in the current directory.");
}

/**
 * @brief Result of parsing command-line flags
 */
struct ParsedFlags {
  std::vector<std::string> flags;
  bool has_error = false;
};

/**
 * @brief Parse command-line flags from arguments
 * @param args Span of command-line arguments
 * @return ParsedFlags structure with flags and error status
 */
[[nodiscard]] auto get_flags(std::span<char*> args) -> ParsedFlags {
  ParsedFlags result;
  result.flags.reserve(args.size());
  bool flag_found = false;

  for (std::string_view arg : args) {
    // C++23: Use starts_with
    if (arg.starts_with("--") && arg.length() > 2) {
      flag_found = true;
      result.flags.emplace_back(arg.substr(2));
    } else if (flag_found) {
      // If current arg is not a flag but comes after a flag...
      print_help("Please place all flags after other args!", args[0]);
      result.has_error = true;
      return result;
    }
  }
  return result;
}

/**
 * @brief Check if a flag exists in the flags list
 * @param flags List of parsed flags
 * @param wanted_flag Flag to search for
 * @return true if flag exists
 */
[[nodiscard]] auto has_flag(const std::vector<std::string>& flags,
                            std::string_view wanted_flag) -> bool {
  return std::ranges::contains(flags, wanted_flag);
}

/**
 * @brief Read file contents into a string
 * @param path File path to read
 * @return File contents as string, or empty on failure
 */
[[nodiscard]] auto read_file(const fs::path& path) -> std::string {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return {};
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

/**
 * @brief Replace file extension with a new one
 * @param path Original path
 * @param new_ext New extension (including dot)
 * @return Path with new extension
 */
[[nodiscard]] auto replace_extension(const fs::path& path, std::string_view new_ext) -> fs::path {
  auto result = path;
  result.replace_extension(new_ext);
  return result;
}

auto main(int argc, char* argv[]) -> int {
  // Create span for modern iteration
  std::span args(argv, static_cast<std::size_t>(argc));

  auto [flags, has_error] = get_flags(args);
  if (has_error) {
    return EXIT_FAILURE;
  }

  if (has_flag(flags, "help")) {
    print_help();
    return EXIT_SUCCESS;
  }

  if (argc < 2) {
    // If there is no input file, convert all WEM files in the current directory
    bool wem_exists = false;

    for (const auto& entry : fs::directory_iterator(fs::current_path())) {
      if (entry.path().extension() != ".wem") {
        continue;
      }

      wem_exists = true;
      std::println("Converting {}...", entry.path().string());

      auto indata = read_file(entry.path());
      if (indata.empty()) {
        std::println(stderr, "Failed to read {}", entry.path().string());
        return EXIT_FAILURE;
      }

      auto outpath = replace_extension(entry.path(), ".ogg");

      if (auto result = convert(indata, outpath); !result) {
        std::println(stderr, "Failed to convert {}", entry.path().string());
        return EXIT_FAILURE;
      }
    }

    if (!wem_exists) {
      print_help("No WEM files found in the current directory!", args[0]);
    }
    return EXIT_SUCCESS;
  }

  if (argc < 3) {
    print_help("Missing arguments!", args[0]);
    return EXIT_FAILURE;
  }

  std::string_view command = args[1];

  // WEM command handling
  if (command == "wem") {
    fs::path path = args[2];
    auto indata = read_file(path);

    if (indata.empty()) {
      std::println(stderr, "Failed to read {}", path.string());
      return EXIT_FAILURE;
    }

    if (has_flag(flags, "info")) {
      std::print("{}", ww2ogg::wem_info(indata));
      return EXIT_SUCCESS;
    }

    auto outpath = replace_extension(path, ".ogg");
    std::println("Extracting {}...", outpath.string());

    if (auto result = convert(indata, outpath); !result) {
      std::println(stderr, "Failed to convert {}", path.string());
      return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
  }

  // BNK command handling
  if (command == "bnk") {
    if (argc < 4) {
      print_help("You must specify whether to extract or find an event as well as the input!", args[0]);
      return EXIT_FAILURE;
    }

    std::string_view subcommand = args[2];
    fs::path bnk_path = args[3];

    auto indata = read_file(bnk_path);
    if (indata.empty()) {
      std::println(stderr, "Failed to read {}", bnk_path.string());
      return EXIT_FAILURE;
    }

    if (has_flag(flags, "info")) {
      std::print("{}", wwtools::bnk::get_info(indata));
      return EXIT_SUCCESS;
    }

    if (subcommand != "event" && subcommand != "extract") {
      print_help("Incorrect value for read or write!", args[0]);
      return EXIT_FAILURE;
    }

    if (subcommand == "event") {
      std::string in_event_id;
      if (argc >= 5) {
        in_event_id = args[4];
      }

      std::print("{}", wwtools::bnk::get_event_id_info(indata, in_event_id));
      return EXIT_SUCCESS;
    }

    // Extract subcommand
    std::vector<std::string> wems;
    wwtools::bnk::extract(indata, wems);

    auto outdir = replace_extension(bnk_path, "");
    fs::create_directory(outdir);

    const bool noconvert = has_flag(flags, "no-convert");
    const std::string_view file_extension = noconvert ? ".wem" : ".ogg";

    for (std::size_t idx = 0; idx < wems.size(); ++idx) {
      const auto& wem = wems[idx];

      // Re-read file to get WEM ID (consider optimizing this in the future)
      auto id_data = read_file(bnk_path);
      auto wem_id = wwtools::bnk::get_wem_id_at_index(id_data, static_cast<int>(idx));

      fs::path outpath = outdir / wem_id;
      auto full_outpath = outpath.string() + std::string{file_extension};

      std::cout << rang::fg::cyan << "[" << (idx + 1) << "/" << wems.size() << "] "
                << rang::fg::reset << "Extracting " << full_outpath << "...\n";

      if (noconvert) {
        std::ofstream of(full_outpath, std::ios::binary);
        of << wem;
      } else if (auto result = convert(wem, full_outpath); !result) {
        std::println("Failed to convert {}", full_outpath);
        // Don't return error because others may succeed
      }
    }
    return EXIT_SUCCESS;
  }

  // Unknown command
  print_help("Unknown command!", args[0]);
  return EXIT_FAILURE;
}
