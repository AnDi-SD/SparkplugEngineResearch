#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace sparkplug::reconstruction
{
    // Our captured-state facade for a meaningful operation selected by the
    // original spContactConstraint primary table. This portable class does
    // not reproduce native allocation, vptrs or unproved C++ inheritance.
    class spContactConstraint final
    {
    public:
        static constexpr std::uint32_t ClassID = 0x04353BD3;
        static constexpr std::uint32_t RegistrationBaseClassID = 0x86CD6780;
        static constexpr std::size_t CapturedBytesForAnalysis = 0xE4;
        using CapturedStateForAnalysis = std::array<std::uint8_t, CapturedBytesForAnalysis>;
        using RawOwn24SourceForAnalysis = std::array<std::uint8_t, 0x5C>;
        enum class ProfileForAnalysis { Unqualified, PcCapturedFiniteOwn1C, PcCapturedRawOwn24, PcCapturedFiniteOwn1CAndRawOwn24 };
        enum class ResultForAnalysis
        { Completed, UnqualifiedProfile, CapturedPrimaryNotAdmitted, NonFiniteInput };

        // The supplied bytes are copied inert data. A captured pointer word
        // is never interpreted as a live portable pointer or native vptr.
        explicit spContactConstraint(const CapturedStateForAnalysis& state,
            ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept
            : state_(state), profile_(profile) {}

        // Analytical operation/admission status. The native C++ return type
        // is unestablished; observed EAX is diagnostic and has no portable
        // result contract. This enum reports only analytical admission.
        ResultForAnalysis OwnSlot1CForAnalysis() noexcept;
        // Our separate captured raw-ingress operation. Input is copied by
        // value before destination mutation; no native pointer/alias ABI or
        // source92B structure name is inferred. NaN/pointer words stay raw.
        ResultForAnalysis OwnSlot24ForAnalysis(RawOwn24SourceForAnalysis input) noexcept;
        const CapturedStateForAnalysis& GetCapturedStateForAnalysis() const noexcept { return state_; }
        // Explicit supplied input changes between calls, not a native setter.
        CapturedStateForAnalysis& CapturedStateForFixture() noexcept { return state_; }
        ResultForAnalysis LastResultForAnalysis() const noexcept { return lastResult_; }

    private:
        std::uint32_t ReadWordForAnalysis(std::size_t offset) const noexcept;
        void WriteWordForAnalysis(std::size_t offset, std::uint32_t value) noexcept;
        CapturedStateForAnalysis state_;
        const ProfileForAnalysis profile_;
        ResultForAnalysis lastResult_ = ResultForAnalysis::UnqualifiedProfile;
    };
}
