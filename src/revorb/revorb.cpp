/**
 * @file revorb.cpp
 * @brief Recomputes page granule positions in Ogg Vorbis files
 * @version 0.2 (2008/06/29)
 * @note Modernized to C++23
 *
 * Copyright (c) 2008, Jiri Hruska <jiri.hruska@fud.cz>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sstream>

#include <ogg/ogg.h>
#include <vorbis/codec.h>

namespace {

constexpr int kBufferSize = 4096;

bool g_failed = false;

/**
 * @brief RAII wrapper for ogg_stream_state
 */
class OggStreamGuard {
  ogg_stream_state* stream_;
  bool initialized_;

public:
  explicit OggStreamGuard(ogg_stream_state* stream)
      : stream_(stream), initialized_(false) {}

  void init(int serialno) {
    ogg_stream_init(stream_, serialno);
    initialized_ = true;
  }

  void clear() {
    if (initialized_) {
      ogg_stream_clear(stream_);
      initialized_ = false;
    }
  }

  ~OggStreamGuard() { clear(); }

  // Non-copyable
  OggStreamGuard(const OggStreamGuard&) = delete;
  OggStreamGuard& operator=(const OggStreamGuard&) = delete;
};

/**
 * @brief RAII wrapper for vorbis_comment
 */
class VorbisCommentGuard {
  vorbis_comment* vc_;
  bool initialized_;

public:
  explicit VorbisCommentGuard(vorbis_comment* vc)
      : vc_(vc), initialized_(false) {}

  void init() {
    vorbis_comment_init(vc_);
    initialized_ = true;
  }

  void clear() {
    if (initialized_) {
      vorbis_comment_clear(vc_);
      initialized_ = false;
    }
  }

  ~VorbisCommentGuard() { clear(); }

  // Non-copyable
  VorbisCommentGuard(const VorbisCommentGuard&) = delete;
  VorbisCommentGuard& operator=(const VorbisCommentGuard&) = delete;
};

} // anonymous namespace

