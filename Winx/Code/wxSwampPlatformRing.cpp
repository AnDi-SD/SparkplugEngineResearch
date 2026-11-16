#include "wxSwampPlatformRing.h"
#include "Analysis/PC/wxSwampPlatformRingAbi.h"
#include "Analysis/PC/spFloat80TowardZero.h"
#include <cstring>

namespace winx::reconstruction
{
    namespace
    {
        using Ring = wxSwampPlatformRing;
        using Prefix = Ring::CapturedPrefixForAnalysis;
        using Property = Ring::PropertyForAnalysis;
        using Profile = Ring::ProfileForAnalysis;
        using Result = Ring::ResultForAnalysis;
        namespace Abi = winx::analysis::pc::swamp_platform_ring;
        static_assert(sizeof(Prefix) == Abi::CapturedPrefixBytes);

        std::uint32_t Word(const Prefix& input, const std::size_t offset) noexcept
        {
            return std::uint32_t(input[offset]) | (std::uint32_t(input[offset + 1]) << 8)
                | (std::uint32_t(input[offset + 2]) << 16) | (std::uint32_t(input[offset + 3]) << 24);
        }
        bool AdmittedWord(const std::uint32_t word) noexcept
        {
            const auto exponent = (word >> 23) & 255;
            return exponent != 255 && (exponent != 0 || (word & 0x7FFFFF) == 0);
        }
        Result Admit(const Prefix& before, const Property property, const Profile profile,
            std::size_t& offset) noexcept
        {
            if (profile != Profile::PcCapturedPrimaryFiniteNormalOrZeroF32) return Result::UnqualifiedProfile;
            if (Word(before, 0) != Abi::PrimaryTable) return Result::CapturedPrimaryNotAdmitted;
            const auto index = static_cast<unsigned>(property);
            if (index >= Abi::ScalarOffsets.size()) return Result::UnknownProperty;
            for (const auto selected : Abi::ScalarOffsets)
                if (!AdmittedWord(Word(before, selected))) return Result::CapturedScalarNotAdmitted;
            offset = Abi::ScalarOffsets[index];
            return Result::Completed;
        }
    }

    wxSwampPlatformRing::ResultForAnalysis wxSwampPlatformRing::ReadScalarForAnalysis(
        const CapturedPrefixForAnalysis& before, const PropertyForAnalysis property,
        ExtendedReturnForAnalysis& output, const ProfileForAnalysis profile) noexcept
    {
        std::size_t offset = 0;
        const auto admitted = Admit(before, property, profile, offset);
        if (admitted != ResultForAnalysis::Completed) return admitted;
        const auto bits = Word(before, offset);
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        // Reuse the existing exact binary32 widening provider. No arithmetic,
        // nearest/spill operation or general exception model is introduced.
        const auto number = sparkplug::evidence::pc::float80_rtz::FromFloat(value);
        const auto exponent = std::uint16_t((number.negative ? 0x8000 : 0)
            | (number.mantissa ? unsigned(number.exponent + 63 + 16383) : 0));
        ExtendedReturnForAnalysis staged{};
        for (unsigned index = 0; index != 8; ++index)
            staged[index] = std::uint8_t(number.mantissa >> (index * 8));
        staged[8] = std::uint8_t(exponent); staged[9] = std::uint8_t(exponent >> 8);
        output = staged;
        return ResultForAnalysis::Completed;
    }

    wxSwampPlatformRing::ResultForAnalysis wxSwampPlatformRing::WriteScalarBitsForAnalysis(
        const CapturedPrefixForAnalysis& before, const PropertyForAnalysis property,
        const std::uint32_t rawArgument, CapturedPrefixForAnalysis& output,
        const ProfileForAnalysis profile) noexcept
    {
        std::size_t offset = 0;
        const auto admitted = Admit(before, property, profile, offset);
        if (admitted != ResultForAnalysis::Completed) return admitted;
        if (!AdmittedWord(rawArgument)) return ResultForAnalysis::ArgumentNotAdmitted;
        auto staged = before;
        for (unsigned index = 0; index != 4; ++index)
            staged[offset + index] = std::uint8_t(rawArgument >> (index * 8));
        output = staged;
        return ResultForAnalysis::Completed;
    }
}
