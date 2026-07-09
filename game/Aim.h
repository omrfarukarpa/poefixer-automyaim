#pragma once

#include "sdk/PluginSDK.h"

#include "../config/Settings.h"
#include "AimTypes.h"
#include "RayCaster.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace AutoMyAim {

class AimEngine {
public:
    const std::vector<Target>& Targets() const { return m_targets; }
    bool HavePlayer() const { return m_havePlayer; }
    float PlayerWorldX() const { return m_pwx; }
    float PlayerWorldY() const { return m_pwy; }
    float PlayerWorldZ() const { return m_pwz; }

    // Build the weighted, LoS-filtered target list; return the best (or null).
    const Target* Compute(const PluginSDK::Context* ctx,
                          const AutoMyAimConfig::Settings& s,
                          const RayCaster& rc,
                          float dispW, float dispH) {
        m_targets.clear();
        m_havePlayer = false;
        if (!ctx) return nullptr;

        const PluginSDK::Entity player = ctx->Entities.GetPlayer();
        if (!player.IsValid) return nullptr;
        m_havePlayer = true;
        m_pwx = player.WorldX; m_pwy = player.WorldY; m_pwz = player.WorldZ;

        const float px = player.GridPositionX;
        const float py = player.GridPositionY;
        const int   pxi = static_cast<int>(px);
        const int   pyi = static_cast<int>(py);
        const float range = static_cast<float>(s.aimRange);
        const float maxDist = range > 1.f ? range : 1.f;

        ctx->Entities.Enumerate([&](const PluginSDK::Entity& e) -> bool {
            if (!e.IsValid) return true;
            if (e.EntityType != PluginSDK::EntityType::Monster) return true;
            if (e.CurrentHP <= 0) return true;
            if (e.EntityState == PluginSDK::EntityState::MonsterFriendly) return true;
            if (IsServerMod(e.Path)) return true;

            const float dx = e.GridPositionX - px;
            const float dy = e.GridPositionY - py;
            const float dist = std::sqrt(dx * dx + dy * dy);
            if (dist > range) return true;

            if (s.useLineOfSight &&
                !rc.HasLineOfSight(pxi, pyi, static_cast<int>(e.GridPositionX),
                                   static_cast<int>(e.GridPositionY), s.losBlockBelow))
                return true;

            Target t;
            t.id = e.Id;
            t.gridX = e.GridPositionX; t.gridY = e.GridPositionY;
            t.worldX = e.WorldX; t.worldY = e.WorldY; t.worldZ = e.WorldZ;
            t.distance = dist;
            t.rarity = e.Rarity;
            t.weight = Weight(s, e, dist, maxDist);

            float sx = 0.f, sy = 0.f;
            if (ctx->Render.WorldToScreen(e.WorldX, e.WorldY, e.WorldZ, sx, sy)) {
                t.screenX = sx; t.screenY = sy;
                t.onScreen = sx >= 0.f && sy >= 0.f && sx < dispW && sy < dispH;
            }
            m_targets.push_back(t);
            return true;
        });

        if (m_targets.empty()) return nullptr;
        std::sort(m_targets.begin(), m_targets.end(),
                  [](const Target& a, const Target& b) { return a.weight > b.weight; });

        for (const auto& t : m_targets)
            if (t.onScreen) return &t;
        return nullptr;
    }

private:
    static bool IsServerMod(const std::wstring& path) {
        static const std::wstring kPrefix = L"Metadata/Monsters/MonsterMods/";
        return path.size() >= kPrefix.size() &&
               path.compare(0, kPrefix.size(), kPrefix) == 0;
    }

    static float Weight(const AutoMyAimConfig::Settings& s, const PluginSDK::Entity& e,
                        float dist, float maxDist) {
        const float raw = 1.f - dist / maxDist;
        const float df = raw < 0.f ? 0.f : raw;
        float w = df * df * s.distanceWeight;
        if (s.rarityWeighting) w += RarityWeight(s, e.Rarity) * df;
        if (s.hpWeighting) {
            const float maxHp = static_cast<float>(e.MaxHP + e.MaxES);
            if (maxHp > 0.f) {
                const float hp = static_cast<float>(e.CurrentHP + e.CurrentES) / maxHp;
                w += (s.preferHigherHp ? hp : 1.f - hp) * s.hpWeight * df;
            }
        }
        return w;
    }

    static float RarityWeight(const AutoMyAimConfig::Settings& s, int rarity) {
        switch (rarity) {
            case 1:  return s.wMagic;
            case 2:  return s.wRare;
            case 3:  return s.wUnique;
            default: return s.wNormal;
        }
    }

    std::vector<Target> m_targets;
    bool  m_havePlayer = false;
    float m_pwx = 0.f, m_pwy = 0.f, m_pwz = 0.f;
};

}
