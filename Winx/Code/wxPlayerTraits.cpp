#include "wxPlayerTraits.h"
#include "Analysis/PC/wxPlayerTraitsAbi.h"

namespace winx::reconstruction
{
    namespace
    {
        void WriteWord(wxPlayerTraits::CapturedPrefixForAnalysis& bytes,
            const std::size_t offset, const std::uint32_t value) noexcept
        {
            for (unsigned byte = 0; byte != 4; ++byte)
                bytes[offset + byte] = static_cast<std::uint8_t>(value >> (byte * 8));
        }

        std::uint32_t ReadPrimary(const wxPlayerTraits::CapturedPrefixForAnalysis& bytes) noexcept
        {
            std::uint32_t value = 0;
            for (unsigned byte = 0; byte != 4; ++byte)
                value |= std::uint32_t{bytes[byte]} << (byte * 8);
            return value;
        }
    }

    wxPlayerTraits::ResetResultForAnalysis wxPlayerTraits::ResetCapturedPrefixForAnalysis(
        const CapturedPrefixForAnalysis& before, CapturedPrefixForAnalysis& output,
        const ResetProfileForAnalysis profile) noexcept
    {
        if (profile != ResetProfileForAnalysis::PcCapturedPrimary3CReset)
            return ResetResultForAnalysis::UnqualifiedProfile;
        if (ReadPrimary(before) != evidence::pc::wxPlayerTraitsPrimary)
            return ResetResultForAnalysis::CapturedPrimaryNotAdmitted;

        static_assert(CapturedPrefixBytesForAnalysis == evidence::pc::wxPlayerTraitsResetPrefixBytes);
        auto staged = before;
        WriteWord(staged, 0x124, 6);
        WriteWord(staged, 0x128, 6);
        WriteWord(staged, 0x12C, 0);
        WriteWord(staged, 0x130, 0);
        WriteWord(staged, 0x134, 0);
        staged[0x1A0] = 0;
        WriteWord(staged, 0x1A8, 0x42C80000);
        WriteWord(staged, 0x1AC, 0x42C80000);
        WriteWord(staged, 0x1B0, 0);
        WriteWord(staged, 0x1B4, 0);
        WriteWord(staged, 0x1BC, 0x42A00000);
        WriteWord(staged, 0x1B8, 0);
        WriteWord(staged, 0x1C8, 0);
        WriteWord(staged, 0x1C4, 0);
        WriteWord(staged, 0x1C0, 0);
        staged[0x1D0] = 0;
        WriteWord(staged, 0x230, 0);
        WriteWord(staged, 0x234, 0);
        WriteWord(staged, 0x238, 0x3F800000);
        WriteWord(staged, 0x240, 0);
        WriteWord(staged, 0x23C, 0);
        staged[0x24C] = 0;
        WriteWord(staged, 0x248, 0);
        output = staged;
        return ResetResultForAnalysis::Completed;
    }
}
