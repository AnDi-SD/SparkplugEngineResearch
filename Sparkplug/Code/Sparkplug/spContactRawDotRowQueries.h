#pragma once
#include "spContactRowQueries.h"
#include <array>
#include <cstdint>

namespace sparkplug::reconstruction
{
    using spContactRawDotRowStateForAnalysis = spContactSingleBodyRowStateForAnalysis;
    struct spContactRawDotRowInputForAnalysis final
    {
        std::uint32_t scalarArgumentWord = 0;
        std::array<std::uint32_t,3> bodyPointWords{};
        // Six supplied raw read roles at body8C/90/94/98/9C/A0.
        // Our BYVALUE data, with no native pointer dereference or field names.
        std::array<std::uint32_t,6> bodyRaw8CToA0Words{};
        std::uint32_t managerRaw7C = 0, managerRaw80 = 0;
    };
    struct spContactRawDotRowDiagnosticForAnalysis final
    {
        // Original optional F32 store branch at PC48B8E6. This may be true
        // when the new F32 word equals the old word. It describes branch
        // selection, not a C++ machine-store trace, native ABI or C++ return.
        bool rhsStoreSelected = false;
    };
    enum class spContactRawDotRowProfileForAnalysis
    { Unqualified, PcCapturedWarmFiniteRawDotNearest037F };
    using spContactRawDotRowResultForAnalysis = spContactSingleBodyRowResultForAnalysis;

    // Fixed40=0/4C=0/58=1/70=0/fullDWORD88=8; required tags are inert/nonzero.
    // Scalar[.25,4], all point/normal/CC/raw body/contact94/98 finite abs<=16;
    // Manager7C/80 exact. Zero normal is admitted; no tangent/root is used.
    // Refusal preserves BOTH outputs. Success is staged and supports exact
    // before/output alias. Diagnostic is a separate valid bool-containing
    // object; overlap with the state representation is outside this API.
    spContactRawDotRowResultForAnalysis ComputeContactRawDotRowForAnalysis(
        const spContactRawDotRowStateForAnalysis& before,
        spContactRawDotRowInputForAnalysis input,
        spContactRawDotRowStateForAnalysis& output,
        spContactRawDotRowDiagnosticForAnalysis& diagnostic,
        spContactRawDotRowProfileForAnalysis profile =
            spContactRawDotRowProfileForAnalysis::Unqualified) noexcept;
}
