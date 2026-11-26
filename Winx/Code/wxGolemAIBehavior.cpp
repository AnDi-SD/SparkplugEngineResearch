#include "wxGolemAIBehavior.h"
#include "../Analysis/PC/wxGolemAIBehaviorAbi.h"

namespace
{
    using Behavior = wxGolemAIBehavior;
    using Prefix = Behavior::CapturedPrefixForAnalysis;
    using Profile = Behavior::ProfileForAnalysis;
    namespace Abi = wxGolemAIBehaviorAbi;

    static_assert(Prefix{}.size() == Abi::CapturedMinimumPrefixBytes);
    static_assert(Abi::StunnedTimeRawOffset + sizeof(std::uint32_t) == Abi::CapturedMinimumPrefixBytes);

    std::uint32_t ReadWord(const Prefix& prefix, std::size_t offset) noexcept
    {
        return std::uint32_t(prefix[offset]) | (std::uint32_t(prefix[offset + 1]) << 8)
            | (std::uint32_t(prefix[offset + 2]) << 16) | (std::uint32_t(prefix[offset + 3]) << 24);
    }

    bool Admit(const Prefix& before, Profile profile) noexcept
    {
        return profile == Profile::PcCapturedPrimaryStunnedTimeRawDword1D8
            && ReadWord(before, 0) == Abi::CapturedPrimary;
    }
}

bool wxGolemAIBehavior::ReadStunnedTimeRawForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t& output, ProfileForAnalysis profile) noexcept
{
    if (!Admit(before, profile))
        return false;
    output = ReadWord(before, Abi::StunnedTimeRawOffset);
    return true;
}

bool wxGolemAIBehavior::WriteStunnedTimeRawForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t rawArgument, CapturedPrefixForAnalysis& output, ProfileForAnalysis profile) noexcept
{
    if (!Admit(before, profile))
        return false;
    auto staged = before;
    for (unsigned byte = 0; byte < 4; ++byte)
        staged[Abi::StunnedTimeRawOffset + byte] = std::uint8_t(rawArgument >> (8 * byte));
    output = staged;
    return true;
}

bool wxGolemAIBehavior::ComposeStunnedTimeRawForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t rawArgument, ComposedStunnedTimeForAnalysis& output, ProfileForAnalysis profile) noexcept
{
    ComposedStunnedTimeForAnalysis staged;
    if (!ReadStunnedTimeRawForAnalysis(before, staged.beforeRaw, profile)
        || !WriteStunnedTimeRawForAnalysis(before, rawArgument, staged.afterSet, profile)
        || !ReadStunnedTimeRawForAnalysis(staged.afterSet, staged.afterRaw, profile))
        return false;
    output = staged;
    return true;
}
