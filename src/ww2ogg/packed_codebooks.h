#pragma once

// Extern declarations for the compiled-in Vorbis codebook data.
// The actual data is in packed_codebooks.cpp or packed_codebooks_aoTuV_603.cpp,
// selected by the PACKED_CODEBOOKS_AOTUV CMake option.
namespace ww2ogg
{
extern const unsigned char g_packed_codebooks_bin[];
extern const unsigned int g_packed_codebooks_bin_len;
} // namespace ww2ogg
