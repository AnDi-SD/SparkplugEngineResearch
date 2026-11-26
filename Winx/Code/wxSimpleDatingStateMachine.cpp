#include "wxSimpleDatingStateMachine.h"
#include "../Analysis/PC/wxSimpleDatingStateMachineAbi.h"

namespace
{
    using Machine = wxSimpleDatingStateMachine;
    using Prefix = Machine::CapturedPrefixForAnalysis;
    using Profile = Machine::ProfileForAnalysis;
    namespace Abi = wxSimpleDatingStateMachineAbi;

    static_assert(Prefix{}.size() == Abi::CapturedMinimumPrefixBytes);
    static_assert(Abi::BloomOffset + sizeof(std::uint8_t) <= Abi::DateOffset);
    static_assert(Abi::DateOffset + sizeof(std::uint32_t) == Abi::CapturedMinimumPrefixBytes);

    std::uint32_t ReadWord(const Prefix& prefix, std::size_t offset) noexcept
    {
        return std::uint32_t(prefix[offset]) | (std::uint32_t(prefix[offset + 1]) << 8)
            | (std::uint32_t(prefix[offset + 2]) << 16) | (std::uint32_t(prefix[offset + 3]) << 24);
    }

    bool Admit(const Prefix& before, Profile profile) noexcept
    {
        return profile == Profile::PcCapturedPrimaryBloomByteAndDateDword && ReadWord(before, 0) == Abi::CapturedPrimary;
    }
}

bool wxSimpleDatingStateMachine::ReadRawDateForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t& output, ProfileForAnalysis profile) noexcept
{
    if (!Admit(before, profile))
        return false;
    output = ReadWord(before, Abi::DateOffset);
    return true;
}

bool wxSimpleDatingStateMachine::WriteRawDateForAnalysis(const CapturedPrefixForAnalysis& before, std::uint32_t argument,
    CapturedPrefixForAnalysis& output, ProfileForAnalysis profile) noexcept
{
    if (!Admit(before, profile))
        return false;
    auto staged = before;
    for (std::size_t byte = 0; byte < 4; ++byte)
        staged[Abi::DateOffset + byte] = std::uint8_t(argument >> (8 * byte));
    output = staged;
    return true;
}

bool wxSimpleDatingStateMachine::ComposeRawDateForAnalysis(const CapturedPrefixForAnalysis& before, std::uint32_t argument,
    ComposedDateForAnalysis& output, ProfileForAnalysis profile) noexcept
{
    ComposedDateForAnalysis staged;
    if (!ReadRawDateForAnalysis(before, staged.beforeRaw, profile)
        || !WriteRawDateForAnalysis(before, argument, staged.afterSet, profile)
        || !ReadRawDateForAnalysis(staged.afterSet, staged.afterRaw, profile))
        return false;
    output = staged;
    return true;
}

bool wxSimpleDatingStateMachine::ReadRawBloomForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint8_t& output, ProfileForAnalysis profile) noexcept
{
    if (!Admit(before, profile))
        return false;
    output = before[Abi::BloomOffset];
    return true;
}

bool wxSimpleDatingStateMachine::WriteBloomLowByteForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t argument, CapturedPrefixForAnalysis& output, ProfileForAnalysis profile) noexcept
{
    if (!Admit(before, profile))
        return false;
    auto staged = before;
    staged[Abi::BloomOffset] = std::uint8_t(argument);
    output = staged;
    return true;
}

bool wxSimpleDatingStateMachine::ComposeBloomLowByteForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t argument, ComposedBloomForAnalysis& output, ProfileForAnalysis profile) noexcept
{
    ComposedBloomForAnalysis staged;
    if (!ReadRawBloomForAnalysis(before, staged.beforeRaw, profile)
        || !WriteBloomLowByteForAnalysis(before, argument, staged.afterSet, profile)
        || !ReadRawBloomForAnalysis(staged.afterSet, staged.afterRaw, profile))
        return false;
    output = staged;
    return true;
}
