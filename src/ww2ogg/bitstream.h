#pragma once

#include <array>
#include <cstdint>
#include <istream>
#include <limits>
#include <ostream>
#include <vector>

#include "crc.h"
#include "errors.h"

// Host-endian-neutral integer reading/writing utilities.
// These manually assemble multi-byte integers byte-by-byte so they produce correct
// results regardless of the host CPU's native endianness.
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

// Input bitstream that reads bits one at a time from an underlying byte stream.
// Bits are consumed MSB-first within each byte.
// Used to parse Wwise's compact codebook/setup representations.
class Bitstream
{
    std::istream& m_is;

    unsigned char m_bit_buffer{0}; // current byte being consumed
    unsigned int m_bits_left{0};   // unconsumed bits remaining in m_bit_buffer
    unsigned long m_total_bits_read{0};

  public:
    class WeirdCharSize
    {
    };
    class OutOfBits
    {
    };

    explicit Bitstream(std::istream& is) : m_is(is)
    {
        if (std::numeric_limits<unsigned char>::digits != 8)
            throw WeirdCharSize();
    }

    [[nodiscard]] bool GetBit()
    {
        if (m_bits_left == 0)
        {
            int c = m_is.get();
            if (c == EOF)
                throw OutOfBits();
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

// Output bitstream that accumulates bits and flushes them as complete OGG pages.
//
// Bits are accumulated LSB-first (matching Vorbis bit-packing order), collected into
// a page buffer, and written as OGG pages with correct headers, segment tables,
// and CRC checksums when FlushPage() is called.
//
// The page buffer is sized to hold a maximum-length OGG page:
//   27 bytes header + 255 segment table entries + 255*255 bytes payload
class Bitoggstream
{
    std::ostream& m_os;

    unsigned char m_bit_buffer{0}; // partial byte being assembled
    unsigned int m_bits_stored{0}; // bits written into m_bit_buffer so far

    enum
    {
        HEADER_BYTES = 27,  // OGG page header size
        MAX_SEGMENTS = 255, // max segments per OGG page
        SEGMENT_SIZE = 255  // max bytes per segment
    };

    unsigned int m_payload_bytes{0}; // bytes accumulated in current page
    bool m_first{true};              // true for BOS (beginning of stream) page
    bool m_continued{false};         // packet continues from previous page
    std::array<unsigned char, HEADER_BYTES + MAX_SEGMENTS + SEGMENT_SIZE * MAX_SEGMENTS>
        m_page_buffer{};   // workspace: payload stored at offset HEADER_BYTES+MAX_SEGMENTS
    uint32_t m_granule{0}; // granule position for current page
    uint32_t m_seqno{0};   // incrementing page sequence number

  public:
    class WeirdCharSize
    {
    };

    explicit Bitoggstream(std::ostream& os) : m_os(os)
    {
        if (std::numeric_limits<unsigned char>::digits != 8)
            throw WeirdCharSize();
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
            if (m_payload_bytes == SEGMENT_SIZE * MAX_SEGMENTS)
            {
                throw ParseErrorStr("ran out of space in an Ogg packet");
            }

            m_page_buffer[HEADER_BYTES + MAX_SEGMENTS + m_payload_bytes] = m_bit_buffer;
            ++m_payload_bytes;

            m_bits_stored = 0;
            m_bit_buffer = 0;
        }
    }

    void FlushPage(const bool next_continued = false, const bool last = false)
    {
        if (m_payload_bytes != SEGMENT_SIZE * MAX_SEGMENTS)
        {
            FlushBits();
        }

        if (m_payload_bytes != 0)
        {
            unsigned int segments =
                (m_payload_bytes + SEGMENT_SIZE) / SEGMENT_SIZE; // intentionally round up
            if (segments == MAX_SEGMENTS + 1)
                segments = MAX_SEGMENTS; // at max eschews the final 0

            // move payload back
            for (unsigned int i = 0; i < m_payload_bytes; ++i)
            {
                m_page_buffer[HEADER_BYTES + segments + i] =
                    m_page_buffer[HEADER_BYTES + MAX_SEGMENTS + i];
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
                if (bytes_left >= SEGMENT_SIZE)
                {
                    bytes_left -= SEGMENT_SIZE;
                    m_page_buffer[27 + i] = SEGMENT_SIZE;
                }
                else
                {
                    m_page_buffer[27 + i] = static_cast<unsigned char>(bytes_left);
                }
            }

            // checksum
            Write32Le(&m_page_buffer[22],
                      Checksum(m_page_buffer.data(), HEADER_BYTES + segments + m_payload_bytes));

            // output to ostream
            for (unsigned int i = 0; i < HEADER_BYTES + segments + m_payload_bytes; ++i)
            {
                m_os.put(static_cast<char>(m_page_buffer[i]));
            }

            ++m_seqno;
            m_first = false;
            m_continued = next_continued;
            m_payload_bytes = 0;
        }
    }

    ~Bitoggstream() noexcept
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

// Fixed-width unsigned integer for bitstream I/O.
// BitSize is the number of bits to read/write (compile-time constant).
// Supports streaming from Bitstream (input) and to Bitoggstream (output).
// Bits are read/written LSB-first (Vorbis bit-packing convention).
template <unsigned int BitSize> class BitUint
{
    unsigned int m_total;

  public:
    class TooManyBits
    {
    };
    class IntTooBig
    {
    };

    BitUint() : m_total(0)
    {
        if (BitSize > static_cast<unsigned int>(std::numeric_limits<unsigned int>::digits))
            throw TooManyBits();
    }

    explicit BitUint(const unsigned int v) : m_total(v)
    {
        if (BitSize > static_cast<unsigned int>(std::numeric_limits<unsigned int>::digits))
            throw TooManyBits();
        if ((v >> (BitSize - 1U)) > 1U)
            throw IntTooBig();
    }

    BitUint& operator=(unsigned int v)
    {
        if ((v >> (BitSize - 1U)) > 1U)
            throw IntTooBig();
        m_total = v;
        return *this;
    }

    [[nodiscard]] operator unsigned int() const
    {
        return m_total;
    }

    friend Bitstream& operator>>(Bitstream& bstream, BitUint& bui)
    {
        bui.m_total = 0;
        for (unsigned int i = 0; i < BitSize; ++i)
        {
            if (bstream.GetBit())
                bui.m_total |= (1U << i);
        }
        return bstream;
    }

    friend Bitoggstream& operator<<(Bitoggstream& bstream, const BitUint& bui)
    {
        for (unsigned int i = 0; i < BitSize; ++i)
        {
            bstream.PutBit((bui.m_total & (1U << i)) != 0);
        }
        return bstream;
    }
};

// Variable-width unsigned integer for bitstream I/O.
// Like BitUint but the bit width is determined at runtime (passed to constructor).
class BitUintv
{
    unsigned int m_size; // number of bits
    unsigned int m_total;

  public:
    class TooManyBits
    {
    };
    class IntTooBig
    {
    };

    explicit BitUintv(const unsigned int s) : m_size(s), m_total(0)
    {
        if (s > static_cast<unsigned int>(std::numeric_limits<unsigned int>::digits))
            throw TooManyBits();
    }

    BitUintv(const unsigned int s, const unsigned int v) : m_size(s), m_total(v)
    {
        if (m_size > static_cast<unsigned int>(std::numeric_limits<unsigned int>::digits))
            throw TooManyBits();
        if ((v >> (m_size - 1U)) > 1U)
            throw IntTooBig();
    }

    BitUintv& operator=(unsigned int v)
    {
        if ((v >> (m_size - 1U)) > 1U)
            throw IntTooBig();
        m_total = v;
        return *this;
    }

    [[nodiscard]] operator unsigned int() const
    {
        return m_total;
    }

    friend Bitstream& operator>>(Bitstream& bstream, BitUintv& bui)
    {
        bui.m_total = 0;
        for (unsigned int i = 0; i < bui.m_size; ++i)
        {
            if (bstream.GetBit())
                bui.m_total |= (1U << i);
        }
        return bstream;
    }

    friend Bitoggstream& operator<<(Bitoggstream& bstream, const BitUintv& bui)
    {
        for (unsigned int i = 0; i < bui.m_size; ++i)
        {
            bstream.PutBit((bui.m_total & (1U << i)) != 0);
        }
        return bstream;
    }
};

// Adapts a raw byte array into a std::streambuf for use with std::istream.
// Owns a copy of the data (the input pointer may go out of scope after construction).
class ArrayStreambuf : public std::streambuf
{
    ArrayStreambuf& operator=(const ArrayStreambuf&) = delete;
    ArrayStreambuf(const ArrayStreambuf&) = delete;
    ArrayStreambuf(ArrayStreambuf&&) = delete;
    ArrayStreambuf& operator=(ArrayStreambuf&&) = delete;

    std::vector<char> m_arr;

  public:
    ArrayStreambuf(const char* const a, const int l) : m_arr(a, a + l)
    {
        setg(m_arr.data(), m_arr.data(), m_arr.data() + m_arr.size());
    }

    ~ArrayStreambuf() override = default;
};

} // namespace ww2ogg
