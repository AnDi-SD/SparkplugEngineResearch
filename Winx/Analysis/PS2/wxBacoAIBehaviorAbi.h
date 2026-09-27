#pragma once

#include <cstddef>
#include <cstdint>

namespace winx::evidence::ps2
{
    struct wxBacoAIBehaviorLayout final
    {
        std::uint8_t wxEntityAndBasePrefix[0x130];
        std::uint32_t currentAction;            // 0x130
        std::uint32_t savedActionKey;           // 0x134
        std::uint8_t actionMap[0x10];           // 0x138
        std::uint8_t actionGate;                // 0x148
        std::uint8_t unknown149[7];
        std::uint8_t inhibitSwitch;             // 0x150
        std::uint8_t unknown151[0x0C];
        std::uint8_t changeGateEnabled;         // 0x15d
        std::uint8_t unknown15E[0x3E];
        float baseValue0;                       // 0x19c: 125
        float baseValue1;                       // 0x1a0: 400
        float baseValue2;                       // 0x1a4: 700
        std::uint8_t unknown1A8[0x20];
        std::uint8_t bacoFlag0, bacoFlag1;     // 0x1c8/0x1c9: 1
        std::uint8_t unknown1CA[2];
        std::uint32_t bacoValue0;               // 0x1cc: 500
        std::uint32_t bacoValue1;               // 0x1d0: 1500
        std::uint8_t bacoFlag2, bacoFlag3;     // 0x1d4/0x1d5: 0
        std::uint8_t unknown1D6[0x0A];
    };
    static_assert(sizeof(wxBacoAIBehaviorLayout) == 0x1E0);
    static_assert(offsetof(wxBacoAIBehaviorLayout, currentAction) == 0x130);
    static_assert(offsetof(wxBacoAIBehaviorLayout, changeGateEnabled) == 0x15D);
    static_assert(offsetof(wxBacoAIBehaviorLayout, bacoFlag0) == 0x1C8);
    static_assert(offsetof(wxBacoAIBehaviorLayout, bacoValue0) == 0x1CC);
    inline constexpr std::uint32_t wxBacoAIBehaviorClassID = 0x11EE770A;
    inline constexpr std::uint32_t wxBacoAIBehaviorFactory = 0x003E8D70;
    inline constexpr std::uint32_t wxBacoAIBehaviorConstructor = 0x00228E30;
    inline constexpr std::uint32_t wxBacoAIBehaviorVTable = 0x004961C0;
    inline constexpr std::uint32_t wxBacoAIBehaviorClone = 0x003E8CB0;
    inline constexpr std::uint32_t wxBacoAIBehaviorCopy = 0x00227CC0;
    inline constexpr std::uint32_t wxBacoAIBehaviorRTTIGetter = 0x003F89C0;
    inline constexpr std::uint32_t wxBacoAIBehaviorOwnSlots[5] = {
        0x00228960, 0x002285D0, 0x00228400, 0x00227820, 0x00228740
    }; // source-level indices 14, 15, 16, 17, 22; skip PS2 ABI header
}
