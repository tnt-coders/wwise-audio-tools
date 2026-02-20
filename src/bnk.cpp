#include <cstdint>
#include <format>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "bnk.h"
#include "kaitai/kaitaistream.h"
#include "kaitai/structs/bnk.h"

namespace
{

// Associates an event action (play/stop/etc.) with the SFX object it targets.
// m_is_child is set when the SFX was reached through a parent container rather
// than being directly referenced by the event action's game_object_id.
struct EventSFX
{
    bnk_t::action_type_t m_action_type{};
    bnk_t::sound_effect_or_voice_t* m_sfx = nullptr;
    bool m_is_child = false;
};

// Searches the top-level BNK section list for one matching `type` (e.g. "BKHD", "DATA", "HIRC")
// and returns a pointer to its parsed data, or nullptr if not present.
template <typename T> [[nodiscard]] T* FindSection(bnk_t& bnk, std::string_view type)
{
    for (const auto& section : *bnk.data())
    {
        if (section->type() == type)
        {
            return static_cast<T*>(section->section_data());
        }
    }
    return nullptr;
}

// Extracts the parent object ID from a SFX's raw sound_structure blob.
//
// The sound_structure layout (simplified):
//   [0]     override_parent_fx (1 byte)
//   [1]     num_effects (1 byte)
//   if num_effects > 0:
//     [2]     bypassed_fx bitmask (1 byte)
//     [3..]   num_effects * 7 bytes of effect entries
//   then:
//     [+0..+3]  bus_id (4 bytes, skipped)
//     [+4..+5]  direct_parent_id is at offset +4 (but we read from offset 6 + effect_block_size)
//
// The parent ID links an SFX to its container (e.g. random/sequence container),
// allowing event->container->child SFX resolution in GetEventIdInfo.
[[nodiscard]] std::uint32_t GetParentId(bnk_t::sound_effect_or_voice_t* sfx)
{
    std::uint32_t parent_id_offset = 6;
    const auto& sound_structure = sfx->sound_structure();

    if (sound_structure.size() < 2)
    {
        return 0;
    }

    const auto num_effects = static_cast<std::uint8_t>(sound_structure.at(1));
    if (num_effects > 0)
    {
        parent_id_offset++;                  // bit mask for bypassed effects
        parent_id_offset += num_effects * 7; // 7 bytes for each effect
    }

    if (sound_structure.size() < parent_id_offset + 4)
    {
        return 0;
    }

    // Read the 4-byte LE parent ID from the computed offset
    std::uint32_t parent_id = 0;
    std::stringstream ss;
    ss.write(sound_structure.c_str(), static_cast<std::streamsize>(sound_structure.size()));
    ss.seekg(static_cast<std::streamoff>(parent_id_offset));
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    ss.read(reinterpret_cast<char*>(&parent_id), 4);

    return parent_id;
}

// Maps a BNK event action type enum to a human-readable label.
// Uses a thread_local string for unknown types so the returned string_view stays valid.
[[nodiscard]] std::string_view GetEventActionType(const bnk_t::action_type_t action_type)
{
    switch (action_type)
    {
    case bnk_t::ACTION_TYPE_PLAY:
        return "play";
    case bnk_t::ACTION_TYPE_PAUSE:
        return "pause";
    case bnk_t::ACTION_TYPE_STOP:
        return "stop";
    case bnk_t::ACTION_TYPE_RESUME:
        return "resume";
    default:
        // For unknown types, we need to return a stable string
        // Using a thread_local static for the formatted string
        thread_local std::string g_unknown_type;
        g_unknown_type = std::to_string(static_cast<int>(action_type));
        return g_unknown_type;
    }
}

// Searches the STID (string-to-ID mapping) section for a human-readable event name.
// Returns empty string when stid_data is null or the ID isn't found.
[[nodiscard]] std::string LookupEventName(bnk_t::stid_data_t* stid_data,
                                          const std::uint32_t event_id)
{
    if (!stid_data)
    {
        return {};
    }

    for (const auto* stid_obj : *stid_data->objs())
    {
        if (stid_obj->id() == event_id)
        {
            return stid_obj->name();
        }
    }

    return {};
}

} // anonymous namespace

