#pragma once

/**
 * @file wwriff.h
 * @brief Wwise RIFF Vorbis file parser
 * @note Modernized to C++23
 */

#include <cstdint>
#include <istream>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

#include "bitstream.h"

namespace ww2ogg
{

inline constexpr const char* VERSION = "0.24";

/**
 * @brief Force specific packet format during conversion
 */
enum ForcePacketFormat
{
    kNoForcePacketFormat,
    kForceModPackets,
    kForceNoModPackets
};

/**
 * @brief Parser and converter for Wwise RIFF Vorbis files
 */
class Wwise_RIFF_Vorbis
{
    std::string _codebooks_data;
    std::stringstream _indata;
    long _file_size = -1;

    bool _little_endian = true;

    long _riff_size = -1;
    long _fmt_offset = -1;
    long _cue_offset = -1;
    long _LIST_offset = -1;
    long _smpl_offset = -1;
    long _vorb_offset = -1;
    long _data_offset = -1;
    long _fmt_size = -1;
    long _cue_size = -1;
    long _LIST_size = -1;
    long _smpl_size = -1;
    long _vorb_size = -1;
    long _data_size = -1;

    // RIFF fmt
    uint16_t _channels = 0;
    uint32_t _sample_rate = 0;
    uint32_t _avg_bytes_per_second = 0;

    // RIFF extended fmt
    uint16_t _ext_unk = 0;
    uint32_t _subtype = 0;

    // cue info
    uint32_t _cue_count = 0;

    // smpl info
    uint32_t _loop_count = 0;
    uint32_t _loop_start = 0;
    uint32_t _loop_end = 0;

    // vorbis info
    uint32_t _sample_count = 0;
    uint32_t _setup_packet_offset = 0;
    uint32_t _first_audio_packet_offset = 0;
    uint32_t _uid = 0;
    uint8_t _blocksize_0_pow = 0;
    uint8_t _blocksize_1_pow = 0;

    const bool _inline_codebooks;
    const bool _full_setup;
    bool _header_triad_present = false;
    bool _old_packet_headers = false;
    bool _no_granule = false;
    bool _mod_packets = false;

    uint16_t (*_read_16)(std::istream& is) = nullptr;
    uint32_t (*_read_32)(std::istream& is) = nullptr;

  public:
    Wwise_RIFF_Vorbis(const std::string& indata, std::string codebooks_data, bool inline_codebooks,
                      bool full_setup, ForcePacketFormat force_packet_format);

    [[nodiscard]] std::string get_info();

    void generate_ogg(std::ostream& os);
    void generate_ogg_header(bitoggstream& os, std::vector<bool>& mode_blockflag, int& mode_bits);
    void generate_ogg_header_with_triad(bitoggstream& os);
};

} // namespace ww2ogg
