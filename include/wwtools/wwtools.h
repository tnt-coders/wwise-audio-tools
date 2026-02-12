#pragma once

/**
 * @file wwtools.hpp
 * @author Abheek Dhawan (abheekd at protonmail dot com)
 * @brief helper functions for other Wwise file actions
 * @date 2022-05-26
 * @note Modernized to C++23
 *
 * @copyright Copyright (c) 2022 RED Modding Tools
 *
 */

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

/**
 * @namespace wwtools
 * @brief parent namespace for specific file type helper functions
 *
 */
namespace wwtools
{

/**
 * @brief Information about a WEM referenced by a BNK soundbank
 */
struct BnkWem
{
    std::uint32_t id; ///< WEM ID
    bool streamed;    ///< true if the WEM is streamed (external .wem file needed)
    std::string data; ///< Embedded WEM data (full file if !streamed, prefetch stub if streamed)
};

/**
 * @brief get OGG file data from WEM file data
 *
 * @param indata WEM file data
 * @return OGG file data
 * @throws std::exception on conversion failure
 */
[[nodiscard]] std::string wem_to_ogg(std::string_view indata);

/**
 * @brief extract all WEMs from a BNK soundbank with their IDs and streaming status
 *
 * For embedded WEMs, data contains the full WEM audio.
 * For streamed WEMs, data contains only the prefetch stub; the caller
 * must load the full audio from an external <id>.wem file.
 *
 * @param indata BNK file data
 * @return vector of BnkWem structs
 */
[[nodiscard]] std::vector<BnkWem> bnk_extract(std::string_view indata);

} // namespace wwtools
