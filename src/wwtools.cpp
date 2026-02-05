/**
 * @file wwtools.cpp
 * @brief Implementation of WEM to OGG conversion
 * @note Modernized to C++23
 */

#include <sstream>
#include <string>
#include <string_view>

#include "revorb/revorb.hpp"
#include "ww2ogg/ww2ogg.h"
#include "wwtools/wwtools.hpp"

namespace wwtools {

[[nodiscard]] auto wem_to_ogg(std::string_view indata) -> std::string {
  std::stringstream wem_out;
  std::stringstream revorb_out;

  // Convert WEM to intermediate OGG format
  if (!ww2ogg::ww2ogg(std::string{indata}, wem_out)) {
    return {};
  }

  // Fix granule positions in the OGG stream
  if (!revorb::revorb(wem_out, revorb_out)) {
    return {};
  }

  return revorb_out.str();
}

} // namespace wwtools
