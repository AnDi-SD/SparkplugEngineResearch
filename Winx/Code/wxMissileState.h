#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace winx::reconstruction
{
    class wxMissileState
    {
    public:
        using CapturedStatePrefixForAnalysis = std::array<std::uint8_t, 0x40>;

        // Returns raw AL from the PC slot +0x34. The DWORD argument is ignored.
        // A foreign primary returns an empty result.
        [[nodiscard]] static std::optional<std::uint8_t> ReadPcPermissionByteForAnalysis(
            const CapturedStatePrefixForAnalysis& state, std::uint32_t rawArgument) noexcept;
    };
}
