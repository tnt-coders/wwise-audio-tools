/**
 * @file codebook.cpp
 * @brief Vorbis codebook library implementation
 * @note Modernized to C++23
 */

#include <sstream>
#include <string>

#include "ww2ogg/codebook.h"

namespace ww2ogg
{

CodebookLibrary::CodebookLibrary() = default;

CodebookLibrary::CodebookLibrary(const std::string& indata)
{
    std::stringstream is(indata);

    is.seekg(0, std::ios::end);
    const auto file_size = static_cast<long>(is.tellg());

    is.seekg(file_size - 4, std::ios::beg);
    const auto offset_offset = static_cast<long>(Read32Le(is));
    const auto codebook_count = (file_size - offset_offset) / 4;

    m_codebook_data.resize(offset_offset);
    m_codebook_offsets.resize(codebook_count);

    is.seekg(0, std::ios::beg);
    for (long i = 0; i < offset_offset; ++i)
    {
        m_codebook_data[i] = static_cast<char>(is.get());
    }

    for (long i = 0; i < codebook_count; ++i)
    {
        m_codebook_offsets[i] = static_cast<long>(Read32Le(is));
    }
}

void CodebookLibrary::Rebuild(const int i, Bitoggstream& bos)
{
    const char* cb = GetCodebook(i);
    unsigned long cb_size = 0;

    {
        const long signed_cb_size = GetCodebookSize(i);

        if (cb == nullptr || signed_cb_size == -1)
        {
            throw InvalidId(i);
        }

        cb_size = static_cast<unsigned long>(signed_cb_size);
    }

    ArrayStreambuf asb(cb, static_cast<int>(cb_size));
    std::istream is(&asb);
    Bitstream bis{is};

    Rebuild(bis, cb_size, bos);
}

/* cb_size == 0 to not check size (for an inline bitstream) */
void CodebookLibrary::Copy(Bitstream& bis, Bitoggstream& bos)
{
    /* IN: 24 bit identifier, 16 bit dimensions, 24 bit entry count */

    BitUint<24> id;
    BitUint<16> dimensions;
    BitUint<24> entries;

    bis >> id >> dimensions >> entries;

    if (0x564342 != id)
    {
        throw ParseErrorStr("invalid codebook identifier");
    }

    /* OUT: 24 bit identifier, 16 bit dimensions, 24 bit entry count */
    bos << id << BitUint<16>(dimensions) << BitUint<24>(entries);

    // gather codeword lengths

    /* IN/OUT: 1 bit ordered flag */
    BitUint<1> ordered;
    bis >> ordered;
    bos << ordered;
    if (ordered)
    {
        /* IN/OUT: 5 bit initial length */
        BitUint<5> initial_length;
        bis >> initial_length;
        bos << initial_length;

        unsigned int current_entry = 0;
        while (current_entry < entries)
        {
            /* IN/OUT: ilog(entries-current_entry) bit count w/ given length */
            BitUintv number(Ilog(entries - current_entry));
            bis >> number;
            bos << number;
            current_entry += number;
        }
        if (current_entry > entries)
        {
            throw ParseErrorStr("current_entry out of range");
        }
    }
    else
    {
        /* IN/OUT: 1 bit sparse flag */
        BitUint<1> sparse;
        bis >> sparse;
        bos << sparse;

        for (unsigned int i = 0; i < entries; ++i)
        {
            bool present_bool = true;

            if (sparse)
            {
                /* IN/OUT 1 bit sparse presence flag */
                BitUint<1> present;
                bis >> present;
                bos << present;

                present_bool = (present != 0);
            }

            if (present_bool)
            {
                /* IN/OUT: 5 bit codeword length-1 */
                BitUint<5> codeword_length;
                bis >> codeword_length;
                bos << codeword_length;
            }
        }
    } // done with lengths

    // lookup table

    /* IN/OUT: 4 bit lookup type */
    BitUint<4> lookup_type;
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
        BitUint<32> min;
        BitUint<32> max;
        BitUint<4> value_length;
        BitUint<1> sequence_flag;
        bis >> min >> max >> value_length >> sequence_flag;
        bos << min << max << value_length << sequence_flag;

        const unsigned int quantvals = BookMaptype1Quantvals(entries, dimensions);
        for (unsigned int i = 0; i < quantvals; ++i)
        {
            /* IN/OUT: n bit value */
            BitUintv val(value_length + 1);
            bis >> val;
            bos << val;
        }
    }
    else if (lookup_type == 2)
    {
        throw ParseErrorStr("didn't expect lookup type 2");
    }
    else
    {
        throw ParseErrorStr("invalid lookup type");
    }
}

