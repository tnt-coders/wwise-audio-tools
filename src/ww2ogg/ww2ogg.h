#pragma once

#include <ostream>
#include <string>

#include "packed_codebooks.h"
#include "wwriff.h"

namespace ww2ogg
{

// Converts a Wwise WEM byte buffer to OGG and writes the result to `outdata`.
// Throws ParseError-derived exceptions when WEM data is invalid or unsupported.
void Ww2Ogg(const std::string& indata, std::ostream& outdata,
            const unsigned char* codebooks_data = g_packed_codebooks_bin,
            bool inline_codebooks = false, bool full_setup = false,
            ForcePacketFormat force_packet_format = K_NO_FORCE_PACKET_FORMAT);

// Returns a human-readable metadata summary for a WEM buffer without producing OGG output.
// Uses the same parsing path/options as Ww2Ogg and may throw the same ParseError-derived
// exceptions.
[[nodiscard]] std::string WemInfo(const std::string& indata,
                                  const unsigned char* codebooks_data = g_packed_codebooks_bin,
                                  bool inline_codebooks = false, bool full_setup = false,
                                  ForcePacketFormat force_packet_format = K_NO_FORCE_PACKET_FORMAT);

} // namespace ww2ogg
