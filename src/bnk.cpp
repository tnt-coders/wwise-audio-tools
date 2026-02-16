/**
 * @file bnk.cpp
 * @brief Implementation of BNK file processing functions
 * @note Modernized to C++23
 */

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

/**
 * @brief Internal structure to track events and corresponding SFX
 */
struct EventSFX
{
    BnkT::ActionTypeT m_action_type{};
    BnkT::SoundEffectOrVoiceT* m_sfx = nullptr;
    bool m_is_child = false;
};

/**
 * @brief Find a section by type in the BNK data
 * @param bnk The parsed BNK object
 * @param type Section type to find (e.g., "DATA", "BKHD", "DIDX", "HIRC")
 * @return Pointer to section data, or nullptr if not found
 */
template <typename T> [[nodiscard]] T* FindSection(BnkT& bnk, std::string_view type)
{
    for (const auto& section : *bnk.Data())
    {
        if (section->Type() == type)
        {
            return static_cast<T*>(section->SectionData());
        }
    }
    return nullptr;
}

/**
 * @brief Get parent ID from sound structure
 */
[[nodiscard]] std::uint32_t GetParentId(BnkT::SoundEffectOrVoiceT* sfx)
{
    std::uint32_t parent_id_offset = 6;
    const auto& sound_structure = sfx->SoundStructure();

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

    std::uint32_t parent_id = 0;
    std::stringstream ss;
    ss.write(sound_structure.c_str(), static_cast<std::streamsize>(sound_structure.size()));
    ss.seekg(static_cast<std::streamoff>(parent_id_offset));
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    ss.read(reinterpret_cast<char*>(&parent_id), 4);

    return parent_id;
}

/**
 * @brief Get a string with the action type from the enum
 */
[[nodiscard]] std::string_view GetEventActionType(const BnkT::ActionTypeT action_type)
{
    switch (action_type)
    {
    case BnkT::ACTION_TYPE_PLAY:
        return "play";
    case BnkT::ACTION_TYPE_PAUSE:
        return "pause";
    case BnkT::ACTION_TYPE_STOP:
        return "stop";
    case BnkT::ACTION_TYPE_RESUME:
        return "resume";
    default:
        // For unknown types, we need to return a stable string
        // Using a thread_local static for the formatted string
        thread_local std::string g_unknown_type;
        g_unknown_type = std::to_string(static_cast<int>(action_type));
        return g_unknown_type;
    }
}

/**
 * @brief Look up an event name from the STID section
 * @param stid_data The STID section data (can be nullptr)
 * @param event_id The event ID to look up
 * @return The event name, or empty string if not found or STID section is missing
 */
[[nodiscard]] std::string LookupEventName(BnkT::StidDataT* stid_data, const std::uint32_t event_id)
{
    if (!stid_data)
    {
        return {};
    }

    for (const auto* stid_obj : *stid_data->Objs())
    {
        if (stid_obj->Id() == event_id)
        {
            return stid_obj->Name();
        }
    }

    return {};
}

} // anonymous namespace

