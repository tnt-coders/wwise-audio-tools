#pragma once

/**
 * @file errors.h
 * @brief Error types for ww2ogg parsing
 * @note Modernized to C++23
 */

#include <cstddef>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>

namespace ww2ogg {

/**
 * @brief Error for invalid command-line arguments
 */
class Argument_error : public std::runtime_error {
public:
  explicit Argument_error(std::string_view str)
      : std::runtime_error(std::format("Argument error: {}", str)) {}
};

/**
 * @brief Error for file opening failures
 */
class file_open_error : public std::runtime_error {
public:
  explicit file_open_error(std::string_view name)
      : std::runtime_error(std::format("Error opening {}", name)) {}
};

/**
 * @brief Base class for parsing errors
 */
class parse_error : public std::runtime_error {
public:
  parse_error() : std::runtime_error("Parse error: unspecified") {}
  explicit parse_error(std::string_view msg)
      : std::runtime_error(std::format("Parse error: {}", msg)) {}
};

/**
 * @brief Parse error with string message
 */
class parse_error_str : public parse_error {
public:
  explicit parse_error_str(std::string_view s) : parse_error(s) {}
};

/**
 * @brief Error for size mismatches during parsing
 */
class size_mismatch : public parse_error {
public:
  size_mismatch(std::size_t real_s, std::size_t read_s)
      : parse_error(std::format("expected {} bits, read {}", real_s, read_s)) {}
};

/**
 * @brief Error for invalid codebook IDs
 */
class invalid_id : public parse_error {
  int id_;

public:
  explicit invalid_id(int i)
      : parse_error(std::format("invalid codebook id {}, try --inline-codebooks", i)),
        id_(i) {}

  [[nodiscard]] int id() const { return id_; }
};

} // namespace ww2ogg
