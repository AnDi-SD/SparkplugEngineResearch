#pragma once
#include "spContactTwoBodyRowQueries.h"
namespace sparkplug::reconstruction
{
    using spContactTwoBodyRawDotRowStateForAnalysis = spContactTwoBodyRowStateForAnalysis;
    struct spContactTwoBodyRawDotRowInputForAnalysis final
    {
        std::uint32_t scalarArgumentWord = 0;
        std::array<std::uint32_t,3> bodyPointWords{}, body2PointWords{};
        std::array<std::uint32_t,6> bodyRaw8CToA0Words{}, body2Raw8CToA0Words{};
        std::uint32_t managerRaw7C = 0, managerRaw80 = 0;
    };
    struct spContactTwoBodyRawDotRowDiagnosticForAnalysis final
    {
        // Selection of original optional store PC0048B8E6 BEFORE F32 rounding.
        // True may leave identical bytes. Our bool is not a C++ machine-write
        // trace, native ABI, CPU status word or original method return value.
        bool rhsStoreSelected = false;
    };
    enum class spContactTwoBodyRawDotRowProfileForAnalysis
    { Unqualified, PcCapturedWarmFiniteRawDotNearest037F };
    using spContactTwoBodyRawDotRowResultForAnalysis = spContactTwoBodyRowResultForAnalysis;
    // Fixed40=0/count58=1/full88=8; distinct nonzero body48/4C and buffer6C/70
    // tags plus other required nonzero tags are inert and never dereferenced.
    // Scalar[.25,4], other participating numbers finite abs<=16, Manager exact.
    // Zero normal is admitted; count1 calls no tangent helper or root.
    // Refusal preserves BOTH outputs. Staged success permits before/output
    // alias. Diagnostic is a separate valid object, with no state overlap.
    spContactTwoBodyRawDotRowResultForAnalysis ComputeContactTwoBodyRawDotRowForAnalysis(
        const spContactTwoBodyRawDotRowStateForAnalysis& before,
        spContactTwoBodyRawDotRowInputForAnalysis input,
        spContactTwoBodyRawDotRowStateForAnalysis& output,
        spContactTwoBodyRawDotRowDiagnosticForAnalysis& diagnostic,
        spContactTwoBodyRawDotRowProfileForAnalysis profile =
            spContactTwoBodyRawDotRowProfileForAnalysis::Unqualified) noexcept;
}
