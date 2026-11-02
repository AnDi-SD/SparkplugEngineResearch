#pragma once
#include <cstddef>
#include <cstdint>

namespace winx::evidence::pc
{
    // Captured foreign record, not a declaration of a named native class.
    struct ForeignCommand703090Layout final
    {
        std::uint32_t primary;
        std::uint8_t rawBytes04[0x70];
    };
    static_assert(sizeof(ForeignCommand703090Layout) == 0x74);
    static_assert(offsetof(ForeignCommand703090Layout, rawBytes04) == 4);
    inline constexpr std::uint32_t ForeignCommandConstructor = 0x599430;
    inline constexpr std::uint32_t ForeignCommandPrimary = 0x703090;
    inline constexpr std::uint32_t ForeignCommandReset = 0x5992E0;
    inline constexpr std::uint32_t ForeignCommandResetCellOffset = 4;
    // Destinations consumed by actual Mikael Entry/self48; analytical raw
    // field storage APIs do not introduce native callable/ownership claims.
    inline constexpr std::size_t ForeignCommandRawWord04Offset = 0x04;
    inline constexpr std::size_t ForeignCommandRawWord08Offset = 0x08;
    inline constexpr std::size_t ForeignCommandRawByte1BOffset = 0x1B;
}
