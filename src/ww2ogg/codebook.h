#pragma once

/**
 * @file codebook.h
 * @brief Vorbis codebook handling for ww2ogg
 * @note Modernized to C++23
 */

#include <string>
#include <vector>

#include "bitstream.h"
#include "errors.h"

// Helper functions from Tremor (lowmem)
namespace {

/**
 * @brief Integer logarithm base 2
 */
[[nodiscard]] inline int ilog(unsigned int v) {
  int ret = 0;
  while (v != 0) {
    ++ret;
    v >>= 1;
  }
  return ret;
}

/**
 * @brief Calculate quantization values for maptype 1 codebooks
 */
[[nodiscard]] inline unsigned int _book_maptype1_quantvals(const unsigned int entries,
                                                            const unsigned int dimensions) {
  // Get us a starting hint, we'll polish it below
  const int bits = ilog(entries);
  int vals = static_cast<int>(entries >> ((bits - 1) * (dimensions - 1) / dimensions));

  while (true) {
    unsigned long acc = 1;
    unsigned long acc1 = 1;
    for (unsigned int i = 0; i < dimensions; ++i) {
      acc *= static_cast<unsigned long>(vals);
      acc1 *= static_cast<unsigned long>(vals + 1);
    }
    if (acc <= entries && acc1 > entries) {
      return static_cast<unsigned int>(vals);
    }
    if (acc > entries) {
      --vals;
    } else {
      ++vals;
    }
  }
}

} // anonymous namespace

namespace ww2ogg {

/**
 * @brief Manages a library of Vorbis codebooks
 */
class codebook_library {
  std::vector<char> codebook_data;
  std::vector<long> codebook_offsets;

  // Non-copyable
  codebook_library(const codebook_library&) = delete;
  codebook_library& operator=(const codebook_library&) = delete;

public:
  // Movable
  codebook_library(codebook_library&&) = default;
  codebook_library& operator=(codebook_library&&) = default;

  /**
   * @brief Construct from codebook data string
   */
  explicit codebook_library(const std::string& indata);

  /**
   * @brief Construct empty library (for inline codebooks)
   */
  codebook_library();

  ~codebook_library() = default;

  /**
   * @brief Get pointer to codebook data
   */
  [[nodiscard]] const char* get_codebook(int i) const {
    if (codebook_data.empty() || codebook_offsets.empty()) {
      throw parse_error_str("codebook library not loaded");
    }
    if (i >= static_cast<int>(codebook_offsets.size()) - 1 || i < 0) {
      return nullptr;
    }
    return &codebook_data[codebook_offsets[i]];
  }

  /**
   * @brief Get size of codebook
   */
  [[nodiscard]] long get_codebook_size(int i) const {
    if (codebook_data.empty() || codebook_offsets.empty()) {
      throw parse_error_str("codebook library not loaded");
    }
    if (i >= static_cast<int>(codebook_offsets.size()) - 1 || i < 0) {
      return -1;
    }
    return codebook_offsets[i + 1] - codebook_offsets[i];
  }

  void rebuild(int i, bitoggstream& bos);
  void rebuild(bitstream& bis, unsigned long cb_size, bitoggstream& bos);
  void copy(bitstream& bis, bitoggstream& bos);
};

} // namespace ww2ogg
