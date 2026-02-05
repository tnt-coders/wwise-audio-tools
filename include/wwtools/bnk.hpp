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

#ifndef WWTOOLS_BNK_HPP
#define WWTOOLS_BNK_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "kaitai/kaitaistream.h"
#include "kaitai/structs/bnk.h"

/**
 * @namespace wwtools::bnk
 * @brief contains helper functions for Wwise Soundbank files
 *
 */
namespace wwtools::bnk {

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
[[nodiscard]] auto get_info(std::string_view indata) -> std::string;

/**
 * @brief get WEMs correlating to a BNK and an optional event ID
 *
 * @param indata std::string_view with the file data
 * @param in_event_id the input event ID
 * @return a printable info string
 */
[[nodiscard]] auto get_event_id_info(std::string_view indata,
                                      std::string_view in_event_id) -> std::string;

/**
 * @brief get the ID of a WEM at an index
 *
 * @param indata std::string_view with the file data
 * @param index the index to get the ID from
 * @return the ID as a string
 */
[[nodiscard]] auto get_wem_id_at_index(std::string_view indata,
                                        std::size_t index) -> std::string;

/**
 * @brief get the event name from an event ID
 *
 * @param event_id the event ID to look up
 * @return the event name, or empty string if not found
 */
[[nodiscard]] auto get_event_name_from_id(std::uint32_t event_id) -> std::string;

/**
 * @brief get a string with the action type from the enum
 *
 * @param action_type an action type to be converted to string
 * @return the string name of the action type
 */
[[nodiscard]] auto get_event_action_type(bnk_t::action_type_t action_type) -> std::string_view;

} // namespace wwtools::bnk

#endif // WWTOOLS_BNK_HPP
