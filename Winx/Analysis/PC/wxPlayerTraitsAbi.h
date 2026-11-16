#pragma once

#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    inline constexpr std::uint32_t wxPlayerTraitsClassID = 0x28C56217;
    inline constexpr std::uint32_t wxPlayerTraitsRegistrationBaseID = 0x1BC6108E;
    inline constexpr std::uint32_t wxPlayerTraitsPrimary = 0x006F6990;
    inline constexpr std::uint32_t wxPlayerTraitsRTTIGetter = 0x004FE040;
    inline constexpr std::uint32_t wxPlayerTraitsRTTIRecord = 0x0074E908;
    inline constexpr std::uint32_t wxPlayerTraitsResetMethod = 0x004FD7E0;
    inline constexpr std::size_t wxPlayerTraitsResetMethodBytes = 159;
    inline constexpr std::size_t wxPlayerTraitsResetSlot = 0x3C;
    inline constexpr std::size_t wxPlayerTraitsResetPrefixBytes = 0x24D;
    // Prefix extent follows the last observed byte store at24C. It defines
    // the supplied analytical buffer. Full native sizeof remains unknown.
}
