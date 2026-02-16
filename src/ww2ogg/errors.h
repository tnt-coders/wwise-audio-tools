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

namespace ww2ogg
{

/**
 * @brief Error for invalid command-line arguments
 */
class ArgumentError : public std::runtime_error
{
  public:
    explicit ArgumentError(std::string_view str)
        : std::runtime_error(std::format("Argument error: {}", str))
    {
    }
};

/**
 * @brief Error for file opening failures
 */
class FileOpenError : public std::runtime_error
{
  public:
    explicit FileOpenError(std::string_view name)
        : std::runtime_error(std::format("Error opening {}", name))
    {
    }
};

/**
 * @brief Base class for parsing errors
 */
class ParseError : public std::runtime_error
{
  public:
    ParseError() : std::runtime_error("Parse error: unspecified")
    {
    }
    explicit ParseError(std::string_view msg)
        : std::runtime_error(std::format("Parse error: {}", msg))
    {
    }
};

/**
 * @brief Parse error with string message
 */
class ParseErrorStr : public ParseError
{
  public:
    explicit ParseErrorStr(std::string_view s) : ParseError(s)
    {
    }
};

/**
 * @brief Error for size mismatches during parsing
 */
class SizeMismatch : public ParseError
{
  public:
    SizeMismatch(std::size_t real_s, std::size_t read_s)
        : ParseError(std::format("expected {} bits, read {}", real_s, read_s))
    {
    }
};

/**
 * @brief Error for invalid codebook IDs
 */
class InvalidId : public ParseError
{
    int m_id;

  public:
    explicit InvalidId(int i)
        : ParseError(std::format("invalid codebook id {}, try --inline-codebooks", i)), m_id(i)
    {
    }

    [[nodiscard]] int Id() const
    {
        return m_id;
    }
};

} // namespace ww2ogg
