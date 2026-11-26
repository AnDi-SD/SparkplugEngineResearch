#include "wxOneLinerMovingCharacter.h"
#include "../Analysis/PC/wxOneLinerMovingCharacterAbi.h"

namespace
{
using namespace wxOneLinerMovingCharacterAbi;
using Character = wxOneLinerMovingCharacter;

static_assert(wxOneLinerMovingCharacter::CapturedPrefixForAnalysis{}.size() == CapturedMinimumPrefixBytes);
static_assert(OneLinerIdRawDwordOffset + sizeof(std::uint32_t) == CapturedMinimumPrefixBytes);

std::uint32_t ReadLittleEndianWord(const std::uint8_t* bytes)
{
    return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8)
        | (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
}

bool MatchesPcProfile(const Character::CapturedPrefixForAnalysis& prefix, Character::ProfileForAnalysis profile)
{
    return profile == Character::ProfileForAnalysis::PcCapturedPrimaryOneLinerIdRawDword124
        && ReadLittleEndianWord(prefix.data()) == CapturedPrimary;
}
}

bool wxOneLinerMovingCharacter::ReadOneLinerIdRawForAnalysis(const CapturedPrefixForAnalysis& before,
    std::uint32_t& output, ProfileForAnalysis profile)
{
    if (!MatchesPcProfile(before, profile))
    {
        return false;
    }

    output = ReadLittleEndianWord(before.data() + OneLinerIdRawDwordOffset);
    return true;
}

bool wxOneLinerMovingCharacter::WriteOneLinerIdRawForAnalysis(CapturedPrefixForAnalysis& prefix,
    std::uint32_t value, ProfileForAnalysis profile)
{
    if (!MatchesPcProfile(prefix, profile))
    {
        return false;
    }

    for (std::size_t byte = 0; byte < sizeof(value); ++byte)
    {
        prefix[OneLinerIdRawDwordOffset + byte] = static_cast<std::uint8_t>(value >> (byte * 8));
    }
    return true;
}
