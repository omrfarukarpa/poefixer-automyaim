#pragma once

#include "sdk/PluginSDK.h"

#include "../config/Settings.h"
#include "AimTypes.h"
#include "RayCaster.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

namespace AutoMyAim {

class AimEngine {
public:
    const std::vector<Target>& Targets() const { return m_targets; }
    bool HavePlayer() const { return m_havePlayer; }
    float PlayerWorldX() const { return m_pwx; }
    float PlayerWorldY() const { return m_pwy; }
    float PlayerWorldZ() const { return m_pwz; }

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
            if (e.EntityState == PluginSDK::EntityState::PinnacleBossHidden) return true;
            if (e.EntityState == PluginSDK::EntityState::Useless) return true;
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
            t.buffsAddr = e.Components.Buffs;
            t.targetableAddr = e.Components.Targetable;
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

        const bool useBlacklist = s.skipBlacklistedBuffs && !s.buffBlacklist.empty();
        if (useBlacklist) RefreshBlacklist(s.buffBlacklist);
        int buffChecks = 0;

        for (const auto& t : m_targets) {
            if (!t.onScreen) continue;
            if (useBlacklist && !m_blacklist.empty() && buffChecks < kMaxBuffChecksPerPass) {
                ++buffChecks;
                if (HasBlacklistedBuff(ctx, t)) continue;
            }
            return &t;
        }
        return nullptr;
    }

private:
    static constexpr int kMaxBuffChecksPerPass = 8;

    static std::string NormalizeToken(const std::string& raw) {
        std::string out;
        out.reserve(raw.size());
        for (unsigned char c : raw) {
            if (std::isspace(c)) continue;
            out.push_back(static_cast<char>(std::tolower(c)));
        }
        return out;
    }

    void RefreshBlacklist(const std::string& src) {
        if (src == m_blacklistSrc) return;
        m_blacklistSrc = src;
        m_blacklist.clear();
        std::string token;
        for (char c : src) {
            if (c == ',') {
                token = NormalizeToken(token);
                if (!token.empty()) m_blacklist.push_back(token);
                token.clear();
            } else {
                token.push_back(c);
            }
        }
        token = NormalizeToken(token);
        if (!token.empty()) m_blacklist.push_back(token);
    }

    bool HasBlacklistedBuff(const PluginSDK::Context* ctx, const Target& t) const {
        if (!t.buffsAddr) return false;
        for (const auto& b : ctx->Components.EnumerateBuffs(t.buffsAddr)) {
            if (b.Name.empty()) continue;
            const std::string name = NormalizeToken(b.Name);
            for (const auto& bad : m_blacklist)
                if (name == bad) return true;
        }
        return false;
    }
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
    std::string m_blacklistSrc;
    std::vector<std::string> m_blacklist;
    bool  m_havePlayer = false;
    float m_pwx = 0.f, m_pwy = 0.f, m_pwz = 0.f;
};

}