namespace wwtools::bnk
{

// Parses the BNK and pulls raw WEM file blobs from the DATA section.
// The DATA section contains a DIDX (data index) followed by concatenated WEM payloads.
// Each entry in outdata corresponds to one embedded WEM in index order.
void Extract(const std::string_view indata, std::vector<std::string>& outdata)
{
    kaitai::kstream ks(std::string{indata});
    bnk_t bnk(&ks);

    auto* data_section = FindSection<bnk_t::data_data_t>(bnk, "DATA");
    if (!data_section)
    {
        return;
    }

    const auto num_files = data_section->didx_data()->num_files();
    outdata.reserve(num_files);

    for (const auto& file_data : *data_section->data_obj_section()->data())
    {
        outdata.push_back(file_data->file());
    }
}

[[nodiscard]] std::string GetInfo(const std::string_view indata)
{
    kaitai::kstream ks(std::string{indata});
    bnk_t bnk(&ks);

    std::string result;

    // Get bank header info
    if (auto* bkhd = FindSection<bnk_t::bkhd_data_t>(bnk, "BKHD"))
    {
        result += std::format("Version: {}\n", bkhd->version());
        result += std::format("Soundbank ID: {}\n", bkhd->id());
    }

    // Get data index info
    if (auto* didx = FindSection<bnk_t::didx_data_t>(bnk, "DIDX"))
    {
        result += std::format("{} embedded WEM files:\n", didx->num_files());
        for (const auto& index : *didx->objs())
        {
            result += std::format("\t{}\n", index->id());
        }
    }

    return result;
}

// Builds a human-readable report mapping events to the WEM audio files they trigger.
//
// Resolution chain: Event -> EventAction(s) -> SFX object(s) -> audio_file_id
//
// When in_event_id is empty, reports on ALL events in the BNK.  When non-empty,
// filters to just the event whose numeric ID matches the string.
//
// The three-pass approach:
//   Pass 1: Find events and collect their event-action references
//   Pass 2: Find SFX objects and match them to events via game_object_id or parent_id
//   Pass 3: Format the result string
[[nodiscard]] std::string GetEventIdInfo(const std::string_view indata,
                                         const std::string_view in_event_id)
{
    kaitai::kstream ks(std::string{indata});
    bnk_t bnk(&ks);

    auto* hirc_data = FindSection<bnk_t::hirc_data_t>(bnk, "HIRC");
    if (!hirc_data)
    {
        return {};
    }

    auto* stid_data = FindSection<bnk_t::stid_data_t>(bnk, "STID");

    const bool all_event_ids = in_event_id.empty();
    std::size_t num_events = 0;

    // Pass 1: Map each event to its event-action objects
    std::map<std::uint32_t, std::vector<bnk_t::event_action_t*>> event_to_event_actions;

    for (const auto& obj : *hirc_data->objs())
    {
        if (obj->type() != bnk_t::OBJECT_TYPE_EVENT)
        {
            continue;
        }

        ++num_events;
        auto* event = dynamic_cast<bnk_t::event_t*>(obj->object_data());
        const auto obj_id_str = std::to_string(obj->id());

        // Check if we should process this event
        if (!all_event_ids && obj_id_str != in_event_id)
        {
            continue;
        }

        // Find matching event actions
        for (const auto& event_action_id : *event->event_actions())
        {
            for (const auto& action_obj : *hirc_data->objs())
            {
                if (action_obj->type() != bnk_t::OBJECT_TYPE_EVENT_ACTION)
                {
                    continue;
                }

                auto* event_action =
                    dynamic_cast<bnk_t::event_action_t*>(action_obj->object_data());
                if (action_obj->id() == event_action_id && event_action->game_object_id() != 0)
                {
                    event_to_event_actions[obj->id()].push_back(event_action);
                }
            }
        }
    }

    // Pass 2: Match SFX objects to events via event-action game_object_id or parent container
    std::map<std::uint32_t, std::vector<EventSFX>> event_to_event_sfxs;

    for (const auto& obj : *hirc_data->objs())
    {
        if (obj->type() != bnk_t::OBJECT_TYPE_SOUND_EFFECT_OR_VOICE)
        {
            continue;
        }

        auto* sfx = dynamic_cast<bnk_t::sound_effect_or_voice_t*>(obj->object_data());
        const auto parent_id = GetParentId(sfx);

        for (const auto& [event_id, event_actions] : event_to_event_actions)
        {
            for (const auto* event_action : event_actions)
            {
                const auto game_obj_id = event_action->game_object_id();
                if (game_obj_id != obj->id() && game_obj_id != parent_id)
                {
                    continue;
                }

                event_to_event_sfxs[event_id].push_back({.m_action_type = event_action->type(),
                                                         .m_sfx = sfx,
                                                         .m_is_child = (game_obj_id == parent_id)});
            }
        }
    }

    // Pass 3: Format the output
    std::string result;
    result += std::format("Found {} event(s)\n", num_events);
    result += std::format("{} of them point to files in this BNK\n\n", event_to_event_sfxs.size());

    for (const auto& [event_id, event_sfxs] : event_to_event_sfxs)
    {
        const auto event_name = LookupEventName(stid_data, event_id);
        result +=
            std::format("{} ({})\n", event_id, event_name.empty() ? "can't find name" : event_name);

        for (const auto& event_sfx : event_sfxs)
        {
            result += std::format("\t{} {}{}\n", GetEventActionType(event_sfx.m_action_type),
                                  event_sfx.m_sfx->audio_file_id(),
                                  event_sfx.m_is_child ? " (child)" : "");
        }
        result += '\n';
    }

    return result;
}

[[nodiscard]] std::string GetEventNameFromId([[maybe_unused]] const std::uint32_t event_id)
{
    // This function signature is maintained for API compatibility, but it cannot
    // look up event names without the BNK file data. Event name lookup is now
    // performed within get_event_id_info() using the STID section.
    // If you need event name lookup, use get_event_id_info() instead.
    return {};
}

[[nodiscard]] std::vector<std::uint32_t> GetWemIds(const std::string_view indata)
{
    kaitai::kstream ks(std::string{indata});
    bnk_t bnk(&ks);

    std::vector<std::uint32_t> ids;

    auto* didx = FindSection<bnk_t::didx_data_t>(bnk, "DIDX");
    if (!didx)
    {
        return ids;
    }

    ids.reserve(didx->objs()->size());
    for (const auto& obj : *didx->objs())
    {
        ids.push_back(obj->id());
    }

    return ids;
}

// Scans HIRC SFX objects for those marked as streamed (included_or_streamed != 0).
// Streamed WEMs only have a small prefetch stub embedded in the BNK; the full audio
// lives in a separate .wem file that the caller must locate and read.
[[nodiscard]] std::vector<std::uint32_t> GetStreamedWemIds(const std::string_view indata)
{
    kaitai::kstream ks(std::string{indata});
    bnk_t bnk(&ks);

    std::vector<std::uint32_t> ids;

    auto* hirc_data = FindSection<bnk_t::hirc_data_t>(bnk, "HIRC");
    if (!hirc_data)
    {
        return ids;
    }

    for (const auto& obj : *hirc_data->objs())
    {
        if (obj->type() != bnk_t::OBJECT_TYPE_SOUND_EFFECT_OR_VOICE)
        {
            continue;
        }

        auto* sfx = dynamic_cast<bnk_t::sound_effect_or_voice_t*>(obj->object_data());
        if (sfx->included_or_streamed() != 0)
        {
            ids.push_back(sfx->audio_file_id());
        }
    }

    return ids;
}

} // namespace wwtools::bnk
