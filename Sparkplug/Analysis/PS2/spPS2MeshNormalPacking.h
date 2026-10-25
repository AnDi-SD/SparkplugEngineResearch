#pragma once

// Captured-value reconstruction of EE15E584..15E6BC. Arithmetic words and
// operation order are explicit; a provider is not evidence of EE FPU parity.

#include <array>
#include <cstdint>

namespace sparkplug::evidence::ps2
{
    enum class MeshNormalPackingStageForAnalysis
    {
        Complete, Add, Multiply, Compare, UnsignedAdjustment, Convert,
    };
    struct MeshNormalPackingResultForAnalysis final
    {
        bool completed = false;
        std::uint16_t packed = 0;
        MeshNormalPackingStageForAnalysis stage = MeshNormalPackingStageForAnalysis::Add;
        unsigned component = 2; // Input lanes are X0,Y1,Z2; native visits Z,Y,X.
    };

    // Provider methods return false when their arithmetic domain is unknown.
    // Words are raw binary32 except TryConvertToWord's signed integer bit word.
    // No independent success callback substitutes for an unknown packet body.
    template<class Arithmetic>
    [[nodiscard]] MeshNormalPackingResultForAnalysis PackCompressedMeshNormalForAnalysis(
        const std::array<std::uint32_t, 3>& normal, Arithmetic& arithmetic)
    {
        using Stage = MeshNormalPackingStageForAnalysis;
        std::uint32_t packed = 0x8000;
        for (unsigned component = 3; component-- != 0;)
        {
            const auto fail = [component](Stage stage) { return MeshNormalPackingResultForAnalysis{false, 0, stage, component}; };
            std::uint32_t value = 0;
            if (!arithmetic.TryAddSingle(0x3F800000U, normal[component], value)) return fail(Stage::Add);
            if (!arithmetic.TryMultiplySingle(0x41780000U, value, value)) return fail(Stage::Multiply);
            bool upper = false;
            if (!arithmetic.TryLessEqualSingle(0x4F000000U, value, upper)) return fail(Stage::Compare);
            if (upper && !arithmetic.TrySubtractSingle(value, 0x4F000000U, value)) return fail(Stage::UnsignedAdjustment);
            std::uint32_t integer = 0;
            if (!arithmetic.TryConvertToWord(value, integer)) return fail(Stage::Convert);
            if (upper) integer |= 0x80000000U;
            packed |= (integer & 31U) << (component * 5);
        }
        return {true, static_cast<std::uint16_t>(packed), Stage::Complete, 0};
    }

    // Our strict integer-dyadic provider accepts only finite normal/zero inputs
    // and exactly representable normal/zero ADD/SUB/MUL results. It refuses all
    // operations requiring rounding,overflow,underflow or unsupported inputs.
    // CVT.W truncates/saturates explicitly. No host float or FCR state is used.
    //
    // Primary contract: Sony EE Core User's Manual,8.7/8.8 (round toward0,
    // no guard/round/sticky; IEEE RZ is not generally EE-equivalent):
    // https://docs.alexrp.com/mips/ee.pdf
    // Independent special/conversion rules: PCSX2 commit265d123dc33ee0ddf9c31e5bdf6c03f9d64e9d34,
    // pcsx2/FPU.cpp. No upstream implementation is copied or linked here.
    // Exact-result acceptance concerns only returned packing bits; exception
    // flags,timing and signed-zero arithmetic details are not hardware claims.
    // Ten finite captured records matched unchanged original15E584 words on
    // genericR4000 with explicit RZ:30 ADD/MUL/compare/CVT each. Annulled SUB
    // slots had no effect. This does not qualify the upper unsigned branch,
    // general EE FPU behavior or default compressed-normal packet acceptance.
    class ExactMeshNormalArithmeticForAnalysis final
    {
    public:
        [[nodiscard]] bool TryAddSingle(std::uint32_t first, std::uint32_t second, std::uint32_t& result) const noexcept;
        [[nodiscard]] bool TrySubtractSingle(std::uint32_t first, std::uint32_t second, std::uint32_t& result) const noexcept;
        [[nodiscard]] bool TryMultiplySingle(std::uint32_t first, std::uint32_t second, std::uint32_t& result) const noexcept;
        [[nodiscard]] bool TryLessEqualSingle(std::uint32_t first, std::uint32_t second, bool& result) const noexcept;
        [[nodiscard]] bool TryConvertToWord(std::uint32_t source, std::uint32_t& result) const noexcept;
    };
}
