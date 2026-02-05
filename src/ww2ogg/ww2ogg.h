/**
 * @file ww2ogg.h
 * @brief WEM to OGG conversion interface
 * @note Modernized to C++23
 */

#ifndef WW2OGG_WW2OGG_H
#define WW2OGG_WW2OGG_H

#include <ostream>
#include <string>

#include "packed_codebooks.h"
#include "wwriff.h"

namespace ww2ogg {

/**
 * @brief Convert WEM data to OGG format
 *
 * @param indata Input WEM data
 * @param outdata Output stream for OGG data
 * @param codebooks_data Codebook data for Vorbis decoding
 * @param inline_codebooks Whether to use inline codebooks
 * @param full_setup Whether to use full setup header
 * @param force_packet_format Force specific packet format
 * @return true on success, false on failure
 */
[[nodiscard]] bool ww2ogg(
    const std::string& indata,
    std::ostream& outdata,
    unsigned char* codebooks_data = packed_codebooks_bin,
    bool inline_codebooks = false,
    bool full_setup = false,
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
[[nodiscard]] std::string wem_info(
    const std::string& indata,
    unsigned char* codebooks_data = packed_codebooks_bin,
    bool inline_codebooks = false,
    bool full_setup = false,
    ForcePacketFormat force_packet_format = kNoForcePacketFormat);

} // namespace ww2ogg

#endif // WW2OGG_WW2OGG_H
