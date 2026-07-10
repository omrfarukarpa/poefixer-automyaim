#pragma once

#include "../third_party/json.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

namespace AutoMyAimConfig {

inline constexpr int kAimRangeMin = 20;
inline constexpr int kAimRangeMax = 200;
inline constexpr int kCircleRadiusMin = 50;
inline constexpr int kCircleRadiusMax = 1200;
inline constexpr int kRandomizeRadiusMax = 60;
inline constexpr int kLosBlockBelowMax = 5;
inline constexpr int kActionIntervalMinMs = 0;
inline constexpr int kActionIntervalMaxMs = 200;

inline void ClampColor(float c[4]) {
    for (int i = 0; i < 4; ++i) c[i] = c[i] < 0.f ? 0.f : (c[i] > 1.f ? 1.f : c[i]);
}
inline nlohmann::json ColorToJson(const float c[4]) { return nlohmann::json{c[0], c[1], c[2], c[3]}; }
inline void ColorFromJson(const nlohmann::json& j, float dst[4]) {
    if (!j.is_array()) return;
    for (int i = 0; i < 4 && i < static_cast<int>(j.size()); ++i)
        if (j[i].is_number()) dst[i] = j[i].get<float>();
    ClampColor(dst);
}

struct Settings {
    bool enabled = true;

    int  aimKey = 0x05;    // Mouse4 (XBUTTON1), held to aim
    int  toggleKey = 0x75; // F6, toggles always-on aiming
    int  actionIntervalMs = 0; // 0 = every frame

    int  aimRange = 100;   // grid units: scan + weight falloff
    bool skipUntargetable = true;
    bool debugShowTargetBuffs = false;

    float distanceWeight = 1.0f;
    bool  rarityWeighting = true;
    float wNormal = 0.0f, wMagic = 5.0f, wRare = 20.0f, wUnique = 40.0f;
    bool  hpWeighting = false;
    bool  preferHigherHp = false;
    float hpWeight = 10.0f;

    bool useLineOfSight = true;
    int  losBlockBelow = 1; // walkable cell value < this blocks sight (0 = walls)

    bool confineToCircle = false;
    int  circleRadius = 400; // screen px around the player
    bool randomize = false;
    int  randomizeRadius = 8;

    bool gatePanelOpen = true;
    bool pauseWhilePicking = true; // pause while PickMyLoot is picking

    bool showOverlay = true;      // status + target marker
    bool showRangeCircle = false; // ground aim-range ring

    float targetColor[4] = {0.95f, 0.25f, 0.25f, 1.0f};
    float rangeColor[4]  = {0.30f, 0.60f, 1.00f, 1.0f};

    std::filesystem::path SettingsPath(const std::filesystem::path& dir) const {
        return dir / "config" / "settings.json";
    }

    void Load(const std::filesystem::path& dir) {
        try {
            const auto path = SettingsPath(dir);
            if (!std::filesystem::exists(path)) return;
            std::ifstream in(path);
            if (!in.is_open()) return;
            nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
            if (j.is_discarded() || !j.is_object()) return;

            enabled = j.value("enabled", enabled);
            aimKey = std::clamp(j.value("aim_key", aimKey), 0, 255);
            toggleKey = std::clamp(j.value("toggle_key", toggleKey), 0, 255);
            actionIntervalMs = std::clamp(j.value("action_interval_ms", actionIntervalMs),
                                          kActionIntervalMinMs, kActionIntervalMaxMs);
            aimRange = std::clamp(j.value("aim_range", aimRange), kAimRangeMin, kAimRangeMax);
            skipUntargetable = j.value("skip_untargetable", skipUntargetable);
            debugShowTargetBuffs = j.value("debug_show_target_buffs", debugShowTargetBuffs);

            distanceWeight = j.value("distance_weight", distanceWeight);
            rarityWeighting = j.value("rarity_weighting", rarityWeighting);
            wNormal = j.value("w_normal", wNormal);
            wMagic = j.value("w_magic", wMagic);
            wRare = j.value("w_rare", wRare);
            wUnique = j.value("w_unique", wUnique);
            hpWeighting = j.value("hp_weighting", hpWeighting);
            preferHigherHp = j.value("prefer_higher_hp", preferHigherHp);
            hpWeight = j.value("hp_weight", hpWeight);

            useLineOfSight = j.value("use_line_of_sight", useLineOfSight);
            losBlockBelow = std::clamp(j.value("los_block_below", losBlockBelow), 0, kLosBlockBelowMax);

            confineToCircle = j.value("confine_to_circle", confineToCircle);
            circleRadius = std::clamp(j.value("circle_radius", circleRadius),
                                      kCircleRadiusMin, kCircleRadiusMax);
            randomize = j.value("randomize", randomize);
            randomizeRadius = std::clamp(j.value("randomize_radius", randomizeRadius),
                                         0, kRandomizeRadiusMax);

            gatePanelOpen = j.value("gate_panel_open", gatePanelOpen);
            pauseWhilePicking = j.value("pause_while_picking", pauseWhilePicking);
            showOverlay = j.value("show_overlay", showOverlay);
            showRangeCircle = j.value("show_range_circle", showRangeCircle);

            if (j.contains("colors") && j["colors"].is_object()) {
                ColorFromJson(j["colors"].value("target", nlohmann::json()), targetColor);
                ColorFromJson(j["colors"].value("range", nlohmann::json()), rangeColor);
            }
        } catch (...) {}
    }

    void Save(const std::filesystem::path& dir) const {
        try {
            std::error_code ec;
            std::filesystem::create_directories(dir / "config", ec);
            nlohmann::json j;
            j["enabled"] = enabled;
            j["aim_key"] = aimKey;
            j["toggle_key"] = toggleKey;
            j["action_interval_ms"] = actionIntervalMs;
            j["aim_range"] = aimRange;
            j["skip_untargetable"] = skipUntargetable;
            j["debug_show_target_buffs"] = debugShowTargetBuffs;
            j["distance_weight"] = distanceWeight;
            j["rarity_weighting"] = rarityWeighting;
            j["w_normal"] = wNormal;
            j["w_magic"] = wMagic;
            j["w_rare"] = wRare;
            j["w_unique"] = wUnique;
            j["hp_weighting"] = hpWeighting;
            j["prefer_higher_hp"] = preferHigherHp;
            j["hp_weight"] = hpWeight;
            j["use_line_of_sight"] = useLineOfSight;
            j["los_block_below"] = losBlockBelow;
            j["confine_to_circle"] = confineToCircle;
            j["circle_radius"] = circleRadius;
            j["randomize"] = randomize;
            j["randomize_radius"] = randomizeRadius;
            j["gate_panel_open"] = gatePanelOpen;
            j["pause_while_picking"] = pauseWhilePicking;
            j["show_overlay"] = showOverlay;
            j["show_range_circle"] = showRangeCircle;
            nlohmann::json colors;
            colors["target"] = ColorToJson(targetColor);
            colors["range"] = ColorToJson(rangeColor);
            j["colors"] = std::move(colors);
            const std::string text = j.dump(2);
            std::ofstream out(SettingsPath(dir));
            if (out.is_open()) out << text;
        } catch (...) {}
    }
};

}