namespace wwtools::bnk
{

void Extract(const std::string_view indata, std::vector<std::string>& outdata)
{
    kaitai::kstream ks(std::string{indata});
    BnkT bnk(&ks);

    auto* data_section = FindSection<BnkT::DataDataT>(bnk, "DATA");
    if (!data_section)
    {
        return;
    }

    const auto num_files = data_section->DidxData()->NumFiles();
    outdata.reserve(num_files);

    for (const auto& file_data : *data_section->DataObjSection()->Data())
    {
        outdata.push_back(file_data->File());
    }
}

[[nodiscard]] std::string GetInfo(const std::string_view indata)
{
    kaitai::kstream ks(std::string{indata});
    BnkT bnk(&ks);

    std::string result;

    // Get bank header info
    if (auto* bkhd = FindSection<BnkT::BkhdDataT>(bnk, "BKHD"))
    {
        result += std::format("Version: {}\n", bkhd->Version());
        result += std::format("Soundbank ID: {}\n", bkhd->Id());
    }

    // Get data index info
    if (auto* didx = FindSection<BnkT::DidxDataT>(bnk, "DIDX"))
    {
        result += std::format("{} embedded WEM files:\n", didx->NumFiles());
        for (const auto& index : *didx->Objs())
        {
            result += std::format("\t{}\n", index->Id());
        }
    }

    return result;
}

[[nodiscard]] std::string GetEventIdInfo(const std::string_view indata,
                                         const std::string_view in_event_id)
{
    kaitai::kstream ks(std::string{indata});
    BnkT bnk(&ks);

    auto* hirc_data = FindSection<BnkT::HircDataT>(bnk, "HIRC");
    if (!hirc_data)
    {
        return {};
    }

    // Load STID section for event name lookup (may be nullptr if not present)
    auto* stid_data = FindSection<BnkT::StidDataT>(bnk, "STID");

    const bool all_event_ids = in_event_id.empty();
    std::size_t num_events = 0;

    // Map events to their event actions
    std::map<std::uint32_t, std::vector<BnkT::EventActionT*>> event_to_event_actions;

    for (const auto& obj : *hirc_data->Objs())
    {
        if (obj->Type() != BnkT::OBJECT_TYPE_EVENT)
        {
            continue;
        }

        ++num_events;
        auto* event = dynamic_cast<BnkT::EventT*>(obj->ObjectData());
        const auto obj_id_str = std::to_string(obj->Id());

        // Check if we should process this event
        if (!all_event_ids && obj_id_str != in_event_id)
        {
            continue;
        }

        // Find matching event actions
        for (const auto& event_action_id : *event->EventActions())
        {
            for (const auto& action_obj : *hirc_data->Objs())
            {
                if (action_obj->Type() != BnkT::OBJECT_TYPE_EVENT_ACTION)
                {
                    continue;
                }

                auto* event_action = dynamic_cast<BnkT::EventActionT*>(action_obj->ObjectData());
                if (action_obj->Id() == event_action_id && event_action->GameObjectId() != 0)
                {
                    event_to_event_actions[obj->Id()].push_back(event_action);
                }
            }
        }
    }

    // Map events to their SFX
    std::map<std::uint32_t, std::vector<EventSFX>> event_to_event_sfxs;

    for (const auto& obj : *hirc_data->Objs())
    {
        if (obj->Type() != BnkT::OBJECT_TYPE_SOUND_EFFECT_OR_VOICE)
        {
            continue;
        }

        auto* sfx = dynamic_cast<BnkT::SoundEffectOrVoiceT*>(obj->ObjectData());
        const auto parent_id = GetParentId(sfx);

        for (const auto& [event_id, event_actions] : event_to_event_actions)
        {
            for (const auto* event_action : event_actions)
            {
                const auto game_obj_id = event_action->GameObjectId();
                if (game_obj_id != obj->Id() && game_obj_id != parent_id)
                {
                    continue;
                }

                event_to_event_sfxs[event_id].push_back({.m_action_type = event_action->Type(),
                                                         .m_sfx = sfx,
                                                         .m_is_child = (game_obj_id == parent_id)});
            }
        }
    }

    // Build result string
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
            result +=
                std::format("\t{} {}{}\n", GetEventActionType(event_sfx.m_action_type),
                            event_sfx.m_sfx->AudioFileId(), event_sfx.m_is_child ? " (child)" : "");
        }
        result += '\n';
    }

    return result;
}

[[nodiscard]] std::string GetWemIdAtIndex(const std::string_view indata, const std::size_t index)
{
    kaitai::kstream ks(std::string{indata});
    BnkT bnk(&ks);

    auto* didx = FindSection<BnkT::DidxDataT>(bnk, "DIDX");
    if (!didx || index >= didx->Objs()->size())
    {
        return {};
    }

    return std::to_string(didx->Objs()->at(index)->Id());
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
    BnkT bnk(&ks);

    std::vector<std::uint32_t> ids;

    auto* didx = FindSection<BnkT::DidxDataT>(bnk, "DIDX");
    if (!didx)
    {
        return ids;
    }

    ids.reserve(didx->Objs()->size());
    for (const auto& obj : *didx->Objs())
    {
        ids.push_back(obj->Id());
    }

    return ids;
}

[[nodiscard]] std::vector<std::uint32_t> GetStreamedWemIds(const std::string_view indata)
{
    kaitai::kstream ks(std::string{indata});
    BnkT bnk(&ks);

    std::vector<std::uint32_t> ids;

    auto* hirc_data = FindSection<BnkT::HircDataT>(bnk, "HIRC");
    if (!hirc_data)
    {
        return ids;
    }

    for (const auto& obj : *hirc_data->Objs())
    {
        if (obj->Type() != BnkT::OBJECT_TYPE_SOUND_EFFECT_OR_VOICE)
        {
            continue;
        }

        auto* sfx = dynamic_cast<BnkT::SoundEffectOrVoiceT*>(obj->ObjectData());
        if (sfx->IncludedOrStreamed() != 0)
        {
            ids.push_back(sfx->AudioFileId());
        }
    }

    return ids;
}

} // namespace wwtools::bnk
