#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "sdk/PluginSDK.h"

#include "config/Settings.h"
#include "game/AimTypes.h"
#include "game/RayCaster.h"
#include "game/Aim.h"
#include "game/Gates.h"

#include "ipc/Coordination.h"
#include "input/Win32Input.h"

#include <imgui.h>

inline constexpr const char* kAutoMyAimVersion    = "1.0.1";
inline constexpr const char* kAutoMyAimMaintainer = "Omer Faruk ARPA";

using AutoMyAimConfig::Settings;
using Clock = std::chrono::steady_clock;

static long long MsSince(Clock::time_point t) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t).count();
}

class AutoMyAimPlugin : public PluginSDK::Plugin {
public:
    const char* GetName() const override { return "AutoMyAim"; }

    bool WantsOverlay() const override {
        return m_settings.enabled && (m_settings.showOverlay || m_settings.showRangeCircle);
    }

    void OnEnable(bool) override {
        if (!HostCompatible()) {
            ctx()->Log.Error("AutoMyAim: incompatible PoeFixer host (SDK mismatch) - disabled");
            return;
        }
        if (ctx()->ImGuiContext)
            ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));

        m_settings.Load(DirectoryPath());
        m_rng = static_cast<uint32_t>(::GetTickCount()) | 1u;
        m_rc.Refresh(ctx());
        m_areaCounter = ctx()->Game.GetAreaChangeCounter();
        m_lastAction = Clock::now() - std::chrono::milliseconds(1000);

        auto& events = const_cast<PluginSDK::EventsService&>(ctx()->Events);
        m_frameTok = events.OnFrame([this] { FrameTick(); });
        m_areaTok  = events.OnAreaChange([this] { m_rc.Refresh(ctx()); m_hasTarget = false; });
        ctx()->Log.Info("AutoMyAim plugin enabled");
    }

    void OnDisable() override {
        auto& events = const_cast<PluginSDK::EventsService&>(ctx()->Events);
        if (m_frameTok.Valid()) events.Unsubscribe(m_frameTok);
        if (m_areaTok.Valid())  events.Unsubscribe(m_areaTok);
        m_frameTok = {};
        m_areaTok = {};
        m_hasTarget = false;
        m_active = false;
        SaveSettings();
        ctx()->Log.Info("AutoMyAim plugin disabled");
    }

    void SaveSettings() override { m_settings.Save(DirectoryPath()); }

    void DrawUI() override {
        if (!m_settings.enabled) return;
        if (!m_settings.showOverlay && !m_settings.showRangeCircle) return;
        if (ctx()->ImGuiContext)
            ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));
        if (!ctx()->Game.IsInGame() || !ctx()->Game.IsForeground()) return;

        ImDrawList* dl = ImGui::GetForegroundDrawList();

        if (m_settings.showRangeCircle) DrawRangeCircle(dl);

        if (m_settings.showOverlay) {
            if (m_hasTarget && m_active) {
                const ImU32 col = ImGui::ColorConvertFloat4ToU32(
                    ImVec4(m_settings.targetColor[0], m_settings.targetColor[1],
                           m_settings.targetColor[2], m_settings.targetColor[3]));
                const ImVec2 c(m_curTarget.screenX, m_curTarget.screenY);
                dl->AddCircle(c, 14.f, col, 20, 2.5f);
                dl->AddCircleFilled(c, 4.f, col, 12);
            }
            char status[96];
            std::snprintf(status, sizeof(status), "AutoMyAim: %s%s%s",
                          m_active ? "AIMING" : "idle",
                          m_status.empty() ? "" : " | ", m_status.c_str());
            const ImVec2 pos(14.f, 34.f);
            const ImVec2 sz = ImGui::CalcTextSize(status);
            dl->AddRectFilled(ImVec2(pos.x - 4, pos.y - 2),
                              ImVec2(pos.x + sz.x + 4, pos.y + sz.y + 2), IM_COL32(0, 0, 0, 200), 3.f);
            dl->AddText(pos, m_active ? IM_COL32(240, 120, 120, 255) : IM_COL32(210, 210, 210, 255),
                        status);
        }
    }

    void DrawSettings() override {
        if (ctx()->ImGuiContext)
            ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));

        ImGui::TextDisabled("AutoMyAim v%s  -  by %s", kAutoMyAimVersion, kAutoMyAimMaintainer);
        ImGui::Checkbox("Enable AutoMyAim", &m_settings.enabled);
        ImGui::SameLine();
        ImGui::TextColored(m_active ? ImVec4(0.95f, 0.4f, 0.4f, 1.f) : ImVec4(0.7f, 0.7f, 0.7f, 1.f),
                           m_active ? "[AIMING]" : "[idle]");
        ImGui::TextWrapped(
            "Holds/keys move the cursor onto the best nearby monster (line-of-sight "
            "checked) so your skills aim at it. It only moves the cursor - you still cast.");

        ImGui::SeparatorText("Keys");
        DrawKeyBinder("Aim (hold)", m_settings.aimKey);
        DrawKeyBinder("Toggle (always-on)", m_settings.toggleKey);

        ImGui::SeparatorText("Targeting");
        ImGui::SliderInt("Aim range", &m_settings.aimRange,
                         AutoMyAimConfig::kAimRangeMin, AutoMyAimConfig::kAimRangeMax);
        ImGui::Checkbox("Show range circle", &m_settings.showRangeCircle);
        ImGui::SameLine();
        ImGui::ColorEdit4("Circle color", m_settings.rangeColor,
                          ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
        ImGui::SameLine();
        ImGui::TextDisabled("(adjust the slider and watch the ring)");
        ImGui::SliderInt("Action interval (ms, 0=every frame)", &m_settings.actionIntervalMs,
                         AutoMyAimConfig::kActionIntervalMinMs, AutoMyAimConfig::kActionIntervalMaxMs);
        ImGui::Checkbox("Line-of-sight (skip targets behind walls)", &m_settings.useLineOfSight);
        if (m_settings.useLineOfSight) {
            ImGui::SetNextItemWidth(120.f);
            ImGui::SliderInt("LoS block below", &m_settings.losBlockBelow, 0,
                             AutoMyAimConfig::kLosBlockBelowMax);
        }

        ImGui::SeparatorText("Weighting");
        ImGui::SliderFloat("Distance weight", &m_settings.distanceWeight, 0.f, 50.f, "%.1f");
        ImGui::Checkbox("Prefer rarer monsters", &m_settings.rarityWeighting);
        if (m_settings.rarityWeighting) {
            ImGui::SliderFloat("Magic", &m_settings.wMagic, 0.f, 100.f, "%.0f");
            ImGui::SliderFloat("Rare", &m_settings.wRare, 0.f, 100.f, "%.0f");
            ImGui::SliderFloat("Unique", &m_settings.wUnique, 0.f, 100.f, "%.0f");
        }
        ImGui::Checkbox("Weight by HP", &m_settings.hpWeighting);
        if (m_settings.hpWeighting) {
            ImGui::SameLine();
            ImGui::Checkbox("prefer higher HP", &m_settings.preferHigherHp);
            ImGui::SliderFloat("HP weight", &m_settings.hpWeight, 0.f, 100.f, "%.0f");
        }

        ImGui::SeparatorText("Cursor");
        ImGui::Checkbox("Confine to a circle around the player", &m_settings.confineToCircle);
        if (m_settings.confineToCircle) {
            ImGui::SliderInt("Circle radius (px)", &m_settings.circleRadius,
                             AutoMyAimConfig::kCircleRadiusMin, AutoMyAimConfig::kCircleRadiusMax);
        }
        ImGui::Checkbox("Randomize a little", &m_settings.randomize);
        if (m_settings.randomize) {
            ImGui::SliderInt("Randomize radius (px)", &m_settings.randomizeRadius, 0,
                             AutoMyAimConfig::kRandomizeRadiusMax);
        }

        ImGui::SeparatorText("Safety");
        ImGui::Checkbox("Stop when an item panel is open", &m_settings.gatePanelOpen);
        ImGui::Checkbox("Pause while PickMyLoot is picking", &m_settings.pauseWhilePicking);
        ImGui::TextDisabled(m_pickBusy.Valid() ? "  (shared with PickMyLoot: connected)"
                                               : "  (shared channel unavailable)");

        ImGui::SeparatorText("Display");
        ImGui::Checkbox("Show status + target marker", &m_settings.showOverlay);
        ImGui::SameLine();
        ImGui::ColorEdit4("Target color", m_settings.targetColor,
                          ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
    }

private:
    Settings                        m_settings;
    AutoMyAim::AimEngine            m_aim;
    AutoMyAim::RayCaster            m_rc;
    PoeFixerIpc::PickBusyChannel    m_pickBusy;
    PluginSDK::EventsService::Token m_frameTok;
    PluginSDK::EventsService::Token m_areaTok;

    bool  m_active = false;
    bool  m_toggled = false;
    bool  m_togglePrev = false;
    int*  m_bindTarget = nullptr;

    bool  m_hasTarget = false;
    AutoMyAim::Target m_curTarget;
    std::string m_status;

    Clock::time_point m_lastAction{};
    uint64_t m_areaCounter = 0;

    bool  m_panelOpen = false;
    Clock::time_point m_lastPanelCheck{};

    uint32_t m_rng = 0x1234567u;
    float Rnd01() {
        m_rng ^= m_rng << 13; m_rng ^= m_rng >> 17; m_rng ^= m_rng << 5;
        return static_cast<float>(m_rng) / static_cast<float>(0xFFFFFFFFu);
    }

    void PollToggle(bool foreground) {
        if (m_bindTarget) { m_togglePrev = false; return; }
        const bool down = foreground && AutoMyAimInput::IsKeyDown(m_settings.toggleKey);
        if (down && !m_togglePrev) m_toggled = !m_toggled;
        m_togglePrev = down;
    }

    bool PanelOpenCached() {
        if (MsSince(m_lastPanelCheck) >= 200) {
            m_lastPanelCheck = Clock::now();
            m_panelOpen = AutoMyAim::AnyItemPanelOpen(ctx());
        }
        return m_panelOpen;
    }

    void FrameTick() {
        if (!HostCompatible()) return;
        if (!m_settings.enabled) { m_active = false; return; }

        const bool foreground = ctx()->Game.IsForeground();
        PollToggle(foreground);

        m_active = m_toggled || (foreground && AutoMyAimInput::IsKeyDown(m_settings.aimKey));
        if (!m_active) { m_status = "idle"; m_hasTarget = false; return; }
        if (!ctx()->Game.IsInGame() || !foreground) { m_hasTarget = false; return; }

        const uint64_t ac = ctx()->Game.GetAreaChangeCounter();
        if (ac != m_areaCounter) { m_areaCounter = ac; m_rc.Refresh(ctx()); }

        if (m_settings.actionIntervalMs > 0 && MsSince(m_lastAction) < m_settings.actionIntervalMs)
            return;
        m_lastAction = Clock::now();

        if (m_settings.pauseWhilePicking && m_pickBusy.IsBusy()) {
            m_status = "paused (picking)"; m_hasTarget = false; return;
        }
        if (m_settings.gatePanelOpen && PanelOpenCached()) {
            m_status = "panel open"; m_hasTarget = false; return;
        }

        const PluginSDK::ScreenSize disp = ctx()->Game.GetScreenSize();
        const AutoMyAim::Target* best = m_aim.Compute(ctx(), m_settings, m_rc, disp.Width, disp.Height);
        if (!best) { m_status = "no target"; m_hasTarget = false; return; }

        float ax = best->screenX, ay = best->screenY;

        if (m_settings.confineToCircle && m_aim.HavePlayer()) {
            float psx = 0.f, psy = 0.f;
            if (ctx()->Render.WorldToScreen(m_aim.PlayerWorldX(), m_aim.PlayerWorldY(),
                                            m_aim.PlayerWorldZ(), psx, psy)) {
                const float ddx = ax - psx, ddy = ay - psy;
                const float d = std::sqrt(ddx * ddx + ddy * ddy);
                const float r = static_cast<float>(m_settings.circleRadius);
                if (d > r && d > 0.f) { ax = psx + ddx / d * r; ay = psy + ddy / d * r; }
            }
        }

        if (m_settings.randomize && m_settings.randomizeRadius > 0) {
            const float ang = Rnd01() * 6.2831853f;
            const float rad = Rnd01() * static_cast<float>(m_settings.randomizeRadius);
            ax += std::cos(ang) * rad;
            ay += std::sin(ang) * rad;
        }

        const float margin = 4.f;
        ax = std::clamp(ax, margin, disp.Width - margin);
        ay = std::clamp(ay, margin, disp.Height - margin);

        AutoMyAimInput::MoveCursorScreen(static_cast<int>(ax), static_cast<int>(ay));
        m_curTarget = *best;
        m_hasTarget = true;
        m_status = "aiming";
    }

    void DrawRangeCircle(ImDrawList* dl) {
        const PluginSDK::Entity p = ctx()->Entities.GetPlayer();
        if (!p.IsValid) return;
        const double gm = std::sqrt(static_cast<double>(p.GridPositionX) * p.GridPositionX +
                                    static_cast<double>(p.GridPositionY) * p.GridPositionY);
        const double wm = std::sqrt(static_cast<double>(p.WorldX) * p.WorldX +
                                    static_cast<double>(p.WorldY) * p.WorldY);
        const float k = (gm > 1.0) ? static_cast<float>(wm / gm) : 10.8696f;
        const float R = static_cast<float>(m_settings.aimRange) * k;
        const ImU32 col = ImGui::ColorConvertFloat4ToU32(
            ImVec4(m_settings.rangeColor[0], m_settings.rangeColor[1],
                   m_settings.rangeColor[2], m_settings.rangeColor[3]));
        constexpr int N = 48;
        ImVec2 pts[N];
        int n = 0;
        for (int i = 0; i < N; ++i) {
            const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(N);
            float sx = 0.f, sy = 0.f;
            if (ctx()->Render.WorldToScreen(p.WorldX + R * std::cos(a), p.WorldY + R * std::sin(a),
                                            p.WorldZ, sx, sy))
                pts[n++] = ImVec2(sx, sy);
        }
        if (n >= 3) dl->AddPolyline(pts, n, col, ImDrawFlags_Closed, 2.f);
    }

    void DrawKeyBinder(const char* label, int& vk) {
        ImGui::PushID(label);
        if (m_bindTarget == &vk) {
            ImGui::Button("Press a key...  (Esc to cancel)", ImVec2(220.f, 0.f));
            if (AutoMyAimInput::IsKeyDown(VK_ESCAPE)) {
                m_bindTarget = nullptr;
            } else {
                for (int i = 4; i < 255; ++i) {
                    if (i == VK_RBUTTON) continue;
                    if ((GetAsyncKeyState(i) & 0x8000) != 0) { vk = i; m_bindTarget = nullptr; break; }
                }
            }
        } else {
            char btn[64];
            std::snprintf(btn, sizeof(btn), "%s", VkName(vk).c_str());
            if (ImGui::Button(btn, ImVec2(220.f, 0.f))) m_bindTarget = &vk;
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(label);
        ImGui::PopID();
    }

    static std::string VkName(int vk) {
        switch (vk) {
            case VK_LBUTTON:  return "Mouse1";
            case VK_RBUTTON:  return "Mouse2";
            case VK_MBUTTON:  return "Mouse3";
            case VK_XBUTTON1: return "Mouse4";
            case VK_XBUTTON2: return "Mouse5";
            default: break;
        }
        UINT sc = MapVirtualKeyA(static_cast<UINT>(vk), MAPVK_VK_TO_VSC);
        char name[64] = {0};
        if (sc != 0 && GetKeyNameTextA(static_cast<LONG>(sc << 16), name, sizeof(name)) > 0)
            return std::string(name);
        char fallback[16];
        std::snprintf(fallback, sizeof(fallback), "0x%02X", vk);
        return std::string(fallback);
    }
};

extern "C" PLUGIN_API PluginSDK::Plugin* CreatePlugin() { return new AutoMyAimPlugin(); }
extern "C" PLUGIN_API void DestroyPlugin(PluginSDK::Plugin* p) { delete p; }
