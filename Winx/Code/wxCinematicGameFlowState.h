#pragma once

#include "wxGameFlowState.h"
#include "Analysis/Host/wxCinematicGameFlowStateHost.h"
#include "Code/SparkBase/spNotification.h"

namespace winx::reconstruction
{
    // Inferred portable API, not the nativeE4 byte ABI. PC construction,
    // selection reset, own notification and input Entry/Exit are represented.
    // The native movie/load slot30 has an explicit analytical refusal.
    class wxCinematicGameFlowState final : public wxGameFlowState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID=0x137337B1;
        struct CinematicStateForAnalysis final
        {
            std::uint32_t field7C=0,field80=0;
            std::uint8_t field84=0;
            std::uint32_t field88=0;
            std::array<std::uint8_t,82> flags8C{};
            std::uint8_t fieldDE=0,fieldE0=0;
            // Native path3C[64], DF and padding are not initialized by this
            // constructor. No values for them are invented in this profile.
        };
        explicit wxCinematicGameFlowState(wxCinematicGameFlowStateHost&);
        ~wxCinematicGameFlowState() override;
        static void SetFactoryHostForAnalysis(wxCinematicGameFlowStateHost*) noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        // Inherited Copy is the native literal no-op; Clone runs the new
        // constructor, including its82 shared flag updates and subscription.
        void vfunc_0C(const void*) noexcept override;
        void NotifyCinematicForAnalysis(const sparkplug::reconstruction::spNotificationForAnalysis&) noexcept;
        void ResetCinematicSelectionsForAnalysis();
        void FlowSlot20ForAnalysis() override;
        void FlowSlot24ForAnalysis() override;
        bool FlowSlot30ForAnalysis() override;
        [[nodiscard]] const CinematicStateForAnalysis& GetCinematicStateForAnalysis() const noexcept { return cinematic_; }
        CinematicStateForAnalysis& CinematicStateForFixture() noexcept { return cinematic_; }
    private:
        wxCinematicGameFlowStateHost& cinematicHost_;
        CinematicStateForAnalysis cinematic_;
    };
}
