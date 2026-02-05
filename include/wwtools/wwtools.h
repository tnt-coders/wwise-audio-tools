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

#ifndef WWTOOLS_WWTOOLS_HPP
#define WWTOOLS_WWTOOLS_HPP

#include <string>
#include <string_view>

/**
 * @namespace wwtools
 * @brief parent namespace for specific file type helper functions
 *
 */
namespace wwtools {

/**
 * @brief get OGG file data from WEM file data
 *
 * @param indata WEM file data
 * @return OGG file data (empty string on failure)
 */
[[nodiscard]] std::string wem_to_ogg(std::string_view indata);

} // namespace wwtools

#endif // WWTOOLS_WWTOOLS_HPP
