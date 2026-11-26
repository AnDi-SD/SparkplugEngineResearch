#include "wxIceWormAIBehavior.h"
#include "../Analysis/PC/wxIceWormAIBehaviorAbi.h"

namespace
{
    using Target = wxIceWormAIBehavior;
    using Prefix = Target::CapturedPrefixForAnalysis;
    using Property = Target::PropertyForAnalysis;
    using Profile = Target::ProfileForAnalysis;
    namespace Abi = wxIceWormAIBehaviorAbi;

    static_assert(Prefix{}.size() == Abi::CapturedMinimumPrefixBytes);
    static_assert(Abi::RawOffsets.back() + sizeof(std::uint32_t) == Abi::CapturedMinimumPrefixBytes);

    std::uint32_t ReadWord(const Prefix& bytes, std::size_t offset) noexcept
    {
        return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8)
            | (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
    }

    bool Admit(const Prefix& before, Property property, Profile profile, std::size_t& offset) noexcept
    {
        const auto index = static_cast<unsigned>(property);
        if (profile != Profile::PcCapturedPrimaryHolesRawDwords1B4And1B8
            || index >= Abi::RawOffsets.size() || ReadWord(before, 0) != Abi::CapturedPrimary)
            return false;
        offset = Abi::RawOffsets[index];
        return true;
    }
}

bool wxIceWormAIBehavior::ReadRawPropertyForAnalysis(const CapturedPrefixForAnalysis& before,
    PropertyForAnalysis property, std::uint32_t& output, ProfileForAnalysis profile) noexcept
{
    std::size_t offset{};
    if (!Admit(before, property, profile, offset))
        return false;
    output = ReadWord(before, offset);
    return true;
}

bool wxIceWormAIBehavior::WriteRawPropertyForAnalysis(const CapturedPrefixForAnalysis& before,
    PropertyForAnalysis property, std::uint32_t rawArgument, CapturedPrefixForAnalysis& output,
    ProfileForAnalysis profile) noexcept
{
    std::size_t offset{};
    if (!Admit(before, property, profile, offset))
        return false;
    auto staged = before;
    for (unsigned byte = 0; byte < 4; ++byte)
        staged[offset + byte] = std::uint8_t(rawArgument >> (8 * byte));
    output = staged;
    return true;
}

bool wxIceWormAIBehavior::ComposeRawPropertyForAnalysis(const CapturedPrefixForAnalysis& before,
    PropertyForAnalysis property, std::uint32_t rawArgument, ComposedRawPropertyForAnalysis& output,
    ProfileForAnalysis profile) noexcept
{
    ComposedRawPropertyForAnalysis staged;
    if (!ReadRawPropertyForAnalysis(before, property, staged.beforeRaw, profile)
        || !WriteRawPropertyForAnalysis(before, property, rawArgument, staged.afterSet, profile)
        || !ReadRawPropertyForAnalysis(staged.afterSet, property, staged.afterRaw, profile))
        return false;
    output = staged;
    return true;
}
