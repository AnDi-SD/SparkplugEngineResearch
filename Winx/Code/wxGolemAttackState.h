#pragma once

#include <array>
#include <cstdint>

namespace winx::reconstruction
{
    class wxGolemAttackState
    {
    public:
        using CapturedStatePrefixForAnalysis = std::array<std::uint8_t, 0x40>;
        using CapturedCharacterPrefixForAnalysis = std::array<std::uint8_t, 0x130>;
        using CapturedRecordPrefixForAnalysis = std::array<std::uint8_t, 0x10>;

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

        // The caller supplies the captures represented by both numeric PC links.
        // Success clears the linked record's DWORD at +0x04.
        [[nodiscard]] static bool TryClearPcBorrowedRecordWordForAnalysis(
            const CapturedStatePrefixForAnalysis& state, const BorrowedCharacterForAnalysis& character,
            const BorrowedRecordForAnalysis& record) noexcept;
    };
}
