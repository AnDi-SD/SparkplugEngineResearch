#include "wxCharacterTraits.h"
#include "Analysis/PC/wxCharacterTraitsAbi.h"

namespace winx::reconstruction
{
    namespace
    {
        using Traits = wxCharacterTraits;
        using Prefix = Traits::CapturedPrefixForAnalysis;
        using Profile = Traits::PropertyProfileForAnalysis;
        using Result = Traits::PropertyResultForAnalysis;
        namespace Abi = winx::analysis::pc::character_traits;
        static_assert(sizeof(Prefix) == Abi::CapturedPrefixBytes);

        std::uint32_t ReadWord(const Prefix& bytes, const std::size_t offset) noexcept
        {
            return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8)
                | (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
        }
        Result Admit(const Prefix& before, const Profile profile) noexcept
        {
            if (profile != Profile::PcCapturedPrimaryRaw1A4) return Result::UnqualifiedProfile;
            if (ReadWord(before, 0) != Abi::PrimaryTable) return Result::CapturedPrimaryNotAdmitted;
            return Result::Completed;
        }
    }

    wxCharacterTraits::PropertyResultForAnalysis wxCharacterTraits::ReadValueOfFairyStarsRawForAnalysis(
        const CapturedPrefixForAnalysis& before, std::uint32_t& output,
        const PropertyProfileForAnalysis profile) noexcept
    {
        const auto result = Admit(before, profile);
        if (result != PropertyResultForAnalysis::Completed) return result;
        output = ReadWord(before, Abi::RawPropertyOffset);
        return PropertyResultForAnalysis::Completed;
    }

    wxCharacterTraits::PropertyResultForAnalysis wxCharacterTraits::WriteValueOfFairyStarsRawForAnalysis(
        const CapturedPrefixForAnalysis& before, const std::uint32_t value,
        CapturedPrefixForAnalysis& output, const PropertyProfileForAnalysis profile) noexcept
    {
        const auto result = Admit(before, profile);
        if (result != PropertyResultForAnalysis::Completed) return result;
        auto staged = before;
        for (std::size_t byte = 0; byte != 4; ++byte)
            staged[Abi::RawPropertyOffset + byte] = static_cast<std::uint8_t>(value >> (byte * 8));
        output = staged;
        return PropertyResultForAnalysis::Completed;
    }
}
