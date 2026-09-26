#pragma once
#include "Code/SparkBase/spBaseObject.h"
#include "Analysis/Host/wxAlfeaDoorTriggerHost.h"

namespace winx::reconstruction
{
    struct wxAlfeaDoorMessageForAnalysis final
    {
        std::uint32_t code = 0;
        const wxAlfeaDoorTrigger* sender = nullptr; // native +10
        std::uint32_t value18 = 0, value1C = 0;
    };

    // Recovered leaf behavior. Native physical parent is wxPivotingDoor.
    // Until that hierarchy is implemented, its operations/state are an explicit
    // host boundary; portable C++ inheritance only supplies the named-object API.
    class wxAlfeaDoorTrigger : public sparkplug::reconstruction::spNamedObject
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x6AE9310E;
        wxAlfeaDoorTrigger(wxAlfeaDoorTriggerHost&, std::uint32_t initialDoorIdStorage);
        ~wxAlfeaDoorTrigger() override;
        wxAlfeaDoorTrigger(const wxAlfeaDoorTrigger&) = delete;
        wxAlfeaDoorTrigger& operator=(const wxAlfeaDoorTrigger&) = delete;
        static void SetFactoryHostForAnalysis(wxAlfeaDoorTriggerHost*) noexcept;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&,
            sparkplug::reconstruction::spCloneManager&) const override;
        void vfunc_0C(const void*) noexcept override;
        // Names use PS2 table byte offsets, as in other recovered Winx classes.
        virtual bool vfunc_40_CleanupForAnalysis() noexcept;
        virtual bool vfunc_48_UpdateForAnalysis() noexcept;
        virtual bool vfunc_54_CanInteractForAnalysis() noexcept;
        virtual void vfunc_58_AnimateForAnalysis() noexcept;
        virtual void vfunc_5C_EnterForAnalysis() noexcept;
        void QueueOpenForAnalysis(std::uint32_t id, std::uint32_t instant) noexcept;
        bool SpecialGateForAnalysis() noexcept;
        static bool RegisterPropertiesForAnalysis(wxAlfeaDoorTriggerHost&);

        std::uint32_t GetDoorGroupForAnalysis() const noexcept { return group_; }
        std::uint32_t GetDoorIdForAnalysis() const noexcept { return doorId_; }
        void SetDoorGroupForAnalysis(std::uint32_t v) noexcept { group_ = v; }
        void SetDoorIdForAnalysis(std::uint32_t v) noexcept { doorId_ = v; }
        wxPivotingDoorStateForAnalysis& BaseStateForAnalysis() noexcept { return base_; }
        const wxPivotingDoorStateForAnalysis& BaseStateForAnalysis() const noexcept { return base_; }
        struct FlagsForAnalysis final { std::uint8_t locked = 0, queued = 0, instant = 0; };
        const FlagsForAnalysis& GetFlagsForAnalysis() const noexcept { return flags_; }

    private:
        wxAlfeaDoorTriggerHost& host_;
        wxPivotingDoorStateForAnalysis base_;
        std::uint32_t group_ = 2;
        std::uint32_t doorId_; // explicit storage seed, not initialized to a game default
        FlagsForAnalysis flags_;
    };
}
