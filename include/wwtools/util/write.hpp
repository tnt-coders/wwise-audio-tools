/**
 * @file write.hpp
 * @brief Endian-aware data writing utilities
 * @note Modernized to C++23
 */

#ifndef WWTOOLS_UTIL_WRITE_HPP
#define WWTOOLS_UTIL_WRITE_HPP

#include <array>
#include <bit>
#include <cstddef>
#include <cstring>
#include <ostream>
#include <span>
#include <type_traits>

/**
 * @namespace wwtools::util::write
 * @brief helper functions for writing data with endian conversion
 */
namespace wwtools::util::write {

/**
 * @brief Write data in big-endian (reverse) byte order
 *
 * @tparam T Type to determine size of data (must be trivially copyable)
 * @param data Pointer to source data
 * @param os Output stream to write to
 */
template <typename T>
  requires std::is_trivially_copyable_v<T>
constexpr void big_endian(const char* data, std::ostream& os) {
  std::array<char, sizeof(T)> arr_data{};
  std::memcpy(arr_data.data(), data, sizeof(T));

  for (std::size_t i = sizeof(T); i > 0; --i) {
    os.put(arr_data[i - 1]);
  }
}

/**
 * @brief Write data of a certain type in big-endian byte order
 *
 * @tparam T Type for data and size (must be trivially copyable)
 * @param data Data to be written
 * @param os Output stream to write to
 */
template <typename T>
  requires std::is_trivially_copyable_v<T>
constexpr void big_endian(const T& data, std::ostream& os) {
  big_endian<T>(reinterpret_cast<const char*>(&data), os);
}

/**
 * @brief Write data in little-endian (native on most systems) byte order
 *
 * @tparam T Type to determine size of data (must be trivially copyable)
 * @param data Pointer to source data
 * @param os Output stream to write to
 */
template <typename T>
  requires std::is_trivially_copyable_v<T>
constexpr void little_endian(const char* data, std::ostream& os) {
  std::array<char, sizeof(T)> arr_data{};
  std::memcpy(arr_data.data(), data, sizeof(T));

  for (std::size_t i = 0; i < sizeof(T); ++i) {
    os.put(arr_data[i]);
  }
}

/**
 * @brief Write data of a certain type in little-endian byte order
 *
 * @tparam T Type for data and size (must be trivially copyable)
 * @param data Data to be written
 * @param os Output stream to write to
 */
template <typename T>
  requires std::is_trivially_copyable_v<T>
constexpr void little_endian(const T& data, std::ostream& os) {
  little_endian<T>(reinterpret_cast<const char*>(&data), os);
}

/**
 * @brief Write raw data to stream
 *
 * @param data Span of data to be written
 * @param os Output stream to write to
 */
inline void raw_data(std::span<const char> data, std::ostream& os) {
  os.write(data.data(), static_cast<std::streamsize>(data.size()));
}

/**
 * @brief Write raw data to stream (legacy overload)
 *
 * @param data Pointer to data to be written
 * @param size Size of data in bytes
 * @param os Output stream to write to
 */
inline void raw_data(const char* data, std::size_t size, std::ostream& os) {
  os.write(data, static_cast<std::streamsize>(size));
}

} // namespace wwtools::util::write

#endif // WWTOOLS_UTIL_WRITE_HPP
