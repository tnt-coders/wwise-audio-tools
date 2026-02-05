/**
 * @file wwriff.h
 * @brief Wwise RIFF Vorbis file parser
 * @note Modernized to C++23
 */

#ifndef WW2OGG_WWRIFF_H
#define WW2OGG_WWRIFF_H

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "bitstream.h"
#include "errors.h"

#define VERSION "0.24"

namespace ww2ogg {

/**
 * @brief Force specific packet format during conversion
 */
enum ForcePacketFormat {
  kNoForcePacketFormat,
  kForceModPackets,
  kForceNoModPackets
};

/**
 * @brief Parser and converter for Wwise RIFF Vorbis files
 */
class Wwise_RIFF_Vorbis {
  std::string _codebooks_data;
  std::stringstream _indata;
  long _file_size;

  bool _little_endian;

  long _riff_size, _fmt_offset, _cue_offset, _LIST_offset, _smpl_offset, _vorb_offset,
      _data_offset;
  long _fmt_size, _cue_size, _LIST_size, _smpl_size, _vorb_size, _data_size;

  // RIFF fmt
  uint16_t _channels;
  uint32_t _sample_rate;
  uint32_t _avg_bytes_per_second;

  // RIFF extended fmt
  uint16_t _ext_unk;
  uint32_t _subtype;

  // cue info
  uint32_t _cue_count;

  // smpl info
  uint32_t _loop_count, _loop_start, _loop_end;

  // vorbis info
  uint32_t _sample_count;
  uint32_t _setup_packet_offset;
  uint32_t _first_audio_packet_offset;
  uint32_t _uid;
  uint8_t _blocksize_0_pow;
  uint8_t _blocksize_1_pow;

  const bool _inline_codebooks, _full_setup;
  bool _header_triad_present, _old_packet_headers;
  bool _no_granule, _mod_packets;

  uint16_t (*_read_16)(std::istream& is);
  uint32_t (*_read_32)(std::istream& is);

public:
  Wwise_RIFF_Vorbis(const std::string& indata,
                    const std::string& codebooks_data, bool inline_codebooks,
                    bool full_setup, ForcePacketFormat force_packet_format);

  [[nodiscard]] auto get_info() -> std::string;

  void generate_ogg(std::ostream& os);
  void generate_ogg_header(bitoggstream& os, bool*& mode_blockflag,
                           int& mode_bits);
  void generate_ogg_header_with_triad(bitoggstream& os);
};

} // namespace ww2ogg

#endif // WW2OGG_WWRIFF_H
