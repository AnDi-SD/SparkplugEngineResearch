#include "wxEyeBallSentry.h"
#include "../Analysis/PC/wxEyeBallSentryAbi.h"
#include "Analysis/PC/spFloat80TowardZero.h"
#include <cstring>

namespace
{
using B = wxEyeBallSentry;
using Prefix = B::CapturedPrefixForAnalysis;
std::uint32_t ReadWord(const Prefix& bytes, std::size_t offset)
{
    return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8)
        | (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
}
bool Admit(const Prefix& bytes, B::ProfileForAnalysis profile)
{
    return profile == B::ProfileForAnalysis::PcCapturedPrimaryRawDword144
        && ReadWord(bytes, 0) == wxEyeBallSentryAbi::CapturedPrimary;
}
}
bool wxEyeBallSentry::ReadDamageRawForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t& output, ProfileForAnalysis profile)
{
    if (!Admit(before, profile)) return false;
    output = ReadWord(before, wxEyeBallSentryAbi::DamageRawOffset);
    return true;
}
bool wxEyeBallSentry::WriteDamageRawForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t rawBits, CapturedPrefixForAnalysis& output, ProfileForAnalysis profile)
{
    if (!Admit(before, profile)) return false;
    auto staged = before;
    for (std::size_t byte = 0; byte < 4; ++byte)
        staged[wxEyeBallSentryAbi::DamageRawOffset + byte] = std::uint8_t(rawBits >> (byte * 8));
    output = staged;
    return true;
}
bool wxEyeBallSentry::ComposeDamageRawForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t rawBits, ComposedDamageForAnalysis& output, ProfileForAnalysis profile)
{
    if (!Admit(before, profile)) return false;
    ComposedDamageForAnalysis staged{};
    auto state = before;
    if (!ReadDamageRawForAnalysis(state, staged.beforeRaw, profile)
        || !WriteDamageRawForAnalysis(state, rawBits, state, profile)
        || !ReadDamageRawForAnalysis(state, staged.afterRaw, profile)) return false;
    staged.afterSet = state;
    output = staged;
    return true;
}

namespace
{
using ScalarProperty = wxEyeBallSentry::ScalarPropertyForAnalysis;
using ScalarProfile = wxEyeBallSentry::ScalarProfileForAnalysis;
using ScalarResult = wxEyeBallSentry::ScalarResultForAnalysis;
constexpr std::array<std::size_t, 3> ScalarOffsets{0x130, 0x12C, 0x128};
static_assert(sizeof(wxEyeBallSentry::CapturedPrefixForAnalysis) == wxEyeBallSentryAbi::CapturedMinimumPrefixBytes);

void WriteScalarWord(Prefix& bytes, const std::size_t offset, const std::uint32_t word) noexcept
{
    for (unsigned index = 0; index != 4; ++index)
        bytes[offset + index] = std::uint8_t(word >> (index * 8));
}
bool ScalarWordAdmitted(const std::uint32_t word) noexcept
{
    const auto exponent = (word >> 23) & 255;
    return exponent != 255 && (exponent != 0 || (word & 0x7FFFFF) == 0);
}
ScalarResult AdmitScalar(const Prefix& before, const ScalarProperty property,
    const ScalarProfile profile, std::size_t& offset) noexcept
{
    if (profile != ScalarProfile::PcCapturedPrimaryFiniteNormalOrZeroF32) return ScalarResult::UnqualifiedProfile;
    if (ReadWord(before, 0) != wxEyeBallSentryAbi::CapturedPrimary) return ScalarResult::CapturedPrimaryNotAdmitted;
    const auto index = static_cast<unsigned>(property);
    if (index >= ScalarOffsets.size()) return ScalarResult::UnknownProperty;
    for (const auto selected : ScalarOffsets)
        if (!ScalarWordAdmitted(ReadWord(before, selected))) return ScalarResult::CapturedScalarNotAdmitted;
    offset = ScalarOffsets[index];
    return ScalarResult::Completed;
}
}

wxEyeBallSentry::ScalarResultForAnalysis wxEyeBallSentry::ReadScalarForAnalysis(
    const CapturedPrefixForAnalysis& before, const ScalarPropertyForAnalysis property,
    ScalarReturnForAnalysis& output, const ScalarProfileForAnalysis profile) noexcept
{
    std::size_t offset = 0;
    const auto admitted = AdmitScalar(before, property, profile, offset);
    if (admitted != ScalarResultForAnalysis::Completed) return admitted;
    const auto bits = ReadWord(before, offset);
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    const auto number = sparkplug::evidence::pc::float80_rtz::FromFloat(value);
    const auto exponent = std::uint16_t((number.negative ? 0x8000 : 0)
        | (number.mantissa ? unsigned(number.exponent + 63 + 16383) : 0));
    ScalarReturnForAnalysis staged{};
    for (unsigned index = 0; index != 8; ++index)
        staged[index] = std::uint8_t(number.mantissa >> (index * 8));
    staged[8] = std::uint8_t(exponent); staged[9] = std::uint8_t(exponent >> 8);
    output = staged;
    return ScalarResultForAnalysis::Completed;
}

wxEyeBallSentry::ScalarResultForAnalysis wxEyeBallSentry::WriteScalarBitsForAnalysis(
    const CapturedPrefixForAnalysis& before, const ScalarPropertyForAnalysis property,
    const std::uint32_t rawArgument, CapturedPrefixForAnalysis& output, const ScalarProfileForAnalysis profile) noexcept
{
    std::size_t offset = 0;
    const auto admitted = AdmitScalar(before, property, profile, offset);
    if (admitted != ScalarResultForAnalysis::Completed) return admitted;
    if (!ScalarWordAdmitted(rawArgument)) return ScalarResultForAnalysis::ArgumentNotAdmitted;
    auto staged = before;
    WriteScalarWord(staged, offset, rawArgument);
    output = staged;
    return ScalarResultForAnalysis::Completed;
}
