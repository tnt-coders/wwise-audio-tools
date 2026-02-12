/**
 * @file codebook.cpp
 * @brief Vorbis codebook library implementation
 * @note Modernized to C++23
 */

#define __STDC_CONSTANT_MACROS
#include <sstream>
#include <string>

#include "ww2ogg/codebook.h"

namespace ww2ogg
{

codebook_library::codebook_library() = default;

codebook_library::codebook_library(const std::string& indata)
{
    std::stringstream is(indata);

    is.seekg(0, std::ios::end);
    const auto file_size = static_cast<long>(is.tellg());

    is.seekg(file_size - 4, std::ios::beg);
    const auto offset_offset = static_cast<long>(read_32_le(is));
    const auto codebook_count = (file_size - offset_offset) / 4;

    codebook_data.resize(offset_offset);
    codebook_offsets.resize(codebook_count);

    is.seekg(0, std::ios::beg);
    for (long i = 0; i < offset_offset; ++i)
    {
        codebook_data[i] = static_cast<char>(is.get());
    }

    for (long i = 0; i < codebook_count; ++i)
    {
        codebook_offsets[i] = static_cast<long>(read_32_le(is));
    }
}

void codebook_library::rebuild(const int i, bitoggstream& bos)
{
    const char* cb = get_codebook(i);
    unsigned long cb_size = 0;

    {
        const long signed_cb_size = get_codebook_size(i);

        if (cb == nullptr || signed_cb_size == -1)
        {
            throw invalid_id(i);
        }

        cb_size = static_cast<unsigned long>(signed_cb_size);
    }

    array_streambuf asb(cb, static_cast<int>(cb_size));
    std::istream is(&asb);
    bitstream bis(is);

    rebuild(bis, cb_size, bos);
}

/* cb_size == 0 to not check size (for an inline bitstream) */
void codebook_library::copy(bitstream& bis, bitoggstream& bos)
{
    /* IN: 24 bit identifier, 16 bit dimensions, 24 bit entry count */

    Bit_uint<24> id;
    Bit_uint<16> dimensions;
    Bit_uint<24> entries;

    bis >> id >> dimensions >> entries;

    if (0x564342 != id)
    {
        throw parse_error_str("invalid codebook identifier");
    }

    /* OUT: 24 bit identifier, 16 bit dimensions, 24 bit entry count */
    bos << id << Bit_uint<16>(dimensions) << Bit_uint<24>(entries);

    // gather codeword lengths

    /* IN/OUT: 1 bit ordered flag */
    Bit_uint<1> ordered;
    bis >> ordered;
    bos << ordered;
    if (ordered)
    {
        /* IN/OUT: 5 bit initial length */
        Bit_uint<5> initial_length;
        bis >> initial_length;
        bos << initial_length;

        unsigned int current_entry = 0;
        while (current_entry < entries)
        {
            /* IN/OUT: ilog(entries-current_entry) bit count w/ given length */
            Bit_uintv number(ilog(entries - current_entry));
            bis >> number;
            bos << number;
            current_entry += number;
        }
        if (current_entry > entries)
        {
            throw parse_error_str("current_entry out of range");
        }
    }
    else
    {
        /* IN/OUT: 1 bit sparse flag */
        Bit_uint<1> sparse;
        bis >> sparse;
        bos << sparse;

        for (unsigned int i = 0; i < entries; ++i)
        {
            bool present_bool = true;

            if (sparse)
            {
                /* IN/OUT 1 bit sparse presence flag */
                Bit_uint<1> present;
                bis >> present;
                bos << present;

                present_bool = (present != 0);
            }

            if (present_bool)
            {
                /* IN/OUT: 5 bit codeword length-1 */
                Bit_uint<5> codeword_length;
                bis >> codeword_length;
                bos << codeword_length;
            }
        }
    } // done with lengths

    // lookup table

    /* IN/OUT: 4 bit lookup type */
    Bit_uint<4> lookup_type;
    bis >> lookup_type;
    bos << lookup_type;

    if (lookup_type == 0)
    {
        // no lookup table
    }
    else if (lookup_type == 1)
    {
        /* IN/OUT: 32 bit minimum length, 32 bit maximum length, 4 bit value
         * length-1, 1 bit sequence flag */
        Bit_uint<32> min;
        Bit_uint<32> max;
        Bit_uint<4> value_length;
        Bit_uint<1> sequence_flag;
        bis >> min >> max >> value_length >> sequence_flag;
        bos << min << max << value_length << sequence_flag;

        const unsigned int quantvals = _book_maptype1_quantvals(entries, dimensions);
        for (unsigned int i = 0; i < quantvals; ++i)
        {
            /* IN/OUT: n bit value */
            Bit_uintv val(value_length + 1);
            bis >> val;
            bos << val;
        }
    }
    else if (lookup_type == 2)
    {
        throw parse_error_str("didn't expect lookup type 2");
    }
    else
    {
        throw parse_error_str("invalid lookup type");
    }
}

/* cb_size == 0 to not check size (for an inline bitstream) */
void codebook_library::rebuild(bitstream& bis, const unsigned long cb_size, bitoggstream& bos)
{
    /* IN: 4 bit dimensions, 14 bit entry count */

    Bit_uint<4> dimensions;
    Bit_uint<14> entries;

    bis >> dimensions >> entries;

    /* OUT: 24 bit identifier, 16 bit dimensions, 24 bit entry count */
    bos << Bit_uint<24>(0x564342) << Bit_uint<16>(dimensions) << Bit_uint<24>(entries);

    // gather codeword lengths

    /* IN/OUT: 1 bit ordered flag */
    Bit_uint<1> ordered;
    bis >> ordered;
    bos << ordered;
    if (ordered)
    {
        /* IN/OUT: 5 bit initial length */
        Bit_uint<5> initial_length;
        bis >> initial_length;
        bos << initial_length;

        unsigned int current_entry = 0;
        while (current_entry < entries)
        {
            /* IN/OUT: ilog(entries-current_entry) bit count w/ given length */
            Bit_uintv number(ilog(entries - current_entry));
            bis >> number;
            bos << number;
            current_entry += number;
        }
        if (current_entry > entries)
        {
            throw parse_error_str("current_entry out of range");
        }
    }
    else
    {
        /* IN: 3 bit codeword length length, 1 bit sparse flag */
        Bit_uint<3> codeword_length_length;
        Bit_uint<1> sparse;
        bis >> codeword_length_length >> sparse;

        if (codeword_length_length == 0 || codeword_length_length > 5)
        {
            throw parse_error_str("nonsense codeword length");
        }

        /* OUT: 1 bit sparse flag */
        bos << sparse;

        for (unsigned int i = 0; i < entries; ++i)
        {
            bool present_bool = true;

            if (sparse)
            {
                /* IN/OUT 1 bit sparse presence flag */
                Bit_uint<1> present;
                bis >> present;
                bos << present;

                present_bool = (present != 0);
            }

            if (present_bool)
            {
                /* IN: n bit codeword length-1 */
                Bit_uintv codeword_length(codeword_length_length);
                bis >> codeword_length;

                /* OUT: 5 bit codeword length-1 */
                bos << Bit_uint<5>(codeword_length);
            }
        }
    } // done with lengths

    // lookup table

    /* IN: 1 bit lookup type */
    Bit_uint<1> lookup_type;
    bis >> lookup_type;
    /* OUT: 4 bit lookup type */
    bos << Bit_uint<4>(lookup_type);

    if (lookup_type == 0)
    {
        // no lookup table
    }
    else if (lookup_type == 1)
    {
        /* IN/OUT: 32 bit minimum length, 32 bit maximum length, 4 bit value
         * length-1, 1 bit sequence flag */
        Bit_uint<32> min;
        Bit_uint<32> max;
        Bit_uint<4> value_length;
        Bit_uint<1> sequence_flag;
        bis >> min >> max >> value_length >> sequence_flag;
        bos << min << max << value_length << sequence_flag;

        const unsigned int quantvals = _book_maptype1_quantvals(entries, dimensions);
        for (unsigned int i = 0; i < quantvals; ++i)
        {
            /* IN/OUT: n bit value */
            Bit_uintv val(value_length + 1);
            bis >> val;
            bos << val;
        }
    }
    else if (lookup_type == 2)
    {
        throw parse_error_str("didn't expect lookup type 2");
    }
    else
    {
        throw parse_error_str("invalid lookup type");
    }

    /* check that we used exactly all bytes */
    /* note: if all bits are used in the last byte there will be one extra 0 byte
     */
    if (cb_size != 0 && bis.get_total_bits_read() / 8 + 1 != cb_size)
    {
        throw size_mismatch(cb_size, bis.get_total_bits_read() / 8 + 1);
    }
}

} // namespace ww2ogg
