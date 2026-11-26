#pragma once

#include <array>
#include <cstdint>

namespace winx::reconstruction
{
    class wxLadderState
    {
    public:
        using CapturedStatePrefixForAnalysis = std::array<std::uint8_t, 0x3C>;
        using CapturedCharacterPrefixForAnalysis = std::array<std::uint8_t, 0x130>;
        using CapturedRecordPrefixForAnalysis = std::array<std::uint8_t, 0x62>;

        struct BorrowedCharacterForAnalysis
        {
            std::uint32_t capturedAddress = 0;
            const CapturedCharacterPrefixForAnalysis* prefix = nullptr;
        };

        struct BorrowedRecordForAnalysis
        {
            std::uint32_t capturedAddress = 0;
            CapturedRecordPrefixForAnalysis* prefix = nullptr;
        };

        // Exact mode 39 uses both supplied captures through their numeric address links.
        [[nodiscard]] static bool TryApplyPcTransitionPermissionForAnalysis(
            const CapturedStatePrefixForAnalysis& state, std::uint32_t rawTarget,
            const BorrowedCharacterForAnalysis& character, const BorrowedRecordForAnalysis& record,
            std::uint8_t& al) noexcept;
    };
}
