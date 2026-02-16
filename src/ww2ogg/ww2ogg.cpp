#include <ostream>
#include <string>

#include "ww2ogg/errors.h"
#include "ww2ogg/packed_codebooks.h"
#include "ww2ogg/ww2ogg.h"
#include "ww2ogg/wwriff.h"

namespace ww2ogg
{

void Ww2Ogg(const std::string& indata, std::ostream& outdata,
            const unsigned char* const codebooks_data, const bool inline_codebooks,
            const bool full_setup, const ForcePacketFormat force_packet_format)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const std::string codebooks_data_s(reinterpret_cast<const char*>(codebooks_data),
                                       g_packed_codebooks_bin_len);
    WwiseRiffVorbis ww(indata, codebooks_data_s, inline_codebooks, full_setup, force_packet_format);

    ww.GenerateOgg(outdata);
}

[[nodiscard]] std::string WemInfo(const std::string& indata,
                                  const unsigned char* const codebooks_data,
                                  const bool inline_codebooks, const bool full_setup,
                                  const ForcePacketFormat force_packet_format)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const std::string codebooks_data_s(reinterpret_cast<const char*>(codebooks_data),
                                       g_packed_codebooks_bin_len);
    WwiseRiffVorbis ww(indata, codebooks_data_s, inline_codebooks, full_setup, force_packet_format);
    return ww.GetInfo();
}

} // namespace ww2ogg
