/**
 * @file wem.cpp
 * @brief Unit tests for WEM to OGG conversion
 * @note Modernized to C++23
 */

#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <sstream>
#include <string>

#include "wwtools/wwtools.h"

namespace {

[[nodiscard]] std::string Convert(const std::string& path) {
  std::ifstream filein(path, std::ios::binary);

  // Reserve memory upfront
  std::string indata;
  filein.seekg(0, std::ios::end);
  indata.reserve(static_cast<std::size_t>(filein.tellg()));
  filein.seekg(0, std::ios::beg);

  indata.assign(std::istreambuf_iterator<char>(filein),
                std::istreambuf_iterator<char>());

  return wwtools::wem_to_ogg(indata);
}

} // anonymous namespace

TEST_CASE("Compare WEM converted with WwiseAudioTools to those converted with standalone tools",
          "[wwise-audio-tools]") {
  std::ifstream ogg_in("testdata/wem/test1.ogg", std::ios::binary);
  std::stringstream ogg_in_s;
  ogg_in_s << ogg_in.rdbuf();

  REQUIRE(Convert("testdata/wem/test1.wem") == ogg_in_s.str());
}
