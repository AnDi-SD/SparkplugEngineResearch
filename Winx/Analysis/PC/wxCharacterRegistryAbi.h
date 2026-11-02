#pragma once
#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    // Exact captured extent/selected fields only. The opaque prefix deliberately
    // makes no assertion about native physical C++ inheritance or singleton ABI.
    struct wxCharacterRegistryCapturedLayout final
    {
        std::uint8_t opaquePrefix[0x18];
        std::uint32_t borrowedGroupsBegin18;
        std::uint32_t borrowedGroupsEnd1C;
        std::uint32_t opaqueCapacity20;
        std::uint8_t opaque24[4];
        std::uint32_t cachedTarget28;
    };
    static_assert(sizeof(wxCharacterRegistryCapturedLayout) == 0x2C);
    static_assert(offsetof(wxCharacterRegistryCapturedLayout, borrowedGroupsBegin18) == 0x18);
    static_assert(offsetof(wxCharacterRegistryCapturedLayout, cachedTarget28) == 0x28);
    inline constexpr std::uint32_t wxCharacterRegistryClassID = 0x6EAA030D;
    inline constexpr std::uint32_t wxCharacterRegistryHistoricalFactory = 0x4011D0;
    inline constexpr std::uint32_t wxCharacterRegistryVTable = 0x6F5488;
    inline constexpr std::uint32_t wxCharacterRegistryRTTIGetter = 0x4E2280;
    inline constexpr std::uint32_t wxCharacterRegistryRTTIRecord = 0x74E188;
    inline constexpr std::uint32_t wxCharacterRegistryCapturedSelector = 0x4E21D0;
}
