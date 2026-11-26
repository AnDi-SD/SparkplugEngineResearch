#include "wxStudentsStateMachine.h"
#include "../Analysis/PC/wxStudentsStateMachineAbi.h"

namespace
{
    using Machine = wxStudentsStateMachine;
    using Prefix = Machine::CapturedPrefixForAnalysis;
    using Profile = Machine::ProfileForAnalysis;
    namespace Abi = wxStudentsStateMachineAbi;

    static_assert(Prefix{}.size() == Abi::CapturedMinimumPrefixBytes);
    static_assert(Abi::TrollFightByteOffset + sizeof(std::uint8_t) == Abi::CapturedMinimumPrefixBytes);

    bool Admit(const Prefix& before, Profile profile) noexcept
    {
        const auto primary = std::uint32_t(before[0]) | (std::uint32_t(before[1]) << 8)
            | (std::uint32_t(before[2]) << 16) | (std::uint32_t(before[3]) << 24);
        return profile == Profile::PcCapturedPrimaryTrollFightRawByte284 && primary == Abi::CapturedPrimary;
    }
}

bool wxStudentsStateMachine::ReadTrollFightByteForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint8_t& output, ProfileForAnalysis profile) noexcept
{
    if (!Admit(before, profile))
        return false;
    output = before[Abi::TrollFightByteOffset];
    return true;
}

bool wxStudentsStateMachine::WriteTrollFightLowByteForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t rawArgumentWord, CapturedPrefixForAnalysis& output, ProfileForAnalysis profile) noexcept
{
    if (!Admit(before, profile))
        return false;
    auto staged = before;
    staged[Abi::TrollFightByteOffset] = std::uint8_t(rawArgumentWord);
    output = staged;
    return true;
}

bool wxStudentsStateMachine::ComposeTrollFightLowByteForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t rawArgumentWord, ComposedTrollFightForAnalysis& output, ProfileForAnalysis profile) noexcept
{
    ComposedTrollFightForAnalysis staged;
    if (!ReadTrollFightByteForAnalysis(before, staged.beforeRaw, profile)
        || !WriteTrollFightLowByteForAnalysis(before, rawArgumentWord, staged.afterSet, profile)
        || !ReadTrollFightByteForAnalysis(staged.afterSet, staged.afterRaw, profile))
        return false;
    output = staged;
    return true;
}
