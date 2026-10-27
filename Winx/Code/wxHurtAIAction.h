#pragma once
#include "Code/wxAIAction.h"
#include "Analysis/Host/wxHurtAIActionHost.h"

namespace winx::reconstruction
{
    enum class wxHurtPlatformForAnalysis { PC, PS2 };
    enum class wxHurtUpdateProfileForAnalysis { IntegerHooksOnly, PcSmallIntegerRange };
    enum class wxHurtResultForAnalysis
    {
        Completed,
        SelectorRequested,
        UnqualifiedRangeHelper,
        UnqualifiedNumericDomain,
        UnqualifiedPerceptionRefresh,
    };

    class wxHurtAIAction final : public wxAIAction
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID=0x4D8057F6;
        explicit wxHurtAIAction(wxHurtAIActionHost&,
            wxHurtPlatformForAnalysis=wxHurtPlatformForAnalysis::PC,
            wxHurtUpdateProfileForAnalysis=wxHurtUpdateProfileForAnalysis::IntegerHooksOnly) noexcept;
        static void SetFactoryHostForAnalysis(wxHurtAIActionHost*) noexcept;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&,
            sparkplug::reconstruction::spCloneManager&) const override;
        void vfunc_0C(const void*) noexcept override {} // literal no-op, no forwarding
        bool vfunc_24() noexcept override;
        void vfunc_30() noexcept override { EnterForAnalysis(0); }
        // Native Entry has one 32-bit argument; the common no-argument virtual
        // adapter chooses0 explicitly. Use this API for the complete argument.
        void EnterForAnalysis(std::uint32_t) noexcept;
        void vfunc_34_ClearForAnalysis() noexcept override;
        void vfunc_38() noexcept override {} // own no-op Query
        [[nodiscard]] wxHurtResultForAnalysis UpdateForAnalysis() noexcept;
        [[nodiscard]] wxHurtResultForAnalysis ExtendRangeForAnalysis() noexcept;
        wxHurtResultForAnalysis LastResultForAnalysis() const noexcept { return lastResult_; }
        std::uint8_t LatchForAnalysis() const noexcept { return latch_; }
        void SetLatchForAnalysis(std::uint8_t value) noexcept { latch_=value; }
    private:
        wxHurtAIActionHost& host_;
        wxHurtPlatformForAnalysis platform_;
        wxHurtUpdateProfileForAnalysis profile_;
        std::uint8_t latch_=0;
        wxHurtResultForAnalysis lastResult_=wxHurtResultForAnalysis::Completed;
    };
}
