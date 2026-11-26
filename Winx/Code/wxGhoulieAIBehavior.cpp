#include "wxGhoulieAIBehavior.h"
#include "../Analysis/PC/wxGhoulieAIBehaviorAbi.h"

namespace
{
    using Behavior = wxGhoulieAIBehavior;
    using Prefix = Behavior::CapturedPrefixForAnalysis;
    using Profile = Behavior::ProfileForAnalysis;
    using Property = Behavior::PropertyForAnalysis;
    namespace Abi = wxGhoulieAIBehaviorAbi;

    static_assert(Prefix{}.size() == Abi::CapturedMinimumPrefixBytes);
    static_assert(Abi::IsInGardenia01Offset + 1 == Abi::JumpingGhoulieOffset);
    static_assert(Abi::JumpingGhoulieOffset + 1 == Abi::CapturedMinimumPrefixBytes);

    std::size_t PropertyOffset(Property property) noexcept
    {
        switch (property)
        {
            case Property::IsInGardenia01: return Abi::IsInGardenia01Offset;
            case Property::JumpingGhoulie: return Abi::JumpingGhoulieOffset;
        }
        return Abi::CapturedMinimumPrefixBytes;
    }

    bool Admit(const Prefix& before, Profile profile, std::size_t offset) noexcept
    {
        const auto primary = std::uint32_t(before[0]) | (std::uint32_t(before[1]) << 8)
            | (std::uint32_t(before[2]) << 16) | (std::uint32_t(before[3]) << 24);
        return profile == Profile::PcCapturedPrimaryRawBytes1C4And1C5
            && primary == Abi::CapturedPrimary && offset < before.size();
    }
}

bool wxGhoulieAIBehavior::ReadRawByteForAnalysis(const CapturedPrefixForAnalysis& before,
    PropertyForAnalysis property, std::uint8_t& output, ProfileForAnalysis profile) noexcept
{
    const auto offset = PropertyOffset(property);
    if (!Admit(before, profile, offset))
        return false;
    output = before[offset];
    return true;
}

bool wxGhoulieAIBehavior::WriteLowByteForAnalysis(const CapturedPrefixForAnalysis& before,
    PropertyForAnalysis property, std::uint32_t argument, CapturedPrefixForAnalysis& output,
    ProfileForAnalysis profile) noexcept
{
    const auto offset = PropertyOffset(property);
    if (!Admit(before, profile, offset))
        return false;
    auto staged = before;
    staged[offset] = std::uint8_t(argument);
    output = staged;
    return true;
}

bool wxGhoulieAIBehavior::ComposeLowByteForAnalysis(const CapturedPrefixForAnalysis& before,
    PropertyForAnalysis property, std::uint32_t argument, ComposedRawByteForAnalysis& output,
    ProfileForAnalysis profile) noexcept
{
    ComposedRawByteForAnalysis staged;
    if (!ReadRawByteForAnalysis(before, property, staged.beforeRaw, profile)
        || !WriteLowByteForAnalysis(before, property, argument, staged.afterSet, profile)
        || !ReadRawByteForAnalysis(staged.afterSet, property, staged.afterRaw, profile))
        return false;
    output = staged;
    return true;
}
