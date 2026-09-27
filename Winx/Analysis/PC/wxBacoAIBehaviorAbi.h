#pragma once

#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    struct wxBacoAIBehaviorLayout final
    {
        std::uint8_t wxEntityAndBasePrefix[0x124];
        std::uint32_t currentAction;            // 0x124
        std::uint32_t savedActionKey;           // 0x128
        std::uint8_t actionMap[0x0C];           // 0x12c
        std::uint8_t actionGate;                // 0x138
        std::uint8_t unknown139[7];
        std::uint8_t inhibitSwitch;             // 0x140
        std::uint8_t unknown141[0x0C];
        std::uint8_t changeGateEnabled;         // 0x14d
        std::uint8_t unknown14E[0x3E];
        float baseValue0;                       // 0x18c: 125
        float baseValue1;                       // 0x190: 400
        float baseValue2;                       // 0x194: 700
        std::uint8_t unknown198[0x20];
        std::uint8_t bacoFlag0, bacoFlag1;     // 0x1b8/0x1b9: 1
        std::uint8_t unknown1BA[2];
        std::uint32_t bacoValue0;               // 0x1bc: 500
        std::uint32_t bacoValue1;               // 0x1c0: 1500
        std::uint8_t bacoFlag2, bacoFlag3;     // 0x1c4/0x1c5: 0
        std::uint8_t unknown1C6[2];
    };
    static_assert(sizeof(wxBacoAIBehaviorLayout) == 0x1C8);
    static_assert(offsetof(wxBacoAIBehaviorLayout, currentAction) == 0x124);
    static_assert(offsetof(wxBacoAIBehaviorLayout, changeGateEnabled) == 0x14D);
    static_assert(offsetof(wxBacoAIBehaviorLayout, bacoFlag0) == 0x1B8);
    static_assert(offsetof(wxBacoAIBehaviorLayout, bacoValue0) == 0x1BC);
    inline constexpr std::uint32_t wxBacoAIBehaviorClassID = 0x11EE770A;
    inline constexpr std::uint32_t wxBacoAIBehaviorFactory = 0x00406DA0;
    inline constexpr std::uint32_t wxBacoAIBehaviorVTable = 0x006FFB60;
    inline constexpr std::uint32_t wxBacoAIBehaviorClone = 0x0040CE00;
    inline constexpr std::uint32_t wxBacoAIBehaviorCopy = 0x00566BF0;
    inline constexpr std::uint32_t wxBacoAIBehaviorRTTIGetter = 0x00566B30;
    inline constexpr std::uint32_t wxBacoAIBehaviorOwnSlots[5] = {
        0x00567430, 0x00566E70, 0x00566F50, 0x005632B0, 0x00566D20
    }; // vtable indices 14, 15, 16, 17, 22
}
