#pragma once

/**
 * @file bitstream.h
 * @brief Bit-level stream reading/writing for OGG generation
 * @note Modernized to C++23
 */

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif

#include <array>
#include <cstdint>
#include <istream>
#include <limits>
#include <ostream>
#include <vector>

#include "crc.h"
#include "errors.h"

// Host-endian-neutral integer reading/writing utilities
namespace
{

[[nodiscard]] inline uint32_t Read32Le(unsigned char b[4])
{
    uint32_t v = 0;
    for (int i = 3; i >= 0; --i)
    {
        v <<= 8;
        v |= b[i];
    }
    return v;
}

[[nodiscard]] inline uint32_t Read32Le(std::istream& is)
{
    char b[4];
    is.read(b, 4);
    return Read32Le(reinterpret_cast<unsigned char*>(b));
}

inline void Write32Le(unsigned char b[4], uint32_t v)
{
    for (int i = 0; i < 4; ++i)
    {
        b[i] = static_cast<unsigned char>(v & 0xFF);
        v >>= 8;
    }
}

inline void Write32Le(std::ostream& os, uint32_t v)
{
    char b[4];
    Write32Le(reinterpret_cast<unsigned char*>(b), v);
    os.write(b, 4);
}

[[nodiscard]] inline uint16_t Read16Le(unsigned char b[2])
{
    uint16_t v = 0;
    for (int i = 1; i >= 0; --i)
    {
        v <<= 8;
        v |= b[i];
    }
    return v;
}

[[nodiscard]] inline uint16_t Read16Le(std::istream& is)
{
    char b[2];
    is.read(b, 2);
    return Read16Le(reinterpret_cast<unsigned char*>(b));
}

inline void Write16Le(unsigned char b[2], uint16_t v)
{
    for (int i = 0; i < 2; ++i)
    {
        b[i] = static_cast<unsigned char>(v & 0xFF);
        v >>= 8;
    }
}

inline void Write16Le(std::ostream& os, uint16_t v)
{
    char b[2];
    Write16Le(reinterpret_cast<unsigned char*>(b), v);
    os.write(b, 2);
}

[[nodiscard]] inline uint32_t Read32Be(unsigned char b[4])
{
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i)
    {
        v <<= 8;
        v |= b[i];
    }
    return v;
}

[[nodiscard]] inline uint32_t Read32Be(std::istream& is)
{
    char b[4];
    is.read(b, 4);
    return Read32Be(reinterpret_cast<unsigned char*>(b));
}

inline void Write32Be(unsigned char b[4], uint32_t v)
{
    for (int i = 3; i >= 0; --i)
    {
        b[i] = static_cast<unsigned char>(v & 0xFF);
        v >>= 8;
    }
}

inline void Write32Be(std::ostream& os, uint32_t v)
{
    char b[4];
    Write32Be(reinterpret_cast<unsigned char*>(b), v);
    os.write(b, 4);
}

[[nodiscard]] inline uint16_t Read16Be(unsigned char b[2])
{
    uint16_t v = 0;
    for (int i = 0; i < 2; ++i)
    {
        v <<= 8;
        v |= b[i];
    }
    return v;
}

[[nodiscard]] inline uint16_t Read16Be(std::istream& is)
{
    char b[2];
    is.read(b, 2);
    return Read16Be(reinterpret_cast<unsigned char*>(b));
}

inline void Write16Be(unsigned char b[2], uint16_t v)
{
    for (int i = 1; i >= 0; --i)
    {
        b[i] = static_cast<unsigned char>(v & 0xFF);
        v >>= 8;
    }
}

inline void Write16Be(std::ostream& os, uint16_t v)
{
    char b[2];
    Write16Be(reinterpret_cast<unsigned char*>(b), v);
    os.write(b, 2);
}

} // anonymous namespace

namespace ww2ogg
{

/**
 * @brief Bit-level input stream reader (LSB first)
 */
class bitstream
{
    std::istream& m_is;

    unsigned char m_bit_buffer;
    unsigned int m_bits_left;
    unsigned long m_total_bits_read;

  public:
    class Weird_char_size
    {
    };
    class Out_of_bits
    {
    };

    explicit bitstream(std::istream& is)
        : m_is(is), m_bit_buffer(0), m_bits_left(0), m_total_bits_read(0)
    {
        if (std::numeric_limits<unsigned char>::digits != 8)
            throw Weird_char_size();
    }

