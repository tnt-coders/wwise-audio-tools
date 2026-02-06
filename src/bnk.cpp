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

#include "kaitai/kaitaistream.h"
#include "kaitai/structs/bnk.h"
#include "bnk.h"

namespace {

/**
 * @brief Internal structure to track events and corresponding SFX
 */
struct EventSFX {
  bnk_t::action_type_t action_type{};
  bnk_t::sound_effect_or_voice_t* sfx = nullptr;
  bool is_child = false;
};

/**
 * @brief Find a section by type in the BNK data
 * @param bnk The parsed BNK object
 * @param type Section type to find (e.g., "DATA", "BKHD", "DIDX", "HIRC")
 * @return Pointer to section data, or nullptr if not found
 */
template<typename T>
[[nodiscard]] T* find_section(bnk_t& bnk, std::string_view type) {
  for (const auto& section : *bnk.data()) {
    if (section->type() == type) {
      return static_cast<T*>(section->section_data());
    }
  }
  return nullptr;
}

/**
 * @brief Get parent ID from sound structure
 */
[[nodiscard]] std::uint32_t get_parent_id(bnk_t::sound_effect_or_voice_t* sfx) {
  std::uint32_t parent_id_offset = 6;
  const auto& sound_structure = sfx->sound_structure();

  if (sound_structure.size() < 2) {
    return 0;
  }

  auto num_effects = static_cast<std::uint8_t>(sound_structure.at(1));
  if (num_effects > 0) {
    parent_id_offset++;                    // bit mask for bypassed effects
    parent_id_offset += num_effects * 7;   // 7 bytes for each effect
  }

  if (sound_structure.size() < parent_id_offset + 4) {
    return 0;
  }

  std::uint32_t parent_id = 0;
  std::stringstream ss;
  ss.write(sound_structure.c_str(), static_cast<std::streamsize>(sound_structure.size()));
  ss.seekg(static_cast<std::streamoff>(parent_id_offset));
  ss.read(reinterpret_cast<char*>(&parent_id), 4);

  return parent_id;
}

/**
 * @brief Get a string with the action type from the enum
 */
[[nodiscard]] std::string_view get_event_action_type(bnk_t::action_type_t action_type) {
  switch (action_type) {
    case bnk_t::ACTION_TYPE_PLAY:   return "play";
    case bnk_t::ACTION_TYPE_PAUSE:  return "pause";
    case bnk_t::ACTION_TYPE_STOP:   return "stop";
    case bnk_t::ACTION_TYPE_RESUME: return "resume";
    default:
      // For unknown types, we need to return a stable string
      // Using a thread_local static for the formatted string
      thread_local std::string unknown_type;
      unknown_type = std::to_string(static_cast<int>(action_type));
      return unknown_type;
  }
}

/**
 * @brief Look up an event name from the STID section
 * @param stid_data The STID section data (can be nullptr)
 * @param event_id The event ID to look up
 * @return The event name, or empty string if not found or STID section is missing
 */
[[nodiscard]] std::string lookup_event_name(bnk_t::stid_data_t* stid_data, std::uint32_t event_id) {
  if (!stid_data) {
    return {};
  }

  for (const auto* stid_obj : *stid_data->objs()) {
    if (stid_obj->id() == event_id) {
      return stid_obj->name();
    }
  }

  return {};
}

} // anonymous namespace

