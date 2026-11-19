#include "wxCutscene.h"
#include "../Analysis/PC/wxCutsceneAbi.h"
#include "Analysis/PC/spFloat80TowardZero.h"
#include <cstring>

namespace
{
using Facade = wxCutscene;
using Prefix = Facade::CapturedPrefixForAnalysis;
using Property = Facade::ScalarPropertyForAnalysis;
using Profile = Facade::ScalarProfileForAnalysis;
using Result = Facade::ScalarResultForAnalysis;
static_assert(sizeof(Prefix) == wxCutsceneAbi::CapturedMinimumPrefixBytes);
std::uint32_t ReadWord(const Prefix& bytes, const std::size_t offset) noexcept
{
    return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset+1]) << 8)
        | (std::uint32_t(bytes[offset+2]) << 16) | (std::uint32_t(bytes[offset+3]) << 24);
}
bool AdmittedWord(const std::uint32_t word) noexcept
{
    const auto exponent = (word >> 23) & 255;
    return exponent != 255 && (exponent != 0 || (word & 0x7FFFFF) == 0);
}
Result Admit(const Prefix& before, const Property property, const Profile profile, std::size_t& offset) noexcept
{
    if (profile != Profile::PcCapturedPrimaryFiniteNormalOrZeroF32) return Result::UnqualifiedProfile;
    if (ReadWord(before, 0) != wxCutsceneAbi::CapturedPrimary) return Result::CapturedPrimaryNotAdmitted;
    const auto index = static_cast<unsigned>(property);
    if (index >= wxCutsceneAbi::ScalarOffsets.size()) return Result::UnknownProperty;
    for (const auto selected : wxCutsceneAbi::ScalarOffsets)
        if (!AdmittedWord(ReadWord(before, selected))) return Result::CapturedScalarNotAdmitted;
    offset = wxCutsceneAbi::ScalarOffsets[index];
    return Result::Completed;
}
}

wxCutscene::ScalarResultForAnalysis wxCutscene::ReadScalarForAnalysis(
    const CapturedPrefixForAnalysis& before, const ScalarPropertyForAnalysis property,
    ScalarReturnForAnalysis& output, const ScalarProfileForAnalysis profile) noexcept
{
    std::size_t offset = 0;
    const auto admitted = Admit(before, property, profile, offset);
    if (admitted != ScalarResultForAnalysis::Completed) return admitted;
    const auto bits = ReadWord(before, offset);
    float value; std::memcpy(&value, &bits, sizeof(value));
    const auto number = sparkplug::evidence::pc::float80_rtz::FromFloat(value);
    const auto exponent = std::uint16_t((number.negative ? 0x8000 : 0)
        | (number.mantissa ? unsigned(number.exponent + 63 + 16383) : 0));
    ScalarReturnForAnalysis staged{};
    for (unsigned byte = 0; byte != 8; ++byte) staged[byte] = std::uint8_t(number.mantissa >> (8 * byte));
    staged[8] = std::uint8_t(exponent); staged[9] = std::uint8_t(exponent >> 8);
    output = staged;
    return ScalarResultForAnalysis::Completed;
}

wxCutscene::ScalarResultForAnalysis wxCutscene::WriteScalarBitsForAnalysis(
    const CapturedPrefixForAnalysis& before, const ScalarPropertyForAnalysis property,
    const std::uint32_t rawArgument, CapturedPrefixForAnalysis& output, const ScalarProfileForAnalysis profile) noexcept
{
    std::size_t offset = 0;
    const auto admitted = Admit(before, property, profile, offset);
    if (admitted != ScalarResultForAnalysis::Completed) return admitted;
    if (!AdmittedWord(rawArgument)) return ScalarResultForAnalysis::ArgumentNotAdmitted;
    auto staged = before;
    for (unsigned byte = 0; byte != 4; ++byte) staged[offset+byte] = std::uint8_t(rawArgument >> (8*byte));
    output = staged;
    return ScalarResultForAnalysis::Completed;
}
