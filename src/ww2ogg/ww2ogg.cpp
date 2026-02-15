/**
 * @file ww2ogg.cpp
 * @brief WEM to OGG conversion implementation
 * @note Modernized to C++23
 */

#include <ostream>
#include <string>

#include "ww2ogg/errors.h"
#include "ww2ogg/packed_codebooks.h"
#include "ww2ogg/ww2ogg.h"
#include "ww2ogg/wwriff.h"

namespace ww2ogg
{

void ww2ogg(const std::string& indata, std::ostream& outdata,
            const unsigned char* const codebooks_data, const bool inline_codebooks,
            const bool full_setup, const ForcePacketFormat force_packet_format)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const std::string codebooks_data_s(reinterpret_cast<const char*>(codebooks_data),
                                       packed_codebooks_bin_len);
    Wwise_RIFF_Vorbis ww(indata, codebooks_data_s, inline_codebooks, full_setup,
                         force_packet_format);

    ww.generate_ogg(outdata);
}

[[nodiscard]] std::string wem_info(const std::string& indata,
                                   const unsigned char* const codebooks_data,
                                   const bool inline_codebooks, const bool full_setup,
                                   const ForcePacketFormat force_packet_format)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const std::string codebooks_data_s(reinterpret_cast<const char*>(codebooks_data),
                                       packed_codebooks_bin_len);
    Wwise_RIFF_Vorbis ww(indata, codebooks_data_s, inline_codebooks, full_setup,
                         force_packet_format);
    return ww.get_info();
}

} // namespace ww2ogg
