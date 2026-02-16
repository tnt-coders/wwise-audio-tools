#pragma once

/**
 * @file codebook.h
 * @brief Vorbis codebook handling for ww2ogg
 * @note Modernized to C++23
 */

#include <string>
#include <vector>

#include "bitstream.h"
#include "errors.h"

// Helper functions from Tremor (lowmem)
namespace
{

/**
 * @brief Integer logarithm base 2
 */
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

/**
 * @brief Calculate quantization values for maptype 1 codebooks
 */
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

/**
 * @brief Manages a library of Vorbis codebooks
 */
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

    /**
     * @brief Construct from codebook data string
     */
    explicit CodebookLibrary(const std::string& indata);

    /**
     * @brief Construct empty library (for inline codebooks)
     */
    CodebookLibrary();

    ~CodebookLibrary() = default;

    /**
     * @brief Get pointer to codebook data
     */
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

    /**
     * @brief Get size of codebook
     */
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

    void Rebuild(int i, Bitoggstream& bos);
    void Rebuild(Bitstream& bis, unsigned long cb_size, Bitoggstream& bos);
    void Copy(Bitstream& bis, Bitoggstream& bos);
};

} // namespace ww2ogg
