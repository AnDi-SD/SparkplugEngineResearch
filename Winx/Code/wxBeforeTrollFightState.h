#pragma once

#include "wxCharacterState.h"
#include "Analysis/Host/wxBeforeTrollFightStateHost.h"

namespace winx::reconstruction
{
    // Normalized host packet, not the native 32-bit notification ABI.
    struct wxBeforeTrollNotificationForAnalysis final
    {
        std::uint32_t code = 0;
        std::uint32_t word18 = 0;
        void* handle1C = nullptr;
    };

    class wxBeforeTrollFightState final : public wxCharacterState
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x3EFE78B7;
        static constexpr std::uint32_t StateSelector = 8;
        wxBeforeTrollFightState() noexcept;
        ~wxBeforeTrollFightState() override;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject>
            vfunc_10(sparkplug::reconstruction::spCloneManager& manager) const override;
        [[nodiscard]] bool vfunc_1C(wxAnimationRequestForAnalysis& request) override;
        void vfunc_30(wxAnimationRequestForAnalysis& request) override;
        [[nodiscard]] bool vfunc_34(std::uint32_t code) override;
        void vfunc_0C(const void* notification) noexcept override;

        // Explicit portable lifetime: factory/clone products are disconnected
        // analysis objects until bound. This is not native unbound destruction.
        // Once bound, destruction always removes both subscriptions, even if
        // entry/update were never called. Rebinding an active object is an error.
        void BindForAnalysis(void* owner, void* consumer, wxBeforeTrollFightStateHost& host);
        struct FieldsForAnalysis final
        {
            std::uint32_t deadline;
            void* handle40;
            void* handle44;
            void* handle48;
        };
        [[nodiscard]] FieldsForAnalysis GetFieldsForAnalysis() const noexcept
        { return {deadline_, handle40_, handle44_, handle48_}; }
        void SetFieldsForAnalysis(const FieldsForAnalysis& fields) noexcept
        { deadline_ = fields.deadline; handle40_ = fields.handle40; handle44_ = fields.handle44; handle48_ = fields.handle48; }

    private:
        [[nodiscard]] wxBeforeTrollFightStateHost& RequireBoundHost() const;
        [[nodiscard]] void* ChooseHandle(wxBeforeTrollFightStateHost& host);
        void SetDeadline(wxBeforeTrollFightStateHost& host);
        std::uint32_t deadline_ = 0;
        void* handle40_ = nullptr;
        void* handle44_ = nullptr;
        void* handle48_ = nullptr;
        // Host lifetime bookkeeping, absent from the native object layout.
        wxBeforeTrollFightStateHost* lifetimeHost_ = nullptr;
        void* lifetimeConsumer_ = nullptr;
    };
}
