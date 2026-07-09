#pragma once

#include <cstdint>
#include <Windows.h>

// Cross-plugin coordination via a named shared-memory flag. Both PickMyLoot and
// AutoMyAim run inside the same fixer.exe process; PickMyLoot stamps a
// self-expiring "busy until" tick while it is moving the cursor / clicking, and
// AutoMyAim pauses aiming while that window is live. Self-expiring so a stopped
// or crashed writer never leaves the reader stuck. Identical file in both plugins.
namespace PoeFixerIpc {

inline constexpr uint32_t kMagic = 0x504D4C31; // 'PML1'

struct Shared {
    uint32_t          magic;
    volatile uint64_t pickBusyUntilTick;
};

class PickBusyChannel {
public:
    PickBusyChannel() {
        m_map = ::CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                     sizeof(Shared), L"Local\\PoeFixer_PickMyLoot_Busy");
        if (m_map) {
            m_p = static_cast<Shared*>(::MapViewOfFile(m_map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Shared)));
            if (m_p && m_p->magic != kMagic) {
                m_p->magic = kMagic;
                m_p->pickBusyUntilTick = 0;
            }
        }
    }

    ~PickBusyChannel() {
        if (m_p) ::UnmapViewOfFile(m_p);
        if (m_map) ::CloseHandle(m_map);
    }

    PickBusyChannel(const PickBusyChannel&) = delete;
    PickBusyChannel& operator=(const PickBusyChannel&) = delete;

    // Writer (PickMyLoot): keep loot-picking active for the next `ms` ms.
    void MarkBusy(uint32_t ms) {
        if (m_p) m_p->pickBusyUntilTick = ::GetTickCount64() + ms;
    }

    // Reader (AutoMyAim): is a loot-pick action currently in its window?
    bool IsBusy() const {
        return m_p && ::GetTickCount64() < m_p->pickBusyUntilTick;
    }

    bool Valid() const { return m_p != nullptr; }

private:
    HANDLE  m_map = nullptr;
    Shared* m_p = nullptr;
};

}
