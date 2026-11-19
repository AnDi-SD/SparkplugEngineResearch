#include "wxDispelGame.h"

wxDispelGame::ResultForAnalysis wxDispelGame::ReadGameSpeedForAnalysis(
    ScalarReturnForAnalysis& output, const ProfileForAnalysis profile) noexcept
{
    if (profile != ProfileForAnalysis::PcPristineReadOnlyGlobal006DBA9CZero)
        return ResultForAnalysis::UnqualifiedProfile;
    // Original FLD dword ptr [006DBA9C] reads binary32 +0, yielding ten zero bytes.
    output = ScalarReturnForAnalysis{};
    return ResultForAnalysis::Completed;
}

wxDispelGame::ResultForAnalysis wxDispelGame::WriteGameSpeedBitsForAnalysis(
    const std::uint32_t rawArgument, const ProfileForAnalysis profile) noexcept
{
    if (profile != ProfileForAnalysis::PcPristineReadOnlyGlobal006DBA9CZero)
        return ResultForAnalysis::UnqualifiedProfile;
    (void)rawArgument;
    return ResultForAnalysis::Completed;
}
