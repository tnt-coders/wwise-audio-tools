#pragma once

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

inline constexpr const char* g_version = "0.24";

enum ForcePacketFormat
{
    K_NO_FORCE_PACKET_FORMAT,
    K_FORCE_MOD_PACKETS,
    K_FORCE_NO_MOD_PACKETS
};

class WwiseRiffVorbis
{
    std::string m_codebooks_data;
    std::stringstream m_indata;
    long m_file_size = -1;

    bool m_little_endian = true;

    long m_riff_size = -1;
    long m_fmt_offset = -1;
    long m_cue_offset = -1;
    long m_list_offset = -1;
    long m_smpl_offset = -1;
    long m_vorb_offset = -1;
    long m_data_offset = -1;
    long m_fmt_size = -1;
    long m_cue_size = -1;
    long m_list_size = -1;
    long m_smpl_size = -1;
    long m_vorb_size = -1;
    long m_data_size = -1;

    // RIFF fmt
    uint16_t m_channels = 0;
    uint32_t m_sample_rate = 0;
    uint32_t m_avg_bytes_per_second = 0;

    // RIFF extended fmt
    uint16_t m_ext_unk = 0;
    uint32_t m_subtype = 0;

    // cue info
    uint32_t m_cue_count = 0;

    // smpl info
    uint32_t m_loop_count = 0;
    uint32_t m_loop_start = 0;
    uint32_t m_loop_end = 0;

    // vorbis info
    uint32_t m_sample_count = 0;
    uint32_t m_setup_packet_offset = 0;
    uint32_t m_first_audio_packet_offset = 0;
    uint32_t m_uid = 0;
    uint8_t m_blocksize_0_pow = 0;
    uint8_t m_blocksize_1_pow = 0;

    const bool m_inline_codebooks;
    const bool m_full_setup;
    bool m_header_triad_present = false;
    bool m_old_packet_headers = false;
    bool m_no_granule = false;
    bool m_mod_packets = false;

    uint16_t (*m_read_16)(std::istream& is) = nullptr;
    uint32_t (*m_read_32)(std::istream& is) = nullptr;

  public:
    WwiseRiffVorbis(const std::string& indata, std::string codebooks_data, bool inline_codebooks,
                    bool full_setup, ForcePacketFormat force_packet_format);

    [[nodiscard]] std::string GetInfo();

    void GenerateOgg(std::ostream& os);
    void GenerateOggHeader(Bitoggstream& os, std::vector<bool>& mode_blockflag, int& mode_bits);
    void GenerateOggHeaderWithTriad(Bitoggstream& os);
};

} // namespace ww2ogg