/* cb_size == 0 to not check size (for an inline bitstream) */
void CodebookLibrary::Rebuild(Bitstream& bis, const unsigned long cb_size, Bitoggstream& bos)
{
    /* IN: 4 bit dimensions, 14 bit entry count */

    BitUint<4> dimensions;
    BitUint<14> entries;

    bis >> dimensions >> entries;

    /* OUT: 24 bit identifier, 16 bit dimensions, 24 bit entry count */
    bos << BitUint<24>(0x564342) << BitUint<16>(dimensions) << BitUint<24>(entries);

    // gather codeword lengths

    /* IN/OUT: 1 bit ordered flag */
    BitUint<1> ordered;
    bis >> ordered;
    bos << ordered;
    if (ordered)
    {
        /* IN/OUT: 5 bit initial length */
        BitUint<5> initial_length;
        bis >> initial_length;
        bos << initial_length;

        unsigned int current_entry = 0;
        while (current_entry < entries)
        {
            /* IN/OUT: ilog(entries-current_entry) bit count w/ given length */
            BitUintv number(Ilog(entries - current_entry));
            bis >> number;
            bos << number;
            current_entry += number;
        }
        if (current_entry > entries)
        {
            throw ParseErrorStr("current_entry out of range");
        }
    }
    else
    {
        /* IN: 3 bit codeword length length, 1 bit sparse flag */
        BitUint<3> codeword_length_length;
        BitUint<1> sparse;
        bis >> codeword_length_length >> sparse;

        if (codeword_length_length == 0 || codeword_length_length > 5)
        {
            throw ParseErrorStr("nonsense codeword length");
        }

        /* OUT: 1 bit sparse flag */
        bos << sparse;

        for (unsigned int i = 0; i < entries; ++i)
        {
            bool present_bool = true;

            if (sparse)
            {
                /* IN/OUT 1 bit sparse presence flag */
                BitUint<1> present;
                bis >> present;
                bos << present;

                present_bool = (present != 0);
            }

            if (present_bool)
            {
                /* IN: n bit codeword length-1 */
                BitUintv codeword_length(codeword_length_length);
                bis >> codeword_length;

                /* OUT: 5 bit codeword length-1 */
                bos << BitUint<5>(codeword_length);
            }
        }
    } // done with lengths

    // lookup table

    /* IN: 1 bit lookup type */
    BitUint<1> lookup_type;
    bis >> lookup_type;
    /* OUT: 4 bit lookup type */
    bos << BitUint<4>(lookup_type);

    if (lookup_type == 0)
    {
        // no lookup table
    }
    else if (lookup_type == 1)
    {
        /* IN/OUT: 32 bit minimum length, 32 bit maximum length, 4 bit value
         * length-1, 1 bit sequence flag */
        BitUint<32> min;
        BitUint<32> max;
        BitUint<4> value_length;
        BitUint<1> sequence_flag;
        bis >> min >> max >> value_length >> sequence_flag;
        bos << min << max << value_length << sequence_flag;

        const unsigned int quantvals = BookMaptype1Quantvals(entries, dimensions);
        for (unsigned int i = 0; i < quantvals; ++i)
        {
            /* IN/OUT: n bit value */
            BitUintv val(value_length + 1);
            bis >> val;
            bos << val;
        }
    }
    else if (lookup_type == 2)
    {
        throw ParseErrorStr("didn't expect lookup type 2");
    }
    else
    {
        throw ParseErrorStr("invalid lookup type");
    }

    /* check that we used exactly all bytes */
    /* note: if all bits are used in the last byte there will be one extra 0 byte
     */
    if (cb_size != 0 && bis.GetTotalBitsRead() / 8 + 1 != cb_size)
    {
        throw SizeMismatch(cb_size, bis.GetTotalBitsRead() / 8 + 1);
    }
}

} // namespace ww2ogg
