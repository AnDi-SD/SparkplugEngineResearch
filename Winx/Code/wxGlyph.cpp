#include "wxGlyph.h"
#include "Analysis/PC/wxGlyphAbi.h"
#include "Analysis/PC/spFloat80TowardZero.h"
#include <cstring>

namespace winx::reconstruction
{
    namespace
    {
        using Glyph = wxGlyph;
        using Prefix = Glyph::CapturedPrefixForAnalysis;
        using Profile = Glyph::ProfileForAnalysis;
        using Result = Glyph::ResultForAnalysis;
        namespace Abi = winx::analysis::pc::glyph_facing;
        static_assert(sizeof(Prefix) == Abi::CapturedPrefixBytes);
        std::uint32_t Word(const Prefix& input, const std::size_t offset) noexcept
        {
            return std::uint32_t(input[offset]) | (std::uint32_t(input[offset + 1]) << 8)
                | (std::uint32_t(input[offset + 2]) << 16) | (std::uint32_t(input[offset + 3]) << 24);
        }
        Result Admit(const Prefix& input, const Profile profile) noexcept
        {
            if (profile != Profile::PcCapturedPrimaryFacingFiniteNormalOrZeroF32NoOpSetter) return Result::UnqualifiedProfile;
            if (Word(input, 0) != Abi::PrimaryTable) return Result::CapturedPrimaryNotAdmitted;
            return Result::Completed;
        }
    }
    wxGlyph::ResultForAnalysis wxGlyph::ReadFacingForAnalysis(const CapturedPrefixForAnalysis& before,
        ExtendedReturnForAnalysis& output, const ProfileForAnalysis profile) noexcept
    {
        const auto admitted = Admit(before, profile);
        if (admitted != ResultForAnalysis::Completed) return admitted;
        const auto bits = Word(before, Abi::FacingOffset);
        const auto exponent32 = (bits >> 23) & 255;
        if (exponent32 == 255 || (exponent32 == 0 && (bits & 0x7FFFFF) != 0))
            return ResultForAnalysis::CapturedFacingNotAdmitted;
        float value; std::memcpy(&value, &bits, sizeof(value));
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
    wxGlyph::ResultForAnalysis wxGlyph::ApplyFacingNoOpForAnalysis(const CapturedPrefixForAnalysis& before,
        const std::uint32_t unusedRawArgument, CapturedPrefixForAnalysis& output, const ProfileForAnalysis profile) noexcept
    {
        const auto admitted = Admit(before, profile);
        if (admitted != ResultForAnalysis::Completed) return admitted;
        (void)unusedRawArgument;
        auto staged = before;
        output = staged;
        return ResultForAnalysis::Completed;
    }
    wxGlyph::ResultForAnalysis wxGlyph::ComposeFacingForAnalysis(const CapturedPrefixForAnalysis& before,
        const std::uint32_t unusedRawArgument, ComposedFacingForAnalysis& output, const ProfileForAnalysis profile) noexcept
    {
        ComposedFacingForAnalysis staged{};
        auto result = ReadFacingForAnalysis(before, staged.before, profile);
        if (result != ResultForAnalysis::Completed) return result;
        result = ApplyFacingNoOpForAnalysis(before, unusedRawArgument, staged.afterNoOp, profile);
        if (result != ResultForAnalysis::Completed) return result;
        result = ReadFacingForAnalysis(staged.afterNoOp, staged.after, profile);
        if (result != ResultForAnalysis::Completed) return result;
        output = staged;
        return ResultForAnalysis::Completed;
    }
}