    [[nodiscard]] bool GetBit()
    {
        if (m_bits_left == 0)
        {
            int c = m_is.get();
            if (c == EOF)
                throw Out_of_bits();
            m_bit_buffer = static_cast<unsigned char>(c);
            m_bits_left = 8;
        }
        ++m_total_bits_read;
        --m_bits_left;
        return ((m_bit_buffer & (0x80 >> m_bits_left)) != 0);
    }

    [[nodiscard]] unsigned long GetTotalBitsRead() const
    {
        return m_total_bits_read;
    }
};

/**
 * @brief Bit-level OGG output stream writer
 */
class bitoggstream
{
    std::ostream& m_os;

    unsigned char m_bit_buffer;
    unsigned int m_bits_stored;

    enum
    {
        header_bytes = 27,
        max_segments = 255,
        segment_size = 255
    };

    unsigned int m_payload_bytes;
    bool m_first, m_continued;
    std::array<unsigned char, header_bytes + max_segments + segment_size * max_segments>
        m_page_buffer{};
    uint32_t m_granule;
    uint32_t m_seqno;

  public:
    class Weird_char_size
    {
    };

    explicit bitoggstream(std::ostream& os)
        : m_os(os), m_bit_buffer(0), m_bits_stored(0), m_payload_bytes(0), m_first(true),
          m_continued(false), m_granule(0), m_seqno(0)
    {
        if (std::numeric_limits<unsigned char>::digits != 8)
            throw Weird_char_size();
    }

    void PutBit(const bool bit)
    {
        if (bit)
            m_bit_buffer |= static_cast<unsigned char>(1 << m_bits_stored);

        ++m_bits_stored;
        if (m_bits_stored == 8)
        {
            FlushBits();
        }
    }

    void SetGranule(const uint32_t g)
    {
        m_granule = g;
    }

    void FlushBits()
    {
        if (m_bits_stored != 0)
        {
            if (m_payload_bytes == segment_size * max_segments)
            {
                throw parse_error_str("ran out of space in an Ogg packet");
            }

            m_page_buffer[header_bytes + max_segments + m_payload_bytes] = m_bit_buffer;
            ++m_payload_bytes;

            m_bits_stored = 0;
            m_bit_buffer = 0;
        }
    }

    void FlushPage(const bool next_continued = false, const bool last = false)
    {
        if (m_payload_bytes != segment_size * max_segments)
        {
            FlushBits();
        }

        if (m_payload_bytes != 0)
        {
            unsigned int segments =
                (m_payload_bytes + segment_size) / segment_size; // intentionally round up
            if (segments == max_segments + 1)
                segments = max_segments; // at max eschews the final 0

            // move payload back
            for (unsigned int i = 0; i < m_payload_bytes; ++i)
            {
                m_page_buffer[header_bytes + segments + i] =
                    m_page_buffer[header_bytes + max_segments + i];
            }

            m_page_buffer[0] = 'O';
            m_page_buffer[1] = 'g';
            m_page_buffer[2] = 'g';
            m_page_buffer[3] = 'S';
            m_page_buffer[4] = 0; // stream_structure_version
            m_page_buffer[5] = static_cast<unsigned char>((m_continued ? 1 : 0) |
                                                          (m_first ? 2 : 0) | (last ? 4 : 0));
            Write32Le(&m_page_buffer[6], m_granule); // granule low bits
            Write32Le(&m_page_buffer[10], 0);        // granule high bits
            if (m_granule == UINT32_C(0xFFFFFFFF))
                Write32Le(&m_page_buffer[10], UINT32_C(0xFFFFFFFF));
            Write32Le(&m_page_buffer[14], 1);       // stream serial number
            Write32Le(&m_page_buffer[18], m_seqno); // page sequence number
            Write32Le(&m_page_buffer[22], 0);       // checksum (0 for now)
            m_page_buffer[26] = static_cast<unsigned char>(segments);

            // lacing values
            for (unsigned int i = 0, bytes_left = m_payload_bytes; i < segments; ++i)
            {
                if (bytes_left >= segment_size)
                {
                    bytes_left -= segment_size;
                    m_page_buffer[27 + i] = segment_size;
                }
                else
                {
                    m_page_buffer[27 + i] = static_cast<unsigned char>(bytes_left);
                }
            }

            // checksum
            Write32Le(&m_page_buffer[22],
                      checksum(m_page_buffer.data(), header_bytes + segments + m_payload_bytes));

            // output to ostream
            for (unsigned int i = 0; i < header_bytes + segments + m_payload_bytes; ++i)
            {
                m_os.put(static_cast<char>(m_page_buffer[i]));
            }

            ++m_seqno;
            m_first = false;
            m_continued = next_continued;
            m_payload_bytes = 0;
        }
    }

