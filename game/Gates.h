#pragma once

#include "sdk/PluginSDK.h"

namespace AutoMyAim {

// An item window (inventory/stash/vendor) is open when a large item grid is
// rendered (Grid.Valid on a >=40-box inventory). The world map does not.
inline bool AnyItemPanelOpen(const PluginSDK::Context* ctx) {
    if (!ctx) return false;
    for (const auto& inv : ctx->Inventory.GetAll()) {
        if (!inv.Grid.Valid) continue;
        if (inv.TotalBoxesX * inv.TotalBoxesY < 40) continue;
        return true;
    }
    return false;
}

}
