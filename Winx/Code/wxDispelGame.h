#pragma once
#include <array>
#include <cstdint>

// Stateless analytical facade of one PC descriptor. No native object layout,
// constructor, physical inheritance or current runtime global is represented.
class wxDispelGame
{
public:
    using ScalarReturnForAnalysis = std::array<std::uint8_t, 10>;
    enum class ProfileForAnalysis { Unqualified, PcPristineReadOnlyGlobal006DBA9CZero };
    enum class ResultForAnalysis { Completed, UnqualifiedProfile };

    // The qualified pristine-image getter reads +0.0 from read-only .rdata.
    // Refusal preserves output. No receiver state or native signature is inferred.
    static ResultForAnalysis ReadGameSpeedForAnalysis(ScalarReturnForAnalysis& output,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
    // RET4 does not read its argument or receiver and performs no store.
    static ResultForAnalysis WriteGameSpeedBitsForAnalysis(std::uint32_t rawArgument,
        ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept;
};