namespace wwtools::bnk {

void extract(std::string_view indata, std::vector<std::string>& outdata) {
  kaitai::kstream ks(std::string{indata});
  bnk_t bnk(&ks);

  auto* data_section = find_section<bnk_t::data_data_t>(bnk, "DATA");
  if (!data_section) {
    return;
  }

  const auto num_files = data_section->didx_data()->num_files();
  outdata.reserve(num_files);

  for (const auto& file_data : *data_section->data_obj_section()->data()) {
    outdata.push_back(file_data->file());
  }
}

[[nodiscard]] std::string get_info(std::string_view indata) {
  kaitai::kstream ks(std::string{indata});
  bnk_t bnk(&ks);

  std::string result;

  // Get bank header info
  if (auto* bkhd = find_section<bnk_t::bkhd_data_t>(bnk, "BKHD")) {
    result += std::format("Version: {}\n", bkhd->version());
    result += std::format("Soundbank ID: {}\n", bkhd->id());
  }

  // Get data index info
  if (auto* didx = find_section<bnk_t::didx_data_t>(bnk, "DIDX")) {
    result += std::format("{} embedded WEM files:\n", didx->num_files());
    for (const auto& index : *didx->objs()) {
      result += std::format("\t{}\n", index->id());
    }
  }

  return result;
}

[[nodiscard]] std::string get_event_id_info(std::string_view indata,
                                             std::string_view in_event_id) {
  kaitai::kstream ks(std::string{indata});
  bnk_t bnk(&ks);

  auto* hirc_data = find_section<bnk_t::hirc_data_t>(bnk, "HIRC");
  if (!hirc_data) {
    return {};
  }

  // Load STID section for event name lookup (may be nullptr if not present)
  auto* stid_data = find_section<bnk_t::stid_data_t>(bnk, "STID");

  const bool all_event_ids = in_event_id.empty();
  std::size_t num_events = 0;

  // Map events to their event actions
  std::map<std::uint32_t, std::vector<bnk_t::event_action_t*>> event_to_event_actions;

  for (const auto& obj : *hirc_data->objs()) {
    if (obj->type() != bnk_t::OBJECT_TYPE_EVENT) {
      continue;
    }

    ++num_events;
    auto* event = static_cast<bnk_t::event_t*>(obj->object_data());
    const auto obj_id_str = std::to_string(obj->id());

    // Check if we should process this event
    if (!all_event_ids && obj_id_str != in_event_id) {
      continue;
    }

    // Find matching event actions
    for (const auto& event_action_id : *event->event_actions()) {
      for (const auto& action_obj : *hirc_data->objs()) {
        if (action_obj->type() != bnk_t::OBJECT_TYPE_EVENT_ACTION) {
          continue;
        }

        auto* event_action = static_cast<bnk_t::event_action_t*>(action_obj->object_data());
        if (action_obj->id() == event_action_id && event_action->game_object_id() != 0) {
          event_to_event_actions[obj->id()].push_back(event_action);
        }
      }
    }
  }

  // Map events to their SFX
  std::map<std::uint32_t, std::vector<EventSFX>> event_to_event_sfxs;

  for (const auto& obj : *hirc_data->objs()) {
    if (obj->type() != bnk_t::OBJECT_TYPE_SOUND_EFFECT_OR_VOICE) {
      continue;
    }

    auto* sfx = static_cast<bnk_t::sound_effect_or_voice_t*>(obj->object_data());
    const auto parent_id = get_parent_id(sfx);

    for (const auto& [event_id, event_actions] : event_to_event_actions) {
      for (const auto* event_action : event_actions) {
        const auto game_obj_id = event_action->game_object_id();
        if (game_obj_id != obj->id() && game_obj_id != parent_id) {
          continue;
        }

        event_to_event_sfxs[event_id].push_back({
          .action_type = event_action->type(),
          .sfx = sfx,
          .is_child = (game_obj_id == parent_id)
        });
      }
    }
  }

  // Build result string
  std::string result;
  result += std::format("Found {} event(s)\n", num_events);
  result += std::format("{} of them point to files in this BNK\n\n", event_to_event_sfxs.size());

  for (const auto& [event_id, event_sfxs] : event_to_event_sfxs) {
    auto event_name = lookup_event_name(stid_data, event_id);
    result += std::format("{} ({})\n", event_id,
                          event_name.empty() ? "can't find name" : event_name);

    for (const auto& event_sfx : event_sfxs) {
      result += std::format("\t{} {}{}\n",
                            get_event_action_type(event_sfx.action_type),
                            event_sfx.sfx->audio_file_id(),
                            event_sfx.is_child ? " (child)" : "");
    }
    result += '\n';
  }

  return result;
}

[[nodiscard]] std::string get_wem_id_at_index(std::string_view indata,
                                               std::size_t index) {
  kaitai::kstream ks(std::string{indata});
  bnk_t bnk(&ks);

  auto* didx = find_section<bnk_t::didx_data_t>(bnk, "DIDX");
  if (!didx || index >= didx->objs()->size()) {
    return {};
  }

  return std::to_string(didx->objs()->at(index)->id());
}

[[nodiscard]] std::string get_event_name_from_id([[maybe_unused]] std::uint32_t event_id) {
  // This function signature is maintained for API compatibility, but it cannot
  // look up event names without the BNK file data. Event name lookup is now
  // performed within get_event_id_info() using the STID section.
  // If you need event name lookup, use get_event_id_info() instead.
  return {};
}

} // namespace wwtools::bnk
