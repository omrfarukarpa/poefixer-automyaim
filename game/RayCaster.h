#pragma once

#include "sdk/PluginSDK.h"

#include <cstdint>
#include <cstdlib>

namespace AutoMyAim {

// Line-of-sight over the host's walkable grid (nibble-packed, 2 cells/byte).
// A cell whose value is below the threshold (0 = walls) blocks sight. The grid
// is per-area, so it is refreshed on area change, not every frame.
class RayCaster {
public:
    void Refresh(const PluginSDK::Context* ctx) {
        if (!ctx) return;
        m_grid = ctx->Terrain.GetWalkableGrid();
        m_w = m_grid.Width();
        m_h = m_grid.Height();
    }

    bool Valid() const { return m_grid.Valid() && m_w > 0 && m_h > 0; }

    int Cell(int x, int y) const {
        if (x < 0 || y < 0 || x >= m_w || y >= m_h) return 0;
        const uint8_t* d = m_grid.Data();
        const size_t idx = static_cast<size_t>(y) * m_w + x;
        const uint8_t b = d[idx >> 1];
        return (idx & 1) ? ((b >> 4) & 0x0F) : (b & 0x0F);
    }

    // Intermediate cells only (start and target cells are not tested).
    bool HasLineOfSight(int x0, int y0, int x1, int y1, int blockBelow) const {
        if (!Valid()) return true;
        int dx = std::abs(x1 - x0);
        int dy = -std::abs(y1 - y0);
        int sx = x0 < x1 ? 1 : -1;
        int sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;
        int x = x0, y = y0;
        for (;;) {
            if (x == x1 && y == y1) break;
            const int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x += sx; }
            if (e2 <= dx) { err += dx; y += sy; }
            if (x == x1 && y == y1) break; // don't test the target cell
            if (Cell(x, y) < blockBelow) return false;
        }
        return true;
    }

private:
    PluginSDK::WalkableGridHandle m_grid;
    int m_w = 0;
    int m_h = 0;
};

}
