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

namespace
{

constexpr int g_k_buffer_size = 4096;

/**
 * @brief RAII wrapper for ogg_stream_state
 */
class OggStreamGuard
{
    ogg_stream_state* m_stream;
    bool m_initialized{false};

  public:
    explicit OggStreamGuard(ogg_stream_state* const stream) : m_stream(stream)
    {
    }

    void Init(const int serialno)
    {
        ogg_stream_init(m_stream, serialno);
        m_initialized = true;
    }

    void Clear()
    {
        if (m_initialized)
        {
            ogg_stream_clear(m_stream);
            m_initialized = false;
        }
    }

    ~OggStreamGuard()
    {
        Clear();
    }

    // Non-copyable, non-movable
    OggStreamGuard(const OggStreamGuard&) = delete;
    OggStreamGuard& operator=(const OggStreamGuard&) = delete;
    OggStreamGuard(OggStreamGuard&&) = delete;
    OggStreamGuard& operator=(OggStreamGuard&&) = delete;
};

/**
 * @brief RAII wrapper for vorbis_comment
 */
class VorbisCommentGuard
{
    vorbis_comment* m_vc;
    bool m_initialized{false};

  public:
    explicit VorbisCommentGuard(vorbis_comment* const vc) : m_vc(vc)
    {
    }

    void Init()
    {
        vorbis_comment_init(m_vc);
        m_initialized = true;
    }

    void Clear()
    {
        if (m_initialized)
        {
            vorbis_comment_clear(m_vc);
            m_initialized = false;
        }
    }

    ~VorbisCommentGuard()
    {
        Clear();
    }

    // Non-copyable, non-movable
    VorbisCommentGuard(const VorbisCommentGuard&) = delete;
    VorbisCommentGuard& operator=(const VorbisCommentGuard&) = delete;
    VorbisCommentGuard(VorbisCommentGuard&&) = delete;
    VorbisCommentGuard& operator=(VorbisCommentGuard&&) = delete;
};

} // anonymous namespace

namespace revorb
{

[[nodiscard]] bool CopyHeaders(std::stringstream& fi, ogg_sync_state* si, ogg_stream_state* is,
                               std::stringstream& outdata, ogg_stream_state* os, vorbis_info* vi)
{
    char* buffer = ogg_sync_buffer(si, g_k_buffer_size);

    fi.read(buffer, g_k_buffer_size);
    auto numread = fi.gcount();

    ogg_sync_wrote(si, static_cast<long>(numread));

    ogg_page page{};
    int result = ogg_sync_pageout(si, &page);
    if (result != 1)
    {
        return false;
    }

    ogg_stream_init(is, ogg_page_serialno(&page));
    ogg_stream_init(os, ogg_page_serialno(&page));

    if (ogg_stream_pagein(is, &page) < 0)
    {
        ogg_stream_clear(is);
        ogg_stream_clear(os);
        return false;
    }

    ogg_packet packet{};
    if (ogg_stream_packetout(is, &packet) != 1)
    {
        ogg_stream_clear(is);
        ogg_stream_clear(os);
        return false;
    }

    vorbis_comment vc{};
    vorbis_comment_init(&vc);
    if (vorbis_synthesis_headerin(vi, &vc, &packet) < 0)
    {
        vorbis_comment_clear(&vc);
        ogg_stream_clear(is);
        ogg_stream_clear(os);
        return false;
    }

    ogg_stream_packetin(os, &packet);

    int i = 0;
    while (i < 2)
    {
        int res = ogg_sync_pageout(si, &page);

        if (res == 0)
        {
            buffer = ogg_sync_buffer(si, g_k_buffer_size);
            fi.read(buffer, g_k_buffer_size);
            numread = fi.gcount();
            if (numread == 0 && i < 2)
            {
                vorbis_comment_clear(&vc);
                ogg_stream_clear(is);
                ogg_stream_clear(os);
                return false;
            }
            ogg_sync_wrote(si, g_k_buffer_size);
            continue;
        }

        if (res == 1)
        {
            ogg_stream_pagein(is, &page);
            while (i < 2)
            {
                res = ogg_stream_packetout(is, &packet);
                if (res == 0)
                {
                    break;
                }
                if (res < 0)
                {
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

    while (ogg_stream_flush(os, &page) != 0)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        outdata.write(reinterpret_cast<char*>(page.header), page.header_len);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        outdata.write(reinterpret_cast<char*>(page.body), page.body_len);
    }

    return true;
}

[[nodiscard]] bool Revorb(std::istream& indata, std::stringstream& outdata)
{
    bool failed = false;

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

    if (CopyHeaders(indata_ss, &sync_in, &stream_in, outdata, &stream_out, &vi))
    {
        ogg_int64_t granpos = 0;
        ogg_int64_t packetnum = 0;
        long lastbs = 0;

        while (true)
        {
            int eos = 0;
            while (eos == 0)
            {
                int res = ogg_sync_pageout(&sync_in, &page);
                if (res == 0)
                {
                    char* buffer = ogg_sync_buffer(&sync_in, g_k_buffer_size);
                    indata_ss.read(buffer, g_k_buffer_size);
                    const auto numread = indata_ss.gcount();
                    if (numread > 0)
                    {
                        ogg_sync_wrote(&sync_in, static_cast<long>(numread));
                    }
                    else
                    {
                        eos = 2;
                    }
                    continue;
                }

                if (res < 0)
                {
                    failed = true;
                }
                else
                {
                    if (ogg_page_eos(&page) != 0)
                    {
                        eos = 1;
                    }
                    ogg_stream_pagein(&stream_in, &page);

                    while (true)
                    {
                        res = ogg_stream_packetout(&stream_in, &packet);
                        if (res == 0)
                        {
                            break;
                        }
                        if (res < 0)
                        {
                            failed = true;
                            continue;
                        }

                        const auto bs = vorbis_packet_blocksize(&vi, &packet);
                        if (lastbs != 0)
                        {
                            granpos += (lastbs + bs) / 4;
                        }
                        lastbs = bs;

                        packet.granulepos = granpos;
                        packet.packetno = packetnum++;
                        if (packet.e_o_s == 0)
                        {
                            ogg_stream_packetin(&stream_out, &packet);

                            ogg_page opage{};
                            while (ogg_stream_pageout(&stream_out, &opage) != 0)
                            {
                                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                                outdata.write(reinterpret_cast<char*>(opage.header),
                                              opage.header_len);
                                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                                outdata.write(reinterpret_cast<char*>(opage.body), opage.body_len);
                            }
                        }
                    }
                }
            }

            if (eos == 2)
            {
                break;
            }

            {
                packet.e_o_s = 1;
                ogg_stream_packetin(&stream_out, &packet);
                ogg_page opage{};
                while (ogg_stream_flush(&stream_out, &opage) != 0)
                {
                    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                    outdata.write(reinterpret_cast<char*>(opage.header), opage.header_len);
                    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                    outdata.write(reinterpret_cast<char*>(opage.body), opage.body_len);
                }
                ogg_stream_clear(&stream_in);
                break;
            }
        }

        ogg_stream_clear(&stream_out);
    }
    else
    {
        failed = true;
    }

    vorbis_info_clear(&vi);

    ogg_sync_clear(&sync_in);
    ogg_sync_clear(&sync_out);

    return !failed;
}

} // namespace revorb
