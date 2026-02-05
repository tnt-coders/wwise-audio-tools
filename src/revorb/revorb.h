#pragma once

/**
 * @file revorb.hpp
 * @brief OGG Vorbis granule position recomputation
 * @note Modernized to C++23
 *
 * REVORB - Recomputes page granule positions in Ogg Vorbis files.
 * Based on version 0.2 (2008/06/29) by Jiri Hruska
 */

#include <istream>
#include <sstream>

namespace revorb {

/**
 * @brief Recompute granule positions in an OGG Vorbis stream
 *
 * @param indata Input OGG Vorbis stream
 * @param outdata Output stream with corrected granule positions
 * @return true on success, false on failure
 */
[[nodiscard]] bool revorb(std::istream& indata, std::stringstream& outdata);

} // namespace revorb
