#pragma once

#include <string>
#include <vector>

#include "game_state.h"

namespace liminal {

enum class InventoryCommand {
    None,
    List,
    Inspect,
    Take,
    Drop,
    Unsupported,
    Use,
    Ambiguous,
};

struct InventoryRequest {
    InventoryCommand command = InventoryCommand::None;
    std::string item_id;
    std::string target;
};

// The caller supplies an already normalized command: lowercase ASCII, folded
// accents, punctuation replaced by spaces and spaces compacted.
InventoryRequest ParseInventoryRequest(const std::string& normalized);

bool IsKnownInventoryItem(const std::string& id);
std::string InventoryItemName(const std::string& id, GameLanguage language);
std::string InventoryItemDescription(const std::string& id, GameLanguage language);
std::string FormatInventory(const std::vector<std::string>& items, GameLanguage language);

}  // namespace liminal
