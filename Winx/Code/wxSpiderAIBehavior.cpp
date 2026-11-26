#include "wxSpiderAIBehavior.h"
#include "../Analysis/PC/wxSpiderAIBehaviorAbi.h"

namespace
{
    using Behavior = wxSpiderAIBehavior;
    using Prefix = Behavior::CapturedPrefixForAnalysis;
    using Profile = Behavior::ProfileForAnalysis;
    namespace Abi = wxSpiderAIBehaviorAbi;

    static_assert(Prefix{}.size() == Abi::CapturedMinimumPrefixBytes);
    static_assert(Abi::IsSpiderByteOffset + sizeof(std::uint8_t) == Abi::CapturedMinimumPrefixBytes);

    bool Admit(const Prefix& before, Profile profile) noexcept
    {
        const auto primary = std::uint32_t(before[0]) | (std::uint32_t(before[1]) << 8)
            | (std::uint32_t(before[2]) << 16) | (std::uint32_t(before[3]) << 24);
        return profile == Profile::PcCapturedPrimaryIsSpiderRawByte1B0 && primary == Abi::CapturedPrimary;
    }
}

bool wxSpiderAIBehavior::ReadIsSpiderByteForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint8_t& output, ProfileForAnalysis profile) noexcept
{
    if (!Admit(before, profile))
        return false;
    output = before[Abi::IsSpiderByteOffset];
    return true;
}

bool wxSpiderAIBehavior::WriteIsSpiderLowByteForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t rawArgumentWord, CapturedPrefixForAnalysis& output, ProfileForAnalysis profile) noexcept
{
    if (!Admit(before, profile))
        return false;
    auto staged = before;
    staged[Abi::IsSpiderByteOffset] = std::uint8_t(rawArgumentWord);
    output = staged;
    return true;
}

bool wxSpiderAIBehavior::ComposeIsSpiderLowByteForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t rawArgumentWord, ComposedIsSpiderForAnalysis& output, ProfileForAnalysis profile) noexcept
{
    ComposedIsSpiderForAnalysis staged;
    if (!ReadIsSpiderByteForAnalysis(before, staged.beforeRaw, profile)
        || !WriteIsSpiderLowByteForAnalysis(before, rawArgumentWord, staged.afterSet, profile)
        || !ReadIsSpiderByteForAnalysis(staged.afterSet, staged.afterRaw, profile))
        return false;
    output = staged;
    return true;
}
