#pragma once
#include "wxAIAction.h"
#include <cstddef>

namespace winx::reconstruction
{
    // Our captured-value facade: the raw7160B snapshot is separate from this
    // portable C++ object's layout, base fields, containers and ownership.
    class wxKikoMovingAIAction final : public wxAIAction
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x99887766;
        static constexpr std::size_t CapturedBytesForAnalysis = 7160;
        using CapturedState = std::array<std::uint8_t, CapturedBytesForAnalysis>;
        enum class ProfileForAnalysis { Unqualified, PcCapturedReadOnlyDisjointMessage };
        enum class ResultForAnalysis
        { Completed, UnqualifiedProfile, CapturedPrimaryNotAdmitted, UnqualifiedHook };
        struct AccessForAnalysis final
        {
            enum class Kind { Read, Write };
            enum class Role { MessageValue, CapturedState };
            Kind kind{}; Role role{}; std::uint32_t offset = 0;
            std::uint8_t bytes = 4; std::uint32_t value = 0;
        };
        explicit wxKikoMovingAIAction(const CapturedState&,
            ProfileForAnalysis = ProfileForAnalysis::Unqualified) noexcept;
        ~wxKikoMovingAIAction() override = default;
        wxKikoMovingAIAction(const wxKikoMovingAIAction&) = delete;
        wxKikoMovingAIAction& operator=(const wxKikoMovingAIAction&) = delete;
        wxKikoMovingAIAction(wxKikoMovingAIAction&&) = delete;
        wxKikoMovingAIAction& operator=(wxKikoMovingAIAction&&) = delete;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject>
            vfunc_10(sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&,
            sparkplug::reconstruction::spCloneManager&) const override;
        // Our name/admission result only. The existing portable message view
        // is passed BY VALUE; its native class and C++ return type are unknown.
        // This does not qualify production notification delivery or pointer ABI.
        ResultForAnalysis HandleMessageForAnalysis(wxAIActionMessageForAnalysis) noexcept;
        void vfunc_0C(const void*) noexcept override;
        bool vfunc_24() noexcept override;
        bool vfunc_28() noexcept override;
        bool vfunc_2C() noexcept override;
        void vfunc_30() noexcept override;
        void vfunc_34_ClearForAnalysis() noexcept override;
        void vfunc_38() noexcept override;
        bool vfunc_3C(const void*) noexcept override;
        bool vfunc_40(const void*) noexcept override;
        const CapturedState& CapturedStateForAnalysis() const noexcept { return state_; }
        CapturedState& CapturedStateForFixture() noexcept { return state_; }
        ResultForAnalysis LastResultForAnalysis() const noexcept { return result_; }
        const std::array<AccessForAnalysis,3>& LastAccessesForAnalysis() const noexcept { return accesses_; }
        std::size_t LastAccessCountForAnalysis() const noexcept { return accessCount_; }
    private:
        std::uint32_t WordForAnalysis(std::size_t) const noexcept;
        void StoreWordForAnalysis(std::size_t,std::uint32_t) noexcept;
        bool RefuseHookForAnalysis() noexcept;
        CapturedState state_;
        const ProfileForAnalysis profile_;
        ResultForAnalysis result_ = ResultForAnalysis::UnqualifiedProfile;
        std::array<AccessForAnalysis,3> accesses_{};
        std::size_t accessCount_ = 0;
    };
}
