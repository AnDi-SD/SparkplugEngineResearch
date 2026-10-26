#pragma once

#include "Code/Sparkplug/spButton.h"
#include "Analysis/Host/wxButtonHost.h"
#include <array>
#include <cstdint>

namespace winx::reconstruction
{
    // Bounded PC own state/selection and lifecycle body. This portable subtype
    // is not an overlay of either native ABI. Own Notify is qualified only for
    // PC1C/payload1 with an existing actual common SubscriptionManager.
    class wxButton : public sparkplug::reconstruction::spButton
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID=0x69F878BA;
        // Analytical PC-offset view+60..1A8. The supplied allocation bytes
        // make native-unwritten portions explicit inputs. Default zero storage
        // is a portable choice, not a native allocator initialization claim.
        using OwnBytesForAnalysis=std::array<std::uint8_t,0x148>;
        explicit wxButton(wxButtonHost&,OwnBytesForAnalysis allocationBytes={});
        ~wxButton() override;
        static void SetFactoryHostForAnalysis(wxButtonHost*) noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&,
            sparkplug::reconstruction::spCloneManager&) const override;
        void vfunc_0C(const void*) noexcept override;

        // PC own slot30:0->enabled1/highlight0;1->enabled0;2->both1.
        // Other words leave both bytes unchanged. Every case calls virtual2C.
        virtual void SetStateForAnalysis(std::uint32_t);
        void RefreshSelectionForAnalysis() override;
        [[nodiscard]] std::uint8_t GetRefreshGateByteForAnalysis() const noexcept { return own_[0x144]; }
        [[nodiscard]] OwnBytesForAnalysis& GetOwnBytesForAnalysis() noexcept { return own_; }
        [[nodiscard]] const OwnBytesForAnalysis& GetOwnBytesForAnalysis() const noexcept { return own_; }

    private:
        void StoreHighlightForAnalysis(std::uint8_t) noexcept;
        void StoreSelectedForAnalysis(sparkplug::reconstruction::spNode*) noexcept;
        void StoreSelectionForAnalysis(std::uint32_t,sparkplug::reconstruction::spNode*) noexcept;
        wxButtonHost& host_;
        OwnBytesForAnalysis own_;
    };
}
