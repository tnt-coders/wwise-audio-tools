/**
 * @file main.cpp
 * @brief CLI interface for Wwise audio tools
 * @note Modernized to C++23
 */

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "bnk.h"
#include "ww2ogg/ww2ogg.h"
#include "wwtools/wwtools.h"
#include <rang.hpp>

namespace fs = std::filesystem;

/**
 * @brief Convert WEM data to OGG and write to file
 * @param indata Input WEM data
 * @param outpath Output file path
 * @throws std::exception on conversion or file write failure
 */
void Convert(const std::string_view indata, const fs::path& outpath)
{
    const auto outdata = wwtools::wem_to_ogg(std::string{indata});

    std::ofstream fout(outpath, std::ios::binary);
    if (!fout)
    {
        throw std::runtime_error("failed to open output file");
    }
    fout << outdata;
}

/**
 * @brief Print help message with optional error
 * @param extra_message Error message to display (optional)
 * @param filename Program name for usage display
 */
void PrintHelp(const std::string_view extra_message = {},
               const std::string_view filename = "wwtools")
{
    if (!extra_message.empty())
    {
        std::cout << rang::fg::red << extra_message << rang::fg::reset << "\n\n";
    }
    std::println("Please use the command in one of the following ways:");
    std::println("  {} wem [input.wem] (--info)", filename);
    std::println("  {} bnk [event|extract] (input.bnk) (event ID) (--info) (--no-convert)",
                 filename);
    std::println(
        "Or run it without arguments to find and convert all WEMs in the current directory.");
}

/**
 * @brief Result of parsing command-line flags
 */
struct ParsedFlags
{
    std::vector<std::string> m_flags;
    bool m_has_error = false;
};

/**
 * @brief Parse command-line flags from arguments
 * @param args Span of command-line arguments
 * @return ParsedFlags structure with flags and error status
 */
