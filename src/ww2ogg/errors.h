/**
 * @file errors.h
 * @brief Error types for ww2ogg parsing
 * @note Modernized to C++23
 */

#ifndef WW2OGG_ERRORS_H
#define WW2OGG_ERRORS_H

#include <cstddef>
#include <format>
#include <ostream>
#include <string>
#include <string_view>

namespace ww2ogg {

/**
 * @brief Error for invalid command-line arguments
 */
class Argument_error {
  std::string errmsg_;

public:
  explicit Argument_error(std::string_view str) : errmsg_(str) {}

  friend std::ostream& operator<<(std::ostream& os, const Argument_error& ae) {
    os << "Argument error: " << ae.errmsg_;
    return os;
  }

  [[nodiscard]] std::string_view message() const { return errmsg_; }
};

/**
 * @brief Error for file opening failures
 */
class file_open_error {
  std::string filename_;

public:
  explicit file_open_error(std::string_view name) : filename_(name) {}

  friend std::ostream& operator<<(std::ostream& os, const file_open_error& fe) {
    os << "Error opening " << fe.filename_;
    return os;
  }

  [[nodiscard]] std::string_view filename() const { return filename_; }
};

/**
 * @brief Base class for parsing errors
 */
class parse_error {
public:
  parse_error() = default;
  parse_error(const parse_error&) = default;
  parse_error(parse_error&&) = default;
  parse_error& operator=(const parse_error&) = default;
  parse_error& operator=(parse_error&&) = default;
  virtual ~parse_error() = default;

  virtual void print_self(std::ostream& os) const { os << "unspecified."; }

  friend std::ostream& operator<<(std::ostream& os, const parse_error& pe) {
    os << "Parse error: ";
    pe.print_self(os);
    return os;
  }
};

/**
 * @brief Parse error with string message
 */
class parse_error_str : public parse_error {
  std::string str_;

public:
  explicit parse_error_str(std::string_view s) : str_(s) {}

  void print_self(std::ostream& os) const override { os << str_; }

  [[nodiscard]] std::string_view message() const { return str_; }
};

/**
 * @brief Error for size mismatches during parsing
 */
class size_mismatch : public parse_error {
  std::size_t real_size_;
  std::size_t read_size_;

public:
  size_mismatch(std::size_t real_s, std::size_t read_s)
      : real_size_(real_s), read_size_(read_s) {}

  void print_self(std::ostream& os) const override {
    os << std::format("expected {} bits, read {}", real_size_, read_size_);
  }

  [[nodiscard]] std::size_t expected_size() const { return real_size_; }
  [[nodiscard]] std::size_t actual_size() const { return read_size_; }
};

/**
 * @brief Error for invalid codebook IDs
 */
class invalid_id : public parse_error {
  int id_;

public:
  explicit invalid_id(int i) : id_(i) {}

  void print_self(std::ostream& os) const override {
    os << std::format("invalid codebook id {}, try --inline-codebooks", id_);
  }

  [[nodiscard]] int id() const { return id_; }
};

} // namespace ww2ogg

#endif // WW2OGG_ERRORS_H
