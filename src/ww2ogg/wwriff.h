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

// Controls whether the converter forces Wwise "modified" or "standard" Vorbis packet format.
// Modified packets omit the packet-type bit and window flags, requiring reconstruction.
enum ForcePacketFormat
{
    K_NO_FORCE_PACKET_FORMAT, // auto-detect from vorb chunk metadata
    K_FORCE_MOD_PACKETS,      // force modified Vorbis packets
    K_FORCE_NO_MOD_PACKETS    // force standard Vorbis packets
};

// Parses a Wwise RIFF/RIFX Vorbis WEM file and reconstructs a valid OGG Vorbis stream.
//
// Wwise strips parts of the Vorbis setup (codebooks, floor/residue configs) and uses its
// own packet framing instead of standard OGG pages.  This class reverses those changes:
//   1. Constructor: parses the RIFF structure and extracts chunk offsets/metadata
//   2. GenerateOggHeader (or GenerateOggHeaderWithTriad): rebuilds the three Vorbis
//      header packets (identification, comment, setup) into proper OGG pages
//   3. GenerateOgg: copies audio packets, reconstructing packet-type/window bits for
//      modified packets and wrapping everything in OGG pages with correct granule positions
class WwiseRiffVorbis
{
    std::string m_codebooks_data; // external packed codebook data (or empty if inline)
    std::stringstream m_indata;   // in-memory copy of the WEM file
    long m_file_size = -1;

    bool m_little_endian = true; // RIFF = LE, RIFX = BE

    // Chunk offsets and sizes within the RIFF container (-1 = not present).
    // These point past the 8-byte chunk header (type + size), directly to chunk data.
    long m_riff_size = -1;
    long m_fmt_offset = -1;  // "fmt " chunk: audio format metadata
    long m_cue_offset = -1;  // "cue " chunk: cue point markers
    long m_list_offset = -1; // "LIST" chunk: metadata list (unused by converter)
    long m_smpl_offset = -1; // "smpl" chunk: sampler/loop info
    long m_vorb_offset = -1; // "vorb" chunk: Wwise-specific Vorbis setup
    long m_data_offset = -1; // "data" chunk: audio packet payload
    long m_fmt_size = -1;
    long m_cue_size = -1;
    long m_list_size = -1;
    long m_smpl_size = -1;
    long m_vorb_size = -1;
    long m_data_size = -1;

    // Fields from RIFF "fmt " chunk
    uint16_t m_channels = 0;
    uint32_t m_sample_rate = 0;
    uint32_t m_avg_bytes_per_second = 0;

    // Extended fmt fields
    uint16_t m_ext_unk = 0;
    uint32_t m_subtype = 0; // channel layout indicator

    // "cue " chunk
    uint32_t m_cue_count = 0;

    // "smpl" chunk — loop points
    uint32_t m_loop_count = 0;
    uint32_t m_loop_start = 0; // in samples
    uint32_t m_loop_end = 0;   // in samples (exclusive)

    // "vorb" chunk — Wwise-specific Vorbis metadata
    uint32_t m_sample_count = 0;
    uint32_t m_setup_packet_offset = 0;       // offset within data chunk to setup packet
    uint32_t m_first_audio_packet_offset = 0; // offset within data chunk to first audio packet
    uint32_t m_uid = 0;                       // Vorbis stream UID
    uint8_t m_blocksize_0_pow = 0;            // log2 of short block size
    uint8_t m_blocksize_1_pow = 0;            // log2 of long block size

    const bool m_inline_codebooks;       // true: codebooks are in the WEM, not external
    const bool m_full_setup;             // true: full Vorbis setup header (not stripped)
    bool m_header_triad_present = false; // older WEMs include the full id/comment/setup triad
    bool m_old_packet_headers = false;   // older 8-byte packet headers (size:u32 + granule:u32)
    bool m_no_granule = false;           // 2-byte headers with no granule field
    bool m_mod_packets = false; // Wwise "modified" packets needing window-bit reconstruction

    // Endian-aware read functions, set once based on RIFF vs RIFX
    uint16_t (*m_read_16)(std::istream& is) = nullptr;
    uint32_t (*m_read_32)(std::istream& is) = nullptr;

  public:
    // Parses the entire RIFF structure and validates chunks.  Throws ParseError on malformed input.
    WwiseRiffVorbis(const std::string& indata, std::string codebooks_data, bool inline_codebooks,
                    bool full_setup, ForcePacketFormat force_packet_format);

    // Returns a human-readable summary of the parsed WEM metadata.
    [[nodiscard]] std::string GetInfo();

    // Writes a complete OGG Vorbis stream (headers + audio) to `os`.
    void GenerateOgg(std::ostream& os);

    // Rebuilds the Vorbis header packets (id, comment, setup) for stripped WEMs.
    // Outputs mode_blockflag and mode_bits needed by GenerateOgg for modified-packet decoding.
    void GenerateOggHeader(Bitoggstream& os, std::vector<bool>& mode_blockflag, int& mode_bits);

    // Copies pre-existing Vorbis header triad from older WEM formats that include them verbatim.
    void GenerateOggHeaderWithTriad(Bitoggstream& os);
};

} // namespace ww2ogg
