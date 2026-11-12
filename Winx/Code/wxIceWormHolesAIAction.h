#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace winx::reconstruction
{
    // Our supplied captured-value facade. Registration base metadata does
    // not establish native C++ inheritance; this class has no base or vptr.
    class wxIceWormHolesAIAction final
    {
    public:
        static constexpr std::uint32_t ClassID = 0x812B81AB;
        static constexpr std::uint32_t RegistrationBaseClassID = 0x490A6EB5;
        static constexpr std::size_t CapturedBytesForAnalysis = 968;
        using CapturedStateForAnalysis = std::array<std::uint8_t, CapturedBytesForAnalysis>;
        // Our input view only. The native message class and complete extent
        // are unknown. This supplied view covers DWORD reads at 0 and 18.
        using RawMessageForAnalysis = std::array<std::uint8_t, 28>;
        enum class ProfileForAnalysis
        { Unqualified, PcCapturedNoNodeCallsReadOnlyMessage28 };
        enum class ResultForAnalysis
        { Completed, UnqualifiedProfile, CapturedPrimaryNotAdmitted, ForeignNodeBranchNotAdmitted };
        struct AccessForAnalysis final
        {
            enum class Kind { Read, Write };
            enum class Role { MessageValue, CapturedState };
            Kind kind{}; Role role{}; std::uint32_t offset = 0;
            std::uint8_t bytes = 0; std::uint32_t value = 0;
        };
        explicit wxIceWormHolesAIAction(const CapturedStateForAnalysis& state,
            ProfileForAnalysis profile = ProfileForAnalysis::Unqualified) noexcept
            : state_(state), profile_(profile) {}
        wxIceWormHolesAIAction(const wxIceWormHolesAIAction&) = delete;
        wxIceWormHolesAIAction& operator=(const wxIceWormHolesAIAction&) = delete;
        wxIceWormHolesAIAction(wxIceWormHolesAIAction&&) = delete;
        wxIceWormHolesAIAction& operator=(wxIceWormHolesAIAction&&) = delete;

        // Our admission/status contract only. Input is copied BY VALUE before
        // captured mutation. Native pointer ABI, alias/fault/return semantics,
        // delivery and all method branches are outside this explicit profile.
        ResultForAnalysis HandleMessageForAnalysis(RawMessageForAnalysis) noexcept;
        const CapturedStateForAnalysis& GetCapturedStateForAnalysis() const noexcept { return state_; }
        CapturedStateForAnalysis& CapturedStateForFixture() noexcept { return state_; }
        ResultForAnalysis LastResultForAnalysis() const noexcept { return result_; }
        const std::array<AccessForAnalysis, 3>& LastAccessesForAnalysis() const noexcept { return accesses_; }
        std::size_t LastAccessCountForAnalysis() const noexcept { return accessCount_; }
    private:
        void RecordForAnalysis(AccessForAnalysis::Kind, AccessForAnalysis::Role,
            std::uint32_t offset, std::uint8_t bytes, std::uint32_t value) noexcept;
        void StoreWordForAnalysis(std::size_t offset, std::uint32_t value) noexcept;
        void StoreByteForAnalysis(std::size_t offset, std::uint8_t value) noexcept;
        CapturedStateForAnalysis state_;
        const ProfileForAnalysis profile_;
        ResultForAnalysis result_ = ResultForAnalysis::UnqualifiedProfile;
        std::array<AccessForAnalysis, 3> accesses_{};
        std::size_t accessCount_ = 0;
    };
}
