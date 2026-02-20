#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "ww2ogg/bitstream.h"
#include "ww2ogg/codebook.h"
#include "ww2ogg/errors.h"
#include "ww2ogg/wwriff.h"

namespace ww2ogg
{

// Reads a Wwise audio packet header (6-byte or 2-byte variant).
// Layout for 6-byte: [size:u16][granule:u32]
// Layout for 2-byte (no_granule): [size:u16]
// After construction, Offset() points to the packet payload and Size() gives its length.
class Packet
{
    long m_offset;
    uint16_t m_size{0};
    uint32_t m_absolute_granule{0};
    bool m_no_granule;

  public:
    Packet(std::stringstream& i, const long o, const bool little_endian,
           const bool no_granule = false)
        : m_offset(o), m_no_granule(no_granule)
    {
        i.seekg(m_offset);

        if (little_endian)
        {
            m_size = Read16Le(i);
            if (!m_no_granule)
            {
                m_absolute_granule = Read32Le(i);
            }
        }
        else
        {
            m_size = Read16Be(i);
            if (!m_no_granule)
            {
                m_absolute_granule = Read32Be(i);
            }
        }
    }

    [[nodiscard]] long HeaderSize() const
    {
        return m_no_granule ? 2 : 6;
    }
    [[nodiscard]] long Offset() const
    {
        return m_offset + HeaderSize();
    }
    [[nodiscard]] uint16_t Size() const
    {
        return m_size;
    }
    [[nodiscard]] uint32_t Granule() const
    {
        return m_absolute_granule;
    }
    [[nodiscard]] long NextOffset() const
    {
        return m_offset + HeaderSize() + m_size;
    }
};

// Reads an older 8-byte Wwise audio packet header.
// Layout: [size:u32][granule:u32]
// Used by older BNK/WEM versions that set m_old_packet_headers = true.
class Packet8
{
    long m_offset;
    uint32_t m_size{0};
    uint32_t m_absolute_granule{0};

  public:
    Packet8(std::stringstream& i, const long o, const bool little_endian) : m_offset(o)
    {
        i.seekg(m_offset);

        if (little_endian)
        {
            m_size = Read32Le(i);
            m_absolute_granule = Read32Le(i);
        }
        else
        {
            m_size = Read32Be(i);
            m_absolute_granule = Read32Be(i);
        }
    }

    [[nodiscard]] long HeaderSize() const
    {
        return 8;
    }
    [[nodiscard]] long Offset() const
    {
        return m_offset + HeaderSize();
    }
    [[nodiscard]] uint32_t Size() const
    {
        return m_size;
    }
    [[nodiscard]] uint32_t Granule() const
    {
        return m_absolute_granule;
    }
    [[nodiscard]] long NextOffset() const
    {
        return m_offset + HeaderSize() + static_cast<long>(m_size);
    }
};

// Writes the standard Vorbis packet header: a 1-byte type followed by the "vorbis" magic string.
// Type 1 = identification, type 3 = comment, type 5 = setup.
class VorbisPacketHeader
{
    uint8_t m_type;

    static constexpr std::array<char, 6> g_vorbis_str = {'v', 'o', 'r', 'b', 'i', 's'};

  public:
    explicit VorbisPacketHeader(const uint8_t t) : m_type(t)
    {
    }

