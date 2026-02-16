/**
 * @file wwtools.cpp
 * @brief Implementation of public API functions
 * @note Modernized to C++23
 */

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "bnk.h"
#include "revorb/revorb.h"
#include "ww2ogg/ww2ogg.h"
#include "wwtools/wwtools.h"

namespace wwtools
{

[[nodiscard]] std::string Wem2Ogg(const std::string_view indata)
{
    std::stringstream wem_out;
    std::stringstream revorb_out;

    // Convert WEM to intermediate OGG format
    ww2ogg::Ww2Ogg(std::string{indata}, wem_out);

    // Fix granule positions in the OGG stream
    if (!revorb::Revorb(wem_out, revorb_out))
    {
        throw std::runtime_error("revorb failed to fix OGG granule positions");
    }

    return revorb_out.str();
}

[[nodiscard]] std::vector<BnkEntry> BnkExtract(const std::string_view indata)
{
    const auto ids = bnk::GetWemIds(indata);
    const auto streamed_ids = bnk::GetStreamedWemIds(indata);

    std::vector<std::string> raw_wems;
    bnk::Extract(indata, raw_wems);

    std::vector<BnkEntry> result;
    result.reserve(ids.size());

    for (std::size_t i = 0; i < ids.size(); ++i)
    {
        result.push_back({
            .id = ids[i],
            .streamed = std::ranges::contains(streamed_ids, ids[i]),
            .data = (i < raw_wems.size()) ? std::move(raw_wems[i]) : std::string{},
        });
    }

    return result;
}

} // namespace wwtools
