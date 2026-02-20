#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace wwtools::bnk
{

// Extracts embedded WEM payloads from a BNK and appends them to outdata.
// Does not clear outdata first; when DATA is missing, this returns without adding entries.
void Extract(std::string_view indata, std::vector<std::string>& outdata);

// Returns a human-readable BNK summary (header/data index details).
[[nodiscard]] std::string GetInfo(std::string_view indata);

// Returns event-to-WEM mapping info for one event ID or all events when ID is empty.
// Returns an empty string when the HIRC section is missing.
[[nodiscard]] std::string GetEventIdInfo(std::string_view indata, std::string_view in_event_id);

// Compatibility stub kept for older callers; currently always returns empty string.
// Use GetEventIdInfo(...) for event-name lookup based on BNK STID data.
[[nodiscard]] std::string GetEventNameFromId(std::uint32_t event_id);

// Returns all WEM IDs referenced by the BNK DIDX section (empty when DIDX is missing).
[[nodiscard]] std::vector<std::uint32_t> GetWemIds(std::string_view indata);

// Returns WEM IDs marked as streamed in HIRC object metadata (empty when HIRC is missing).
[[nodiscard]] std::vector<std::uint32_t> GetStreamedWemIds(std::string_view indata);

} // namespace wwtools::bnk
