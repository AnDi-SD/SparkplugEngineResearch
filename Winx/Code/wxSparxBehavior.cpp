#include "wxSparxBehavior.h"
#include "../Analysis/PC/wxSparxBehaviorAbi.h"

namespace
{
using B = wxSparxBehavior;
using Prefix = B::CapturedPrefixForAnalysis;
namespace Abi = wxSparxBehaviorAbi;

static_assert(Abi::SparxTypeRawDwordOffset + 4 == Abi::TestAnimationRawByteOffset);

std::uint32_t ReadPcDword(const Prefix& bytes, std::size_t offset) noexcept
{
    return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8)
        | (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
}

bool Admit(const Prefix& bytes, B::ProfileForAnalysis profile)
{
    return profile == B::ProfileForAnalysis::PcCapturedPrimaryTestAnimationRawByte128
        && ReadPcDword(bytes, 0) == Abi::CapturedPrimary;
}

bool AdmitType(const Prefix& bytes, B::TypeProfileForAnalysis profile) noexcept
{
    return profile == B::TypeProfileForAnalysis::PcCapturedPrimarySparxTypeRawDword124
        && ReadPcDword(bytes, 0) == Abi::CapturedPrimary;
}
}
bool wxSparxBehavior::ReadTestAnimationByteForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint8_t& output, ProfileForAnalysis profile)
{
    if (!Admit(before, profile)) return false;
    output = before[wxSparxBehaviorAbi::TestAnimationRawByteOffset];
    return true;
}
bool wxSparxBehavior::WriteTestAnimationLowByteForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t rawArgumentWord, CapturedPrefixForAnalysis& output, ProfileForAnalysis profile)
{
    if (!Admit(before, profile)) return false;
    auto staged = before;
    staged[wxSparxBehaviorAbi::TestAnimationRawByteOffset] = std::uint8_t(rawArgumentWord & 0xFFu);
    output = staged;
    return true;
}

bool wxSparxBehavior::ComposeTestAnimationLowByteForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t rawArgumentWord, ComposedTestAnimationByteForAnalysis& output, ProfileForAnalysis profile)
{
    if (!Admit(before, profile)) return false;
    ComposedTestAnimationByteForAnalysis staged{};
    auto state = before;
    if (!ReadTestAnimationByteForAnalysis(state, staged.beforeRaw, profile)
        || !WriteTestAnimationLowByteForAnalysis(state, rawArgumentWord, state, profile)
        || !ReadTestAnimationByteForAnalysis(state, staged.afterRaw, profile)) return false;
    staged.afterSet = state;
    output = staged;
    return true;
}

bool wxSparxBehavior::ReadSparxTypeRawForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t& output, TypeProfileForAnalysis profile) noexcept
{
    if (!AdmitType(before, profile))
        return false;
    output = ReadPcDword(before, Abi::SparxTypeRawDwordOffset);
    return true;
}

bool wxSparxBehavior::WriteSparxTypeRawForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t rawArgumentWord, CapturedPrefixForAnalysis& output, TypeProfileForAnalysis profile) noexcept
{
    if (!AdmitType(before, profile))
        return false;
    auto staged = before;
    for (unsigned i = 0; i != 4; ++i)
        staged[Abi::SparxTypeRawDwordOffset + i] = static_cast<std::uint8_t>(rawArgumentWord >> (i * 8));
    output = staged;
    return true;
}

bool wxSparxBehavior::ComposeSparxTypeRawForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t rawArgumentWord, ComposedSparxTypeRawForAnalysis& output, TypeProfileForAnalysis profile) noexcept
{
    ComposedSparxTypeRawForAnalysis staged{};
    if (!ReadSparxTypeRawForAnalysis(before, staged.beforeRaw, profile)
        || !WriteSparxTypeRawForAnalysis(before, rawArgumentWord, staged.afterSet, profile)
        || !ReadSparxTypeRawForAnalysis(staged.afterSet, staged.afterRaw, profile))
        return false;
    output = staged;
    return true;
}
