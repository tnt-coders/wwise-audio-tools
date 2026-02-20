#pragma once

#include <cstddef>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>

// Exception hierarchy for ww2ogg parsing and conversion errors.
// All derive from std::runtime_error.  ParseError is the common base for data-format errors;
// ParseErrorStr adds a string message, SizeMismatch reports byte-count discrepancies,
// and InvalidId reports an unknown codebook ID (suggesting --inline-codebooks).
namespace ww2ogg
{

class ArgumentError : public std::runtime_error
{
  public:
    explicit ArgumentError(std::string_view str)
        : std::runtime_error(std::format("Argument error: {}", str))
    {
    }
};

class FileOpenError : public std::runtime_error
{
  public:
    explicit FileOpenError(std::string_view name)
        : std::runtime_error(std::format("Error opening {}", name))
    {
    }
};

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

class ParseErrorStr : public ParseError
{
  public:
    explicit ParseErrorStr(std::string_view s) : ParseError(s)
    {
    }
};

class SizeMismatch : public ParseError
{
  public:
    SizeMismatch(std::size_t real_s, std::size_t read_s)
        : ParseError(std::format("expected {} bits, read {}", real_s, read_s))
    {
    }
};

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
