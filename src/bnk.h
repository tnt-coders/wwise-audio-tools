#pragma once

/**
 * @file bnk.hpp
 * @author Abheek Dhawan (abheekd at protonmail dot com)
 * @brief works with Wwise Soundbank files
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
 * @namespace wwtools::bnk
 * @brief contains helper functions for Wwise Soundbank files
 *
 */
namespace wwtools::bnk
{

/**
 * @brief Extract BNK to array of WEMS
 *
 * @param indata std::string_view with the BNK content
 * @param outdata vector of std::string that has all the embedded WEM files
 */
void extract(std::string_view indata, std::vector<std::string>& outdata);

/**
 * @brief get the info string
 *
 * @param indata std::string_view with the file data
 * @return a printable info string
 */
[[nodiscard]] std::string get_info(std::string_view indata);

/**
 * @brief get WEMs correlating to a BNK and an optional event ID
 *
 * @param indata std::string_view with the file data
 * @param in_event_id the input event ID
 * @return a printable info string
 */
[[nodiscard]] std::string get_event_id_info(std::string_view indata, std::string_view in_event_id);

/**
 * @brief get the ID of a WEM at an index
 *
 * @param indata std::string_view with the file data
 * @param index the index to get the ID from
 * @return the ID as a string
 */
[[nodiscard]] std::string get_wem_id_at_index(std::string_view indata, std::size_t index);

/**
 * @brief get the event name from an event ID
 *
 * @param event_id the event ID to look up
 * @return the event name, or empty string if not found
 */
[[nodiscard]] std::string get_event_name_from_id(std::uint32_t event_id);

/**
 * @brief get all WEM IDs referenced by the soundbank's DIDX section
 *
 * @param indata std::string_view with the BNK content
 * @return vector of WEM IDs
 */
[[nodiscard]] std::vector<std::uint32_t> get_wem_ids(std::string_view indata);

/**
 * @brief get the set of WEM IDs that are streamed (not fully embedded)
 *
 * Parses the HIRC section to find Sound Effect/Voice objects and checks
 * the included_or_streamed field. WEM IDs with non-zero values are streamed.
 *
 * @param indata std::string_view with the BNK content
 * @return set of WEM IDs that are streamed
 */
[[nodiscard]] std::vector<std::uint32_t> get_streamed_wem_ids(std::string_view indata);

} // namespace wwtools::bnk