namespace revorb {

[[nodiscard]] auto copy_headers(std::stringstream& fi, ogg_sync_state* si,
                                ogg_stream_state* is, std::stringstream& outdata,
                                ogg_stream_state* os, vorbis_info* vi) -> bool {
  char* buffer = ogg_sync_buffer(si, kBufferSize);

  fi.read(buffer, kBufferSize);
  auto numread = fi.gcount();

  ogg_sync_wrote(si, static_cast<long>(numread));

  ogg_page page{};
  int result = ogg_sync_pageout(si, &page);
  if (result != 1) {
    return false;
  }

  ogg_stream_init(is, ogg_page_serialno(&page));
  ogg_stream_init(os, ogg_page_serialno(&page));

  if (ogg_stream_pagein(is, &page) < 0) {
    ogg_stream_clear(is);
    ogg_stream_clear(os);
    return false;
  }

  ogg_packet packet{};
  if (ogg_stream_packetout(is, &packet) != 1) {
    ogg_stream_clear(is);
    ogg_stream_clear(os);
    return false;
  }

  vorbis_comment vc{};
  vorbis_comment_init(&vc);
  if (vorbis_synthesis_headerin(vi, &vc, &packet) < 0) {
    vorbis_comment_clear(&vc);
    ogg_stream_clear(is);
    ogg_stream_clear(os);
    return false;
  }

  ogg_stream_packetin(os, &packet);

  int i = 0;
  while (i < 2) {
    int res = ogg_sync_pageout(si, &page);

    if (res == 0) {
      buffer = ogg_sync_buffer(si, kBufferSize);
      fi.read(buffer, kBufferSize);
      numread = fi.gcount();
      if (numread == 0 && i < 2) {
        vorbis_comment_clear(&vc);
        ogg_stream_clear(is);
        ogg_stream_clear(os);
        return false;
      }
      ogg_sync_wrote(si, kBufferSize);
      continue;
    }

    if (res == 1) {
      ogg_stream_pagein(is, &page);
      while (i < 2) {
        res = ogg_stream_packetout(is, &packet);
        if (res == 0) {
          break;
        }
        if (res < 0) {
          vorbis_comment_clear(&vc);
          ogg_stream_clear(is);
          ogg_stream_clear(os);
          return false;
        }
        vorbis_synthesis_headerin(vi, &vc, &packet);
        ogg_stream_packetin(os, &packet);
        ++i;
      }
    }
  }

  vorbis_comment_clear(&vc);

  while (ogg_stream_flush(os, &page) != 0) {
    outdata.write(reinterpret_cast<char*>(page.header), page.header_len);
    outdata.write(reinterpret_cast<char*>(page.body), page.body_len);
  }

  return true;
}

[[nodiscard]] auto revorb(std::istream& indata, std::stringstream& outdata) -> bool {
  g_failed = false;

  std::stringstream indata_ss;
  indata_ss << indata.rdbuf();

  ogg_sync_state sync_in{};
  ogg_sync_state sync_out{};
  ogg_sync_init(&sync_in);
  ogg_sync_init(&sync_out);

  ogg_stream_state stream_in{};
  ogg_stream_state stream_out{};
  vorbis_info vi{};
  vorbis_info_init(&vi);

  ogg_packet packet{};
  ogg_page page{};

  if (copy_headers(indata_ss, &sync_in, &stream_in, outdata, &stream_out, &vi)) {
    ogg_int64_t granpos = 0;
    ogg_int64_t packetnum = 0;
    int lastbs = 0;

    while (true) {
      int eos = 0;
      while (eos == 0) {
        int res = ogg_sync_pageout(&sync_in, &page);
        if (res == 0) {
          char* buffer = ogg_sync_buffer(&sync_in, kBufferSize);
          indata_ss.read(buffer, kBufferSize);
          auto numread = indata_ss.gcount();
          if (numread > 0) {
            ogg_sync_wrote(&sync_in, static_cast<long>(numread));
          } else {
            eos = 2;
          }
          continue;
        }

        if (res < 0) {
          g_failed = true;
        } else {
          if (ogg_page_eos(&page) != 0) {
            eos = 1;
          }
          ogg_stream_pagein(&stream_in, &page);

          while (true) {
            res = ogg_stream_packetout(&stream_in, &packet);
            if (res == 0) {
              break;
            }
            if (res < 0) {
              g_failed = true;
              continue;
            }

            int bs = vorbis_packet_blocksize(&vi, &packet);
            if (lastbs != 0) {
              granpos += (lastbs + bs) / 4;
            }
            lastbs = bs;

            packet.granulepos = granpos;
            packet.packetno = packetnum++;
            if (packet.e_o_s == 0) {
              ogg_stream_packetin(&stream_out, &packet);

              ogg_page opage{};
              while (ogg_stream_pageout(&stream_out, &opage) != 0) {
                outdata.write(reinterpret_cast<char*>(opage.header),
                              opage.header_len);
                outdata.write(reinterpret_cast<char*>(opage.body),
                              opage.body_len);
              }
            }
          }
        }
      }

      if (eos == 2) {
        break;
      }

      {
        packet.e_o_s = 1;
        ogg_stream_packetin(&stream_out, &packet);
        ogg_page opage{};
        while (ogg_stream_flush(&stream_out, &opage) != 0) {
          outdata.write(reinterpret_cast<char*>(opage.header),
                        opage.header_len);
          outdata.write(reinterpret_cast<char*>(opage.body), opage.body_len);
        }
        ogg_stream_clear(&stream_in);
        break;
      }
    }

    ogg_stream_clear(&stream_out);
  } else {
    g_failed = true;
  }

  vorbis_info_clear(&vi);

  ogg_sync_clear(&sync_in);
  ogg_sync_clear(&sync_out);

  return !g_failed;
}

} // namespace revorb
