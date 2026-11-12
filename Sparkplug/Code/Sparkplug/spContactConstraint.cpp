#include "spContactConstraint.h"
#include "Analysis/PC/spContactConstraintAbi.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        constexpr std::uint32_t Sign = 0x80000000U;
        constexpr std::uint32_t Magnitude = 0x7FFFFFFFU;
        constexpr std::uint32_t Exponent = 0x7F800000U;
        constexpr std::uint32_t MaximumFinite = 0x7F7FFFFFU;

        bool IsFinite(const std::uint32_t word) noexcept { return (word & Exponent) != Exponent; }
        bool IsNegative(const std::uint32_t word) noexcept
        { return (word & Sign) != 0 && (word & Magnitude) != 0; }
        bool IsPositive(const std::uint32_t word) noexcept
        { return (word & Sign) == 0 && (word & Magnitude) != 0; }
    }

    std::uint32_t spContactConstraint::ReadWordForAnalysis(const std::size_t offset) const noexcept
    {
        std::uint32_t value = 0;
        for (unsigned byte = 0; byte != 4; ++byte)
            value |= std::uint32_t{state_[offset + byte]} << (byte * 8);
        return value;
    }

    void spContactConstraint::WriteWordForAnalysis(const std::size_t offset, const std::uint32_t value) noexcept
    {
        for (unsigned byte = 0; byte != 4; ++byte)
            state_[offset + byte] = static_cast<std::uint8_t>(value >> (byte * 8));
    }

    spContactConstraint::ResultForAnalysis spContactConstraint::OwnSlot24ForAnalysis(
        RawOwn24SourceForAnalysis input) noexcept
    {
        using namespace evidence::pc;
        if (profile_ != ProfileForAnalysis::PcCapturedRawOwn24
            && profile_ != ProfileForAnalysis::PcCapturedFiniteOwn1CAndRawOwn24)
            return lastResult_ = ResultForAnalysis::UnqualifiedProfile;
        if (ReadWordForAnalysis(0) != spContactConstraintPrimary)
            return lastResult_ = ResultForAnalysis::CapturedPrimaryNotAdmitted;
        const auto transfer = [this, &input](const std::size_t offset) noexcept
        {
            std::uint32_t word = 0;
            for (unsigned byte = 0; byte != 4; ++byte)
                word |= std::uint32_t{input[offset + byte]} << (byte * 8);
            WriteWordForAnalysis(spContactConstraintOwn24DestinationOffset + offset, word);
        };
        // Original helper: eleven forward MOVSD words, then twelve literal
        // word transfers. Unknown encoded pointer/float words are only data.
        for (std::size_t offset = 0; offset != 0x2C; offset += 4) transfer(offset);
        for (std::size_t offset = 0x2C; offset != 0x5C; offset += 4) transfer(offset);
        return lastResult_ = ResultForAnalysis::Completed;
    }

    spContactConstraint::ResultForAnalysis spContactConstraint::OwnSlot1CForAnalysis() noexcept
    {
        using namespace evidence::pc;
        if (profile_ != ProfileForAnalysis::PcCapturedFiniteOwn1C
            && profile_ != ProfileForAnalysis::PcCapturedFiniteOwn1CAndRawOwn24)
            return lastResult_ = ResultForAnalysis::UnqualifiedProfile;
        if (ReadWordForAnalysis(0) != spContactConstraintPrimary)
            return lastResult_ = ResultForAnalysis::CapturedPrimaryNotAdmitted;
        auto first = ReadWordForAnalysis(spContactConstraintRaw8COffset);
        auto second = ReadWordForAnalysis(spContactConstraintRaw90Offset);
        // Our input admission is separate from the original operation. Check
        // both finite raw values before any captured-state mutation, including
        // when the native mode0 path would not read the second coefficient.
        if (!IsFinite(first) || !IsFinite(second))
            return lastResult_ = ResultForAnalysis::NonFiniteInput;

        WriteWordForAnalysis(spContactConstraintRaw58Offset, 1);
        WriteWordForAnalysis(spContactConstraintRaw5COffset, 0);
        // For finite IEEE32 values these raw classifications exactly preserve
        // the original FCOMP/status branches, including subnormals and -0.
        // Portable caller FPU exception/status flags are outside this facade.
        if (IsNegative(first))
        {
            first = 0;
            WriteWordForAnalysis(spContactConstraintRaw8COffset, 0);
        }
        if ((state_[spContactConstraintRaw88Offset] & 1U) != 0)
        {
            if (IsPositive(first)) WriteWordForAnalysis(spContactConstraintRaw58Offset, 2);
            if (IsNegative(second))
            {
                second = 0;
                WriteWordForAnalysis(spContactConstraintRaw90Offset, 0);
            }
            if (IsPositive(second))
                WriteWordForAnalysis(spContactConstraintRaw58Offset,
                    ReadWordForAnalysis(spContactConstraintRaw58Offset) + 1U);
            if (first == MaximumFinite) WriteWordForAnalysis(spContactConstraintRaw5COffset, 1);
            if (second == MaximumFinite)
                WriteWordForAnalysis(spContactConstraintRaw5COffset,
                    ReadWordForAnalysis(spContactConstraintRaw5COffset) + 1U);
        }
        else
        {
            if (IsPositive(first)) WriteWordForAnalysis(spContactConstraintRaw58Offset, 3);
            if (first == MaximumFinite) WriteWordForAnalysis(spContactConstraintRaw5COffset, 2);
            // The original mode0 branch preserves all raw90 bytes.
        }
        return lastResult_ = ResultForAnalysis::Completed;
    }
}
