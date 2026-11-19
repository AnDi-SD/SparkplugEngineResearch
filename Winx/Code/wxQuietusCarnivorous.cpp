#include "wxQuietusCarnivorous.h"
#include "../Analysis/PC/wxQuietusCarnivorousAbi.h"

namespace
{
using B = wxQuietusCarnivorous;
using Prefix = B::CapturedPrefixForAnalysis;
std::uint32_t ReadWord(const Prefix& bytes, std::size_t offset)
{
    return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8)
        | (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
}
bool Admit(const Prefix& bytes, B::ProfileForAnalysis profile)
{
    return profile == B::ProfileForAnalysis::PcCapturedPrimaryRawDword164
        && ReadWord(bytes, 0) == wxQuietusCarnivorousAbi::CapturedPrimary;
}
}
bool wxQuietusCarnivorous::ReadDamageRawForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t& output, ProfileForAnalysis profile)
{
    if (!Admit(before, profile)) return false;
    output = ReadWord(before, wxQuietusCarnivorousAbi::DamageRawOffset);
    return true;
}
bool wxQuietusCarnivorous::WriteDamageRawForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t rawBits, CapturedPrefixForAnalysis& output, ProfileForAnalysis profile)
{
    if (!Admit(before, profile)) return false;
    auto staged = before;
    for (std::size_t byte = 0; byte < 4; ++byte)
        staged[wxQuietusCarnivorousAbi::DamageRawOffset + byte] = std::uint8_t(rawBits >> (byte * 8));
    output = staged;
    return true;
}
bool wxQuietusCarnivorous::ComposeDamageRawForAnalysis(const CapturedPrefixForAnalysis& before,
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