    ~bitoggstream() noexcept
    {
        try
        {
            FlushPage();
        }
        catch (...)
        {
        }
    }
};

/**
 * @brief Fixed-size bit integer for compile-time sized bit fields
 */
template <unsigned int BIT_SIZE> class Bit_uint
{
    unsigned int m_total;

  public:
    class Too_many_bits
    {
    };
    class Int_too_big
    {
    };

    Bit_uint() : m_total(0)
    {
        if (BIT_SIZE > static_cast<unsigned int>(std::numeric_limits<unsigned int>::digits))
            throw Too_many_bits();
    }

    explicit Bit_uint(const unsigned int v) : m_total(v)
    {
        if (BIT_SIZE > static_cast<unsigned int>(std::numeric_limits<unsigned int>::digits))
            throw Too_many_bits();
        if ((v >> (BIT_SIZE - 1U)) > 1U)
            throw Int_too_big();
    }

    Bit_uint& operator=(unsigned int v)
    {
        if ((v >> (BIT_SIZE - 1U)) > 1U)
            throw Int_too_big();
        m_total = v;
        return *this;
    }

    [[nodiscard]] operator unsigned int() const
    {
        return m_total;
    }

    friend bitstream& operator>>(bitstream& bstream, Bit_uint& bui)
    {
        bui.m_total = 0;
        for (unsigned int i = 0; i < BIT_SIZE; ++i)
        {
            if (bstream.GetBit())
                bui.m_total |= (1U << i);
        }
        return bstream;
    }

    friend bitoggstream& operator<<(bitoggstream& bstream, const Bit_uint& bui)
    {
        for (unsigned int i = 0; i < BIT_SIZE; ++i)
        {
            bstream.PutBit((bui.m_total & (1U << i)) != 0);
        }
        return bstream;
    }
};

/**
 * @brief Variable-size bit integer for runtime-sized bit fields
 */
class Bit_uintv
{
    unsigned int m_size;
    unsigned int m_total;

  public:
    class Too_many_bits
    {
    };
    class Int_too_big
    {
    };

    explicit Bit_uintv(const unsigned int s) : m_size(s), m_total(0)
    {
        if (s > static_cast<unsigned int>(std::numeric_limits<unsigned int>::digits))
            throw Too_many_bits();
    }

    Bit_uintv(const unsigned int s, const unsigned int v) : m_size(s), m_total(v)
    {
        if (m_size > static_cast<unsigned int>(std::numeric_limits<unsigned int>::digits))
            throw Too_many_bits();
        if ((v >> (m_size - 1U)) > 1U)
            throw Int_too_big();
    }

    Bit_uintv& operator=(unsigned int v)
    {
        if ((v >> (m_size - 1U)) > 1U)
            throw Int_too_big();
        m_total = v;
        return *this;
    }

    [[nodiscard]] operator unsigned int() const
    {
        return m_total;
    }

    friend bitstream& operator>>(bitstream& bstream, Bit_uintv& bui)
    {
        bui.m_total = 0;
        for (unsigned int i = 0; i < bui.m_size; ++i)
        {
            if (bstream.GetBit())
                bui.m_total |= (1U << i);
        }
        return bstream;
    }

    friend bitoggstream& operator<<(bitoggstream& bstream, const Bit_uintv& bui)
    {
        for (unsigned int i = 0; i < bui.m_size; ++i)
        {
            bstream.PutBit((bui.m_total & (1U << i)) != 0);
        }
        return bstream;
    }
};

/**
 * @brief Stream buffer backed by a character array
 */
class array_streambuf : public std::streambuf
{
    // Non-copyable, non-movable
    array_streambuf& operator=(const array_streambuf&) = delete;
    array_streambuf(const array_streambuf&) = delete;
    array_streambuf(array_streambuf&&) = delete;
    array_streambuf& operator=(array_streambuf&&) = delete;

    std::vector<char> m_arr;

  public:
    array_streambuf(const char* const a, const int l) : m_arr(a, a + l)
    {
        setg(m_arr.data(), m_arr.data(), m_arr.data() + m_arr.size());
    }

    ~array_streambuf() override = default;
};

} // namespace ww2ogg