    friend Bitoggstream& operator<<(Bitoggstream& bstream, const VorbisPacketHeader& vph)
    {
        BitUint<8> t(vph.m_type);
        bstream << t;

        for (unsigned int i = 0; i < 6; ++i)
        {
            BitUint<8> c(static_cast<unsigned int>(g_vorbis_str.at(i)));
            bstream << c;
        }

        return bstream;
    }
};

WwiseRiffVorbis::WwiseRiffVorbis(const std::string& indata, std::string codebooks_data,
                                 const bool inline_codebooks, const bool full_setup,
                                 const ForcePacketFormat force_packet_format)
    : m_codebooks_data(std::move(codebooks_data)), m_indata(indata),
      m_inline_codebooks(inline_codebooks), m_full_setup(full_setup)
{

    m_indata.seekg(0, std::ios::end);
    m_file_size = static_cast<long>(m_indata.tellg());

    // check RIFF header
    {
        std::array<unsigned char, 4> riff_head{};
        std::array<unsigned char, 4> wave_head{};
        m_indata.seekg(0, std::ios::beg);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        m_indata.read(reinterpret_cast<char*>(riff_head.data()), 4);

        if (std::memcmp(riff_head.data(), "RIFX", 4) != 0)
        {
            if (std::memcmp(riff_head.data(), "RIFF", 4) != 0)
            {
                throw ParseErrorStr("missing RIFF");
            }
            else
            {
                m_little_endian = true;
            }
        }
        else
        {
            m_little_endian = false;
        }

        if (m_little_endian)
        {
            m_read_16 = Read16Le;
            m_read_32 = Read32Le;
        }
        else
        {
            m_read_16 = Read16Be;
            m_read_32 = Read32Be;
        }

        m_riff_size = static_cast<long>(m_read_32(m_indata)) + 8;

        if (m_riff_size > m_file_size)
        {
            throw ParseErrorStr("RIFF truncated (header claims " + std::to_string(m_riff_size) +
                                " bytes but only " + std::to_string(m_file_size) +
                                " available, this is likely a streaming/prefetch WEM"
                                " that requires the full .wem file)");
        }

        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        m_indata.read(reinterpret_cast<char*>(wave_head.data()), 4);
        if (std::memcmp(wave_head.data(), "WAVE", 4) != 0)
        {
            throw ParseErrorStr("missing WAVE");
        }
    }

    // read chunks
    long chunk_offset = 12;
    while (chunk_offset < m_riff_size)
    {
        m_indata.seekg(chunk_offset, std::ios::beg);

        if (chunk_offset + 8 > m_riff_size)
        {
            throw ParseErrorStr("chunk header truncated");
        }

        std::array<char, 4> chunk_type{};
        m_indata.read(chunk_type.data(), 4);
        const uint32_t chunk_size = m_read_32(m_indata);

        if (std::memcmp(chunk_type.data(), "fmt ", 4) == 0)
        {
            m_fmt_offset = chunk_offset + 8;
            m_fmt_size = static_cast<long>(chunk_size);
        }
        else if (std::memcmp(chunk_type.data(), "cue ", 4) == 0)
        {
            m_cue_offset = chunk_offset + 8;
            m_cue_size = static_cast<long>(chunk_size);
        }
        else if (std::memcmp(chunk_type.data(), "LIST", 4) == 0)
        {
            m_list_offset = chunk_offset + 8;
            m_list_size = static_cast<long>(chunk_size);
        }
        else if (std::memcmp(chunk_type.data(), "smpl", 4) == 0)
        {
            m_smpl_offset = chunk_offset + 8;
            m_smpl_size = static_cast<long>(chunk_size);
        }
        else if (std::memcmp(chunk_type.data(), "vorb", 4) == 0)
        {
            m_vorb_offset = chunk_offset + 8;
            m_vorb_size = static_cast<long>(chunk_size);
        }
        else if (std::memcmp(chunk_type.data(), "data", 4) == 0)
        {
            m_data_offset = chunk_offset + 8;
            m_data_size = static_cast<long>(chunk_size);
        }

        chunk_offset = chunk_offset + 8 + static_cast<long>(chunk_size);
    }

    if (chunk_offset > m_riff_size)
    {
        throw ParseErrorStr("chunk truncated");
    }

    // check that we have the chunks we're expecting
    if (m_fmt_offset == -1 && m_data_offset == -1)
    {
        throw ParseErrorStr("expected fmt, data chunks");
    }

    // read fmt
    if (m_vorb_offset == -1 && m_fmt_size != 0x42)
    {
        throw ParseErrorStr("expected 0x42 fmt if vorb missing");
    }

    if (m_vorb_offset != -1 && m_fmt_size != 0x28 && m_fmt_size != 0x18 && m_fmt_size != 0x12)
    {
        throw ParseErrorStr("bad fmt size");
    }

    if (m_vorb_offset == -1 && m_fmt_size == 0x42)
    {
        // fake it out
        m_vorb_offset = m_fmt_offset + 0x18;
    }

    m_indata.seekg(m_fmt_offset, std::ios::beg);
    if (UINT16_C(0xFFFF) != m_read_16(m_indata))
    {
        throw ParseErrorStr("bad codec id");
    }
    m_channels = m_read_16(m_indata);
    m_sample_rate = m_read_32(m_indata);
    m_avg_bytes_per_second = m_read_32(m_indata);
    if (m_read_16(m_indata) != 0U)
    {
        throw ParseErrorStr("bad block align");
    }
    if (m_read_16(m_indata) != 0U)
    {
        throw ParseErrorStr("expected 0 bps");
    }
    if (m_fmt_size - 0x12 != m_read_16(m_indata))
    {
        throw ParseErrorStr("bad extra fmt length");
    }

    if (m_fmt_size - 0x12 >= 2)
    {
        // read extra fmt
        m_ext_unk = m_read_16(m_indata);
        if (m_fmt_size - 0x12 >= 6)
        {
            m_subtype = m_read_32(m_indata);
        }
    }

    if (m_fmt_size == 0x28)
    {
        std::array<char, 16> whoknowsbuf{};
        const std::array<unsigned char, 16> whoknowsbuf_check = {
            1, 0, 0, 0, 0, 0, 0x10, 0, 0x80, 0, 0, 0xAA, 0, 0x38, 0x9b, 0x71};
        m_indata.read(whoknowsbuf.data(), 16);
        if (std::memcmp(whoknowsbuf.data(), whoknowsbuf_check.data(), 16) != 0)
        {
            throw ParseErrorStr("expected signature in extra fmt?");
        }
    }

    // read cue
    if (m_cue_offset != -1)
    {
        m_indata.seekg(m_cue_offset);
        m_cue_count = m_read_32(m_indata);
    }

    // read smpl
    if (m_smpl_offset != -1)
    {
        m_indata.seekg(m_smpl_offset + 0x1C);
        m_loop_count = m_read_32(m_indata);

        if (m_loop_count != 1)
        {
            throw ParseErrorStr("expected one loop");
        }

        m_indata.seekg(m_smpl_offset + 0x2c);
        m_loop_start = m_read_32(m_indata);
        m_loop_end = m_read_32(m_indata);
    }

    // read vorb
    switch (m_vorb_size)
    {
    case -1:
    case 0x28:
    case 0x2A:
    case 0x2C:
    case 0x32:
    case 0x34:
        m_indata.seekg(m_vorb_offset + 0x00, std::ios::beg);
        break;

    default:
        throw ParseErrorStr("bad vorb size");
    }

    m_sample_count = m_read_32(m_indata);

    switch (m_vorb_size)
    {
    case -1:
    case 0x2A: {
        m_no_granule = true;

        m_indata.seekg(m_vorb_offset + 0x4, std::ios::beg);
        const uint32_t mod_signal = m_read_32(m_indata);

        if (mod_signal != 0x4A && mod_signal != 0x4B && mod_signal != 0x69 && mod_signal != 0x70)
        {
            m_mod_packets = true;
        }
        m_indata.seekg(m_vorb_offset + 0x10, std::ios::beg);
        break;
    }

    default:
        m_indata.seekg(m_vorb_offset + 0x18, std::ios::beg);
        break;
    }

    if (force_packet_format == K_FORCE_NO_MOD_PACKETS)
    {
        m_mod_packets = false;
    }
    else if (force_packet_format == K_FORCE_MOD_PACKETS)
    {
        m_mod_packets = true;
    }

    m_setup_packet_offset = m_read_32(m_indata);
    m_first_audio_packet_offset = m_read_32(m_indata);

    // NOLINTNEXTLINE(bugprone-branch-clone)
    switch (m_vorb_size)
    {
    case -1:
    case 0x2A:
        m_indata.seekg(m_vorb_offset + 0x24, std::ios::beg);
        break;

    case 0x32:
    case 0x34:
        m_indata.seekg(m_vorb_offset + 0x2C, std::ios::beg);
        break;

    default:
        break;
    }

    switch (m_vorb_size)
    {
    case 0x28:
    case 0x2C:
        // ok to leave m_uid, m_blocksize_0_pow and m_blocksize_1_pow unset
        m_header_triad_present = true;
        m_old_packet_headers = true;
        break;

    case -1:
    case 0x2A:
    case 0x32:
    case 0x34:
        m_uid = m_read_32(m_indata);
        m_blocksize_0_pow = static_cast<uint8_t>(m_indata.get());
        m_blocksize_1_pow = static_cast<uint8_t>(m_indata.get());
        break;

    default:
        break;
    }

    // check/set loops now that we know total sample count
    if (m_loop_count != 0)
    {
        if (m_loop_end == 0)
        {
            m_loop_end = m_sample_count;
        }
        else
        {
            m_loop_end = m_loop_end + 1;
        }

        if (m_loop_start >= m_sample_count || m_loop_end > m_sample_count ||
            m_loop_start > m_loop_end)
        {
            throw ParseErrorStr("loops out of range");
        }
    }

    // check subtype now that we know the vorb info
    // this is clearly just the channel layout
    switch (m_subtype)
    {
    // NOLINTNEXTLINE(bugprone-branch-clone)
    case 4:    /* 1 channel, no seek table */
    case 3:    /* 2 channels */
    case 0x33: /* 4 channels */
    case 0x37: /* 5 channels, seek or not */
    case 0x3b: /* 5 channels, no seek table */
    case 0x3f: /* 6 channels, no seek table */
        break;
    default:
        break;
    }
}

std::string WwiseRiffVorbis::GetInfo()
{
    std::stringstream info_ss;
    if (m_little_endian)
    {
        info_ss << "RIFF WAVE";
    }
    else
    {
        info_ss << "RIFX WAVE";
    }
    info_ss << " " << m_channels << " channel";
    if (m_channels != 1)
    {
        info_ss << "s";
    }
    info_ss << " " << m_sample_rate << " Hz " << m_avg_bytes_per_second * 8 << " bps\n";
    info_ss << m_sample_count << " samples\n";

    if (m_loop_count != 0)
    {
        info_ss << "loop from " << m_loop_start << " to " << m_loop_end << "\n";
    }

    if (m_old_packet_headers)
    {
        info_ss << "- 8 byte (old) packet headers\n";
    }
    else if (m_no_granule)
    {
        info_ss << "- 2 byte packet headers, no granule\n";
    }
    else
    {
        info_ss << "- 6 byte packet headers\n";
    }

    if (m_header_triad_present)
    {
        info_ss << "- Vorbis header triad present\n";
    }

    if (m_full_setup || m_header_triad_present)
    {
        info_ss << "- full setup header\n";
    }
    else
    {
        info_ss << "- stripped setup header\n";
    }

    if (m_inline_codebooks || m_header_triad_present)
    {
        info_ss << "- inline codebooks\n";
    }

    if (m_mod_packets)
    {
        info_ss << "- modified Vorbis packets\n";
    }
    else
    {
        info_ss << "- standard Vorbis packets\n";
    }

    return info_ss.str();
}

// Reconstructs Vorbis header packets for WEMs where Wwise stripped them.
//
// This produces three OGG pages:
//   Page 1: Identification packet (channels, sample rate, block sizes)
//   Page 2: Comment packet (vendor string + optional loop tags)
//   Page 3: Setup packet (codebooks, floors, residues, mappings, modes)
//
// The setup packet is the complex part — Wwise stores codebooks as 10-bit IDs referencing
// an external packed codebook file, so we must look them up and expand them.  Floor, residue,
// mapping, and mode configurations are stored in a compact bitstream that we read and re-emit
// in standard Vorbis format.
//
// mode_blockflag and mode_bits are output parameters needed later by GenerateOgg to decode
// the first byte of "modified" audio packets.
void WwiseRiffVorbis::GenerateOggHeader(Bitoggstream& os, std::vector<bool>& mode_blockflag,
                                        int& mode_bits)
{
    // generate identification packet
    {
        VorbisPacketHeader vhead(1);

        os << vhead;

        BitUint<32> version(0);
        os << version;

        BitUint<8> ch(m_channels);
        os << ch;

        BitUint<32> srate(m_sample_rate);
        os << srate;

        BitUint<32> bitrate_max(0);
        os << bitrate_max;

        BitUint<32> bitrate_nominal(m_avg_bytes_per_second * 8);
        os << bitrate_nominal;

        BitUint<32> bitrate_minimum(0);
        os << bitrate_minimum;

        BitUint<4> blocksize_0(m_blocksize_0_pow);
        os << blocksize_0;

        BitUint<4> blocksize_1(m_blocksize_1_pow);
        os << blocksize_1;

        BitUint<1> framing(1);
        os << framing;

        // identification packet on its own page
        os.FlushPage();
    }

    // generate comment packet
    {
        VorbisPacketHeader vhead(3);

        os << vhead;

        static const std::string g_vendor =
            std::string("converted from Audiokinetic Wwise by ww2ogg ") + g_version;
        BitUint<32> vendor_size(static_cast<unsigned int>(g_vendor.size()));

        os << vendor_size;
        for (unsigned int i = 0; i < vendor_size; ++i)
        {
            BitUint<8> c(static_cast<unsigned int>(g_vendor[i]));
            os << c;
        }

        if (m_loop_count == 0)
        {
            // no user comments
            BitUint<32> user_comment_count(0);
            os << user_comment_count;
        }
        else
        {
            // two comments, loop start and end
            BitUint<32> user_comment_count(2);
            os << user_comment_count;

            std::stringstream loop_start_str;
            std::stringstream loop_end_str;

            loop_start_str << "LoopStart=" << m_loop_start;
            loop_end_str << "LoopEnd=" << m_loop_end;

            BitUint<32> loop_start_comment_length;
            loop_start_comment_length = static_cast<unsigned int>(loop_start_str.str().length());
            os << loop_start_comment_length;
            for (unsigned int i = 0; i < loop_start_comment_length; ++i)
            {
                BitUint<8> c(static_cast<unsigned int>(loop_start_str.str().c_str()[i]));
                os << c;
            }

            BitUint<32> loop_end_comment_length;
            loop_end_comment_length = static_cast<unsigned int>(loop_end_str.str().length());
            os << loop_end_comment_length;
            for (unsigned int i = 0; i < loop_end_comment_length; ++i)
            {
                BitUint<8> c(static_cast<unsigned int>(loop_end_str.str().c_str()[i]));
                os << c;
            }
        }

        BitUint<1> framing(1);
        os << framing;

        os.FlushPage();
    }

    // generate setup packet
    {
        VorbisPacketHeader vhead(5);

        os << vhead;

        Packet setup_packet(m_indata, m_data_offset + static_cast<long>(m_setup_packet_offset),
                            m_little_endian, m_no_granule);

        m_indata.seekg(setup_packet.Offset());
        if (setup_packet.Granule() != 0)
        {
            throw ParseErrorStr("setup packet granule != 0");
        }
        Bitstream ss(m_indata);

        // codebook count
        BitUint<8> codebook_count_less1;
        ss >> codebook_count_less1;
        const unsigned int codebook_count = codebook_count_less1 + 1;
        os << codebook_count_less1;

        // rebuild codebooks
        if (m_inline_codebooks)
        {
            CodebookLibrary cbl;

            for (unsigned int i = 0; i < codebook_count; ++i)
            {
                if (m_full_setup)
                {
                    cbl.Copy(ss, os);
                }
                else
                {
                    cbl.Rebuild(ss, 0, os);
                }
            }
        }
        else
        {
            /* external codebooks */
            CodebookLibrary cbl(m_codebooks_data);

            for (unsigned int i = 0; i < codebook_count; ++i)
            {
                BitUint<10> codebook_id;
                ss >> codebook_id;
                try
                {
                    cbl.Rebuild(static_cast<int>(codebook_id), os);
                }
                catch (const InvalidId& e)
                {
                    if (codebook_id == 0x342)
                    {
                        BitUint<14> codebook_identifier;
                        ss >> codebook_identifier;

                        if (codebook_identifier == 0x1590)
                        {
                            // starts with BCV, probably --full-setup
                            throw ParseErrorStr("invalid codebook id 0x342, try --full-setup");
                        }
                    }

                    // just an invalid codebook
                    throw e;
                }
            }
        }

        // Time Domain transforms (placeholder)
        BitUint<6> time_count_less1(0);
        os << time_count_less1;
        BitUint<16> dummy_time_value(0);
        os << dummy_time_value;

        if (m_full_setup)
        {
            while (ss.GetTotalBitsRead() < static_cast<unsigned long>(setup_packet.Size()) * 8u)
            {
                BitUint<1> bitly;
                ss >> bitly;
                os << bitly;
            }
        }
        else
        {
            // floor count
            BitUint<6> floor_count_less1;
            ss >> floor_count_less1;
            const unsigned int floor_count = floor_count_less1 + 1;
            os << floor_count_less1;

            // rebuild floors
            for (unsigned int i = 0; i < floor_count; ++i)
            {
                // Always floor type 1
                BitUint<16> floor_type(1);
                os << floor_type;

                BitUint<5> floor1_partitions;
                ss >> floor1_partitions;
                os << floor1_partitions;

                std::vector<unsigned int> floor1_partition_class_list(floor1_partitions);

                unsigned int maximum_class = 0;
                for (unsigned int j = 0; j < floor1_partitions; ++j)
                {
                    BitUint<4> floor1_partition_class;
                    ss >> floor1_partition_class;
                    os << floor1_partition_class;

                    floor1_partition_class_list[j] = floor1_partition_class;

                    if (floor1_partition_class > maximum_class)
                    {
                        maximum_class = floor1_partition_class;
                    }
                }

                std::vector<unsigned int> floor1_class_dimensions_list(maximum_class + 1);

                for (unsigned int j = 0; j <= maximum_class; ++j)
                {
                    BitUint<3> class_dimensions_less1;
                    ss >> class_dimensions_less1;
                    os << class_dimensions_less1;

                    floor1_class_dimensions_list[j] = class_dimensions_less1 + 1;

                    BitUint<2> class_subclasses;
                    ss >> class_subclasses;
                    os << class_subclasses;

                    if (class_subclasses != 0)
                    {
                        BitUint<8> masterbook;
                        ss >> masterbook;
                        os << masterbook;

                        if (masterbook >= codebook_count)
                        {
                            throw ParseErrorStr("invalid floor1 masterbook");
                        }
                    }

                    for (unsigned int k = 0; k < (1U << class_subclasses); ++k)
                    {
                        BitUint<8> subclass_book_plus1;
                        ss >> subclass_book_plus1;
                        os << subclass_book_plus1;

                        const int subclass_book = static_cast<int>(subclass_book_plus1) - 1;
                        if (subclass_book >= 0 &&
                            std::cmp_greater_equal(subclass_book, codebook_count))
                        {
                            throw ParseErrorStr("invalid floor1 subclass book");
                        }
                    }
                }

                BitUint<2> floor1_multiplier_less1;
                ss >> floor1_multiplier_less1;
                os << floor1_multiplier_less1;

                BitUint<4> rangebits;
                ss >> rangebits;
                os << rangebits;

                for (unsigned int j = 0; j < floor1_partitions; ++j)
                {
                    const unsigned int current_class_number = floor1_partition_class_list[j];
                    for (unsigned int k = 0; k < floor1_class_dimensions_list[current_class_number];
                         ++k)
                    {
                        BitUintv x(rangebits);
                        ss >> x;
                        os << x;
                    }
                }
            }

            // residue count
            BitUint<6> residue_count_less1;
            ss >> residue_count_less1;
            const unsigned int residue_count = residue_count_less1 + 1;
            os << residue_count_less1;

            // rebuild residues
            for (unsigned int i = 0; i < residue_count; ++i)
            {
                BitUint<2> residue_type;
                ss >> residue_type;
                os << BitUint<16>(residue_type);

                if (residue_type > 2)
                {
                    throw ParseErrorStr("invalid residue type");
                }

                BitUint<24> residue_begin;
                BitUint<24> residue_end;
                BitUint<24> residue_partition_size_less1;
                BitUint<6> residue_classifications_less1;
                BitUint<8> residue_classbook;

                ss >> residue_begin >> residue_end >> residue_partition_size_less1 >>
                    residue_classifications_less1 >> residue_classbook;
                const unsigned int residue_classifications = residue_classifications_less1 + 1;
                os << residue_begin << residue_end << residue_partition_size_less1
                   << residue_classifications_less1 << residue_classbook;

                if (residue_classbook >= codebook_count)
                {
                    throw ParseErrorStr("invalid residue classbook");
                }

                std::vector<unsigned int> residue_cascade(residue_classifications);

                for (unsigned int j = 0; j < residue_classifications; ++j)
                {
                    BitUint<5> high_bits(0);
                    BitUint<3> low_bits;

                    ss >> low_bits;
                    os << low_bits;

                    BitUint<1> bitflag;
                    ss >> bitflag;
                    os << bitflag;
                    if (bitflag)
                    {
                        ss >> high_bits;
                        os << high_bits;
                    }

                    residue_cascade[j] = high_bits * 8 + low_bits;
                }

                for (unsigned int j = 0; j < residue_classifications; ++j)
                {
                    for (unsigned int k = 0; k < 8; ++k)
                    {
                        if ((residue_cascade[j] & (1 << k)) != 0)
                        {
                            BitUint<8> residue_book;
                            ss >> residue_book;
                            os << residue_book;

                            if (residue_book >= codebook_count)
                            {
                                throw ParseErrorStr("invalid residue book");
                            }
                        }
                    }
                }
            }

            // mapping count
            BitUint<6> mapping_count_less1;
            ss >> mapping_count_less1;
            const unsigned int mapping_count = mapping_count_less1 + 1;
            os << mapping_count_less1;

            for (unsigned int i = 0; i < mapping_count; ++i)
            {
                // always mapping type 0, the only one
                BitUint<16> mapping_type(0);

                os << mapping_type;

                BitUint<1> submaps_flag;
                ss >> submaps_flag;
                os << submaps_flag;

                unsigned int submaps = 1;
                if (submaps_flag)
                {
                    BitUint<4> submaps_less1;

                    ss >> submaps_less1;
                    submaps = submaps_less1 + 1;
                    os << submaps_less1;
                }

                BitUint<1> square_polar_flag;
                ss >> square_polar_flag;
                os << square_polar_flag;

                if (square_polar_flag)
                {
                    BitUint<8> coupling_steps_less1;
                    ss >> coupling_steps_less1;
                    const unsigned int coupling_steps = coupling_steps_less1 + 1;
                    os << coupling_steps_less1;

                    for (unsigned int j = 0; j < coupling_steps; ++j)
                    {
                        BitUintv magnitude(Ilog(m_channels - 1));
                        BitUintv angle(Ilog(m_channels - 1));

                        ss >> magnitude >> angle;
                        os << magnitude << angle;

                        if (angle == magnitude || magnitude >= m_channels || angle >= m_channels)
                        {
                            throw ParseErrorStr("invalid coupling");
                        }
                    }
                }

                // a rare reserved field not removed by Ak!
                BitUint<2> mapping_reserved;
                ss >> mapping_reserved;
                os << mapping_reserved;
                if (mapping_reserved != 0)
                {
                    throw ParseErrorStr("mapping reserved field nonzero");
                }

                if (submaps > 1)
                {
                    for (unsigned int j = 0; j < m_channels; ++j)
                    {
                        BitUint<4> mapping_mux;
                        ss >> mapping_mux;
                        os << mapping_mux;

                        if (mapping_mux >= submaps)
                        {
                            throw ParseErrorStr("mapping_mux >= submaps");
                        }
                    }
                }

                for (unsigned int j = 0; j < submaps; ++j)
                {
                    // Another! Unused time domain transform configuration placeholder!
                    BitUint<8> time_config;
                    ss >> time_config;
                    os << time_config;

                    BitUint<8> floor_number;
                    ss >> floor_number;
                    os << floor_number;
                    if (floor_number >= floor_count)
                    {
                        throw ParseErrorStr("invalid floor mapping");
                    }

                    BitUint<8> residue_number;
                    ss >> residue_number;
                    os << residue_number;
                    if (residue_number >= residue_count)
                    {
                        throw ParseErrorStr("invalid residue mapping");
                    }
                }
            }

            // mode count
            BitUint<6> mode_count_less1;
            ss >> mode_count_less1;
            const unsigned int mode_count = mode_count_less1 + 1;
            os << mode_count_less1;

            mode_blockflag.resize(mode_count);
            mode_bits = Ilog(mode_count - 1);

            for (unsigned int i = 0; i < mode_count; ++i)
            {
                BitUint<1> block_flag;
                ss >> block_flag;
                os << block_flag;

                mode_blockflag[i] = (block_flag != 0);

                // only 0 valid for windowtype and transformtype
                BitUint<16> windowtype(0);
                BitUint<16> transformtype(0);
                os << windowtype << transformtype;

                BitUint<8> mapping;
                ss >> mapping;
                os << mapping;
                if (mapping >= mapping_count)
                {
                    throw ParseErrorStr("invalid mode mapping");
                }
            }

            BitUint<1> framing(1);
            os << framing;
        }

        os.FlushPage();

        if ((ss.GetTotalBitsRead() + 7) / 8 != setup_packet.Size())
        {
            throw ParseErrorStr("didn't read exactly setup packet");
        }

        if (setup_packet.NextOffset() !=
            m_data_offset + static_cast<long>(m_first_audio_packet_offset))
        {
            throw ParseErrorStr("first audio packet doesn't follow setup packet");
        }
    }
}

// Main entry point: writes a complete OGG Vorbis stream.
// First emits the header pages (via triad path or reconstruction), then iterates over
// all audio packets in the data chunk, translating Wwise framing to OGG pages.
//
// For "modified" packets (m_mod_packets), the first byte of each audio packet needs
// reconstruction: Wwise strips the packet-type bit and window-type bits, so we read
// the mode number, determine block flags, peek at the next packet's mode to figure out
// the next-window type, and re-emit the correct Vorbis first byte.
void WwiseRiffVorbis::GenerateOgg(std::ostream& oss)
{
    Bitoggstream os(oss);

    std::vector<bool> mode_blockflag;
    int mode_bits = 0;
    bool prev_blockflag = false;

    if (m_header_triad_present)
    {
        GenerateOggHeaderWithTriad(os);
    }
    else
    {
        GenerateOggHeader(os, mode_blockflag, mode_bits);
    }

    // Audio pages
    {
        long offset = m_data_offset + static_cast<long>(m_first_audio_packet_offset);

        while (offset < m_data_offset + m_data_size)
        {
            uint32_t size = 0;
            uint32_t granule = 0;
            long packet_header_size = 0;
            long packet_payload_offset = 0;
            long next_offset = 0;

            if (m_old_packet_headers)
            {
                Packet8 audio_packet(m_indata, offset, m_little_endian);
                packet_header_size = audio_packet.HeaderSize();
                size = audio_packet.Size();
                packet_payload_offset = audio_packet.Offset();
                granule = audio_packet.Granule();
                next_offset = audio_packet.NextOffset();
            }
            else
            {
                Packet audio_packet(m_indata, offset, m_little_endian, m_no_granule);
                packet_header_size = audio_packet.HeaderSize();
                size = audio_packet.Size();
                packet_payload_offset = audio_packet.Offset();
                granule = audio_packet.Granule();
                next_offset = audio_packet.NextOffset();
            }

            if (offset + packet_header_size > m_data_offset + m_data_size)
            {
                throw ParseErrorStr("page header truncated");
            }

            offset = packet_payload_offset;

            m_indata.seekg(offset);
            // HACK: don't know what to do here
            if (granule == UINT32_C(0xFFFFFFFF))
            {
                os.SetGranule(1);
            }
            else
            {
                os.SetGranule(granule);
            }

            // first byte
            if (m_mod_packets)
            {
                // need to rebuild packet type and window info

                if (mode_blockflag.empty())
                {
                    throw ParseErrorStr("didn't load mode_blockflag");
                }

                // OUT: 1 bit packet type (0 == audio)
                BitUint<1> packet_type(0);
                os << packet_type;

                BitUintv mode_number(mode_bits);
                BitUintv remainder(8 - mode_bits);

                {
                    // collect mode number from first byte
                    Bitstream ss(m_indata);

                    // IN/OUT: N bit mode number (max 6 bits)
                    ss >> mode_number;
                    os << mode_number;

                    // IN: remaining bits of first (input) byte
                    ss >> remainder;
                }

                if (mode_blockflag[mode_number])
                {
                    // long window, peek at next frame

                    m_indata.seekg(next_offset);
                    bool next_blockflag = false;
                    if (next_offset + packet_header_size <= m_data_offset + m_data_size)
                    {

                        // mod_packets always goes with 6-byte headers
                        Packet audio_packet(m_indata, next_offset, m_little_endian, m_no_granule);
                        const uint32_t next_packet_size = audio_packet.Size();
                        if (next_packet_size > 0)
                        {
                            m_indata.seekg(audio_packet.Offset());

                            Bitstream ss(m_indata);
                            BitUintv next_mode_number(mode_bits);

                            ss >> next_mode_number;

                            next_blockflag = mode_blockflag[next_mode_number];
                        }
                    }

                    // OUT: previous window type bit
                    BitUint<1> prev_window_type(prev_blockflag ? 1 : 0);
                    os << prev_window_type;

                    // OUT: next window type bit
                    BitUint<1> next_window_type(next_blockflag ? 1 : 0);
                    os << next_window_type;

                    // fix seek for rest of stream
                    m_indata.seekg(offset + 1);
                }

                prev_blockflag = mode_blockflag[mode_number];

                // OUT: remaining bits of first (input) byte
                os << remainder;
            }
            else
            {
                // nothing unusual for first byte
                int v = m_indata.get();
                if (v < 0)
                {
                    throw ParseErrorStr("file truncated");
                }
                BitUint<8> c(static_cast<unsigned int>(v));
                os << c;
            }

            // remainder of packet
            for (unsigned int i = 1; i < size; ++i)
            {
                int v = m_indata.get();
                if (v < 0)
                {
                    throw ParseErrorStr("file truncated");
                }
                BitUint<8> c(static_cast<unsigned int>(v));
                os << c;
            }

            offset = next_offset;
            os.FlushPage(false, (offset == m_data_offset + m_data_size));
        }
        if (offset > m_data_offset + m_data_size)
        {
            throw ParseErrorStr("page truncated");
        }
    }

    mode_blockflag.clear();
}

// Copies the Vorbis header triad verbatim from older WEM files that already include
// the full identification/comment/setup packets (wrapped in 8-byte Wwise packet headers).
// Codebooks are copied as-is since they're already in standard Vorbis format.
void WwiseRiffVorbis::GenerateOggHeaderWithTriad(Bitoggstream& os)
{
    // Header page triad
    {
        long offset = m_data_offset + static_cast<long>(m_setup_packet_offset);

        // copy information packet
        {
            Packet8 information_packet(m_indata, offset, m_little_endian);
            const uint32_t size = information_packet.Size();

            if (information_packet.Granule() != 0)
            {
                throw ParseErrorStr("information packet granule != 0");
            }

            m_indata.seekg(information_packet.Offset());

            BitUint<8> c(static_cast<unsigned int>(m_indata.get()));
            if (c != 1)
            {
                throw ParseErrorStr("wrong type for information packet");
            }

            os << c;

            for (unsigned int i = 1; i < size; ++i)
            {
                c = static_cast<unsigned int>(m_indata.get());
                os << c;
            }

            // identification packet on its own page
            os.FlushPage();

            offset = information_packet.NextOffset();
        }

        // copy comment packet
        {
            Packet8 comment_packet(m_indata, offset, m_little_endian);
            const auto size = static_cast<uint16_t>(comment_packet.Size());

            if (comment_packet.Granule() != 0)
            {
                throw ParseErrorStr("comment packet granule != 0");
            }

            m_indata.seekg(comment_packet.Offset());

            BitUint<8> c(static_cast<unsigned int>(m_indata.get()));
            if (c != 3)
            {
                throw ParseErrorStr("wrong type for comment packet");
            }

            os << c;

            for (unsigned int i = 1; i < size; ++i)
            {
                c = static_cast<unsigned int>(m_indata.get());
                os << c;
            }

            // identification packet on its own page
            os.FlushPage();

            offset = comment_packet.NextOffset();
        }

        // copy setup packet
        {
            Packet8 setup_packet(m_indata, offset, m_little_endian);

            m_indata.seekg(setup_packet.Offset());
            if (setup_packet.Granule() != 0)
            {
                throw ParseErrorStr("setup packet granule != 0");
            }
            Bitstream ss(m_indata);

            BitUint<8> c;
            ss >> c;

            // type
            if (c != 5)
            {
                throw ParseErrorStr("wrong type for setup packet");
            }
            os << c;

            // 'vorbis'
            for (unsigned int i = 0; i < 6; ++i)
            {
                ss >> c;
                os << c;
            }

            // codebook count
            BitUint<8> codebook_count_less1;
            ss >> codebook_count_less1;
            const unsigned int codebook_count = codebook_count_less1 + 1;
            os << codebook_count_less1;

            CodebookLibrary cbl;

            // rebuild codebooks
            for (unsigned int i = 0; i < codebook_count; ++i)
            {
                cbl.Copy(ss, os);
            }

            while (ss.GetTotalBitsRead() < static_cast<unsigned long>(setup_packet.Size()) * 8u)
            {
                BitUint<1> bitly;
                ss >> bitly;
                os << bitly;
            }

            os.FlushPage();

            offset = setup_packet.NextOffset();
        }

        if (offset != m_data_offset + static_cast<long>(m_first_audio_packet_offset))
        {
            throw ParseErrorStr("first audio packet doesn't follow setup packet");
        }
    }
}

} // namespace ww2ogg
