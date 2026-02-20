#pragma once

#include <string>
#include <vector>

#include "bitstream.h"
#include "errors.h"

// Helper functions ported from Tremor (lowmem Vorbis decoder).
// These are used during codebook parsing/reconstruction.
namespace
{

// Returns number of bits required to represent v (integer log2 + 1, 0 for v==0).
[[nodiscard]] inline int Ilog(unsigned int v)
{
    int ret = 0;
    while (v != 0)
    {
        ++ret;
        v >>= 1;
    }
    return ret;
}

// Calculates the number of quantization values for a Vorbis maptype 1 codebook.
// Returns floor(entries^(1/dimensions)) — the number of scalar values needed to
// reconstruct the full lookup table via the multiplicative decomposition.
[[nodiscard]] inline unsigned int BookMaptype1Quantvals(const unsigned int entries,
                                                        const unsigned int dimensions)
{
    // Get us a starting hint, we'll polish it below
    const int bits = Ilog(entries);
    int vals = static_cast<int>(entries >> ((bits - 1) * (dimensions - 1) / dimensions));

    while (true)
    {
        unsigned long acc = 1;
        unsigned long acc1 = 1;
        for (unsigned int i = 0; i < dimensions; ++i)
        {
            acc *= static_cast<unsigned long>(vals);
            acc1 *= static_cast<unsigned long>(vals + 1);
        }
        if (acc <= entries && acc1 > entries)
        {
            return static_cast<unsigned int>(vals);
        }
        if (acc > entries)
        {
            --vals;
        }
        else
        {
            ++vals;
        }
    }
}

} // anonymous namespace

namespace ww2ogg
{

class CodebookLibrary
{
    std::vector<char> m_codebook_data;
    std::vector<long> m_codebook_offsets;

    // Non-copyable
    CodebookLibrary(const CodebookLibrary&) = delete;
    CodebookLibrary& operator=(const CodebookLibrary&) = delete;

  public:
    // Movable
    CodebookLibrary(CodebookLibrary&&) = default;
    CodebookLibrary& operator=(CodebookLibrary&&) = default;

    explicit CodebookLibrary(const std::string& indata);

    // Creates an empty library used when rebuilding codebooks from inline data.
    CodebookLibrary();

    ~CodebookLibrary() = default;

    // Returns a pointer to raw codebook bytes for id i, or nullptr when i is out of range.
    [[nodiscard]] const char* GetCodebook(int i) const
    {
        if (m_codebook_data.empty() || m_codebook_offsets.empty())
        {
            throw ParseErrorStr("codebook library not loaded");
        }
        if (i >= static_cast<int>(m_codebook_offsets.size()) - 1 || i < 0)
        {
            return nullptr;
        }
        return &m_codebook_data[m_codebook_offsets[i]];
    }

    // Returns codebook byte size for id i, or -1 when i is out of range.
    [[nodiscard]] long GetCodebookSize(int i) const
    {
        if (m_codebook_data.empty() || m_codebook_offsets.empty())
        {
            throw ParseErrorStr("codebook library not loaded");
        }
        if (i >= static_cast<int>(m_codebook_offsets.size()) - 1 || i < 0)
        {
            return -1;
        }
        return m_codebook_offsets[i + 1] - m_codebook_offsets[i];
    }

    // Rebuilds a codebook by id into an OGG bitstream.
    void Rebuild(int i, Bitoggstream& bos);
    // Rebuilds a codebook from a source bitstream and explicit size.
    void Rebuild(Bitstream& bis, unsigned long cb_size, Bitoggstream& bos);
    // Copies a codebook blob from input bitstream to output bitstream.
    void Copy(Bitstream& bis, Bitoggstream& bos);
};

} // namespace ww2ogg