[[nodiscard]] ParsedFlags GetFlags(const std::span<char*> args)
{
    ParsedFlags result;
    result.m_flags.reserve(args.size());
    bool flag_found = false;

    for (std::string_view arg : args)
    {
        // C++23: Use starts_with
        if (arg.starts_with("--") && arg.length() > 2)
        {
            flag_found = true;
            result.m_flags.emplace_back(arg.substr(2));
        }
        else if (flag_found)
        {
            // If current arg is not a flag but comes after a flag...
            PrintHelp("Please place all flags after other args!", args[0]);
            result.m_has_error = true;
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
[[nodiscard]] bool HasFlag(const std::vector<std::string>& flags,
                           const std::string_view wanted_flag)
{
    return std::ranges::contains(flags, wanted_flag);
}

/**
 * @brief Read file contents into a string
 * @param path File path to read
 * @return File contents as string, or empty on failure
 */
[[nodiscard]] std::string ReadFile(const fs::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
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
[[nodiscard]] fs::path ReplaceExtension(const fs::path& path, const std::string_view new_ext)
{
    auto result = path;
    result.replace_extension(new_ext);
    return result;
}

int main(const int argc, char* argv[])
{
    // Create span for modern iteration
    std::span args(argv, static_cast<std::size_t>(argc));

    const auto [flags, has_error] = GetFlags(args);
    if (has_error)
    {
        return EXIT_FAILURE;
    }

    if (HasFlag(flags, "help"))
    {
        PrintHelp();
        return EXIT_SUCCESS;
    }

    if (argc < 2)
    {
        // If there is no input file, convert all WEM files in the current directory
        bool wem_exists = false;

        for (const auto& entry : fs::directory_iterator(fs::current_path()))
        {
            if (entry.path().extension() != ".wem")
            {
                continue;
            }

            wem_exists = true;
            std::println("Converting {}...", entry.path().string());

            const auto indata = ReadFile(entry.path());
            if (indata.empty())
            {
                std::println(stderr, "Failed to read {}", entry.path().string());
                return EXIT_FAILURE;
            }

            const auto outpath = ReplaceExtension(entry.path(), ".ogg");

            try
            {
                Convert(indata, outpath);
            }
            catch (const std::exception& e)
            {
                std::println(stderr, "Failed to convert {}: {}", entry.path().string(), e.what());
                return EXIT_FAILURE;
            }
        }

        if (!wem_exists)
        {
            PrintHelp("No WEM files found in the current directory!", args[0]);
        }
        return EXIT_SUCCESS;
    }

    if (argc < 3)
    {
        PrintHelp("Missing arguments!", args[0]);
        return EXIT_FAILURE;
    }

    const std::string_view command = args[1];

    // WEM command handling
    if (command == "wem")
    {
        const fs::path path = args[2];
        const auto indata = ReadFile(path);

        if (indata.empty())
        {
            std::println(stderr, "Failed to read {}", path.string());
            return EXIT_FAILURE;
        }

        if (HasFlag(flags, "info"))
        {
            try
            {
                std::print("{}", ww2ogg::wem_info(indata));
            }
            catch (const std::exception& e)
            {
                std::println(stderr, "Failed to read WEM info for {}: {}", path.string(), e.what());
                return EXIT_FAILURE;
            }
            return EXIT_SUCCESS;
        }

        const auto outpath = ReplaceExtension(path, ".ogg");
        std::println("Converting {}...", outpath.string());

        try
        {
            Convert(indata, outpath);
        }
        catch (const std::exception& e)
        {
            std::println(stderr, "Failed to convert {}: {}", path.string(), e.what());
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }

    // BNK command handling
    if (command == "bnk")
    {
        if (argc < 4)
        {
            PrintHelp("You must specify whether to extract or find an event as well as the input!",
                      args[0]);
            return EXIT_FAILURE;
        }

        const std::string_view subcommand = args[2];
        const fs::path bnk_path = args[3];

        const auto indata = ReadFile(bnk_path);
        if (indata.empty())
        {
            std::println(stderr, "Failed to read {}", bnk_path.string());
            return EXIT_FAILURE;
        }

        if (HasFlag(flags, "info"))
        {
            std::print("{}", wwtools::bnk::get_info(indata));
            return EXIT_SUCCESS;
        }

        if (subcommand != "event" && subcommand != "extract")
        {
            PrintHelp("Incorrect value for read or write!", args[0]);
            return EXIT_FAILURE;
        }

        if (subcommand == "event")
        {
            std::string in_event_id;
            if (argc >= 5)
            {
                in_event_id = args[4];
            }

            std::print("{}", wwtools::bnk::get_event_id_info(indata, in_event_id));
            return EXIT_SUCCESS;
        }

        // Extract subcommand
        const auto wems = wwtools::bnk_extract(indata);
        const bool noconvert = HasFlag(flags, "no-convert");

        // --no-convert: extract raw embedded data to subdirectory
        if (noconvert)
        {
            const auto outdir = ReplaceExtension(bnk_path, "");
            fs::create_directory(outdir);

            for (std::size_t i = 0; i < wems.size(); ++i)
            {
                const auto outpath = outdir / (std::to_string(wems[i].id) + ".wem");

                std::cout << rang::fg::cyan << "[" << (i + 1) << "/" << wems.size() << "] "
                          << rang::fg::reset << "Extracting " << outpath.string() << "...\n";

                std::ofstream of(outpath, std::ios::binary);
                of << wems[i].data;
            }
            return EXIT_SUCCESS;
        }

        // Convert mode: handle both embedded and streamed WEMs
        const auto bnk_dir = bnk_path.parent_path();
        const auto bnk_stem = bnk_path.stem().string();

        for (std::size_t i = 0; i < wems.size(); ++i)
        {
            const auto wem_id_str = std::to_string(wems[i].id);
            const auto out_name =
                (wems.size() == 1) ? bnk_stem + ".ogg" : bnk_stem + "_" + wem_id_str + ".ogg";
            const auto outpath = bnk_dir / out_name;

            if (!wems[i].streamed)
            {
                // Fully embedded WEM - convert directly
                std::cout << rang::fg::cyan << "[" << (i + 1) << "/" << wems.size() << "] "
                          << rang::fg::reset << "Converting " << outpath.string() << "...\n";

                try
                {
                    Convert(wems[i].data, outpath);
                }
                catch (const std::exception& e)
                {
                    std::println(stderr, "Failed to convert: {}", e.what());
                }
            }
            else
            {
                // Streamed WEM - look for external .wem file
                const auto external_wem = bnk_dir / (wem_id_str + ".wem");
                if (!fs::exists(external_wem))
                {
                    std::cout << rang::fg::cyan << "[" << (i + 1) << "/" << wems.size() << "] "
                              << rang::fg::reset;
                    std::println(stderr, "WEM {} is streamed but {} not found", wem_id_str,
                                 external_wem.string());
                    continue;
                }

                std::cout << rang::fg::cyan << "[" << (i + 1) << "/" << wems.size() << "] "
                          << rang::fg::reset << "Converting " << external_wem.string() << " -> "
                          << outpath.string() << "...\n";

                const auto wem_data = ReadFile(external_wem);
                if (wem_data.empty())
                {
                    std::println(stderr, "Failed to read {}", external_wem.string());
                    continue;
                }

                try
                {
                    Convert(wem_data, outpath);
                }
                catch (const std::exception& e)
                {
                    std::println(stderr, "Failed to convert {}: {}", external_wem.string(),
                                 e.what());
                }
            }
        }
        return EXIT_SUCCESS;
    }

    // Unknown command
    PrintHelp("Unknown command!", args[0]);
    return EXIT_FAILURE;
}
