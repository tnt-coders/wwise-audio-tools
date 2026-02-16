#pragma once

#include <istream>
#include <sstream>

namespace revorb
{

// Rewrites OGG page granule positions so downstream players/decoders seek correctly.
// Returns true when the stream is parsed and rewritten successfully; false on malformed/invalid
// OGG. `outdata` receives rewritten bytes (partial output may exist when false is returned).
[[nodiscard]] bool Revorb(std::istream& indata, std::stringstream& outdata);

} // namespace revorb
