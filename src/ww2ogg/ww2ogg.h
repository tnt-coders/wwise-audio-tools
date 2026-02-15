#pragma once

/**
 * @file ww2ogg.h
 * @brief WEM to OGG conversion interface
 * @note Modernized to C++23
 */

#include <ostream>
#include <string>

#include "packed_codebooks.h"
#include "wwriff.h"

namespace ww2ogg
{

/**
 * @brief Convert WEM data to OGG format
 *
 * @param indata Input WEM data
 * @param outdata Output stream for OGG data
 * @param codebooks_data Codebook data for Vorbis decoding
 * @param inline_codebooks Whether to use inline codebooks
 * @param full_setup Whether to use full setup header
 * @param force_packet_format Force specific packet format
 * @throws ww2ogg::parse_error on malformed WEM data
 * @throws ww2ogg::file_open_error on file access failure
 */
void Ww2Ogg(const std::string& indata, std::ostream& outdata,
            const unsigned char* codebooks_data = packed_codebooks_bin,
            bool inline_codebooks = false, bool full_setup = false,
            ForcePacketFormat force_packet_format = kNoForcePacketFormat);

/**
 * @brief Get information about a WEM file
 *
 * @param indata Input WEM data
 * @param codebooks_data Codebook data for Vorbis decoding
 * @param inline_codebooks Whether to use inline codebooks
 * @param full_setup Whether to use full setup header
 * @param force_packet_format Force specific packet format
 * @return Information string about the WEM file
 */
[[nodiscard]] std::string WemInfo(const std::string& indata,
                                  const unsigned char* codebooks_data = packed_codebooks_bin,
                                  bool inline_codebooks = false, bool full_setup = false,
                                  ForcePacketFormat force_packet_format = kNoForcePacketFormat);

} // namespace ww2ogg
