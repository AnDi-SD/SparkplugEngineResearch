#pragma once
#include "wxEntity.h"
#include "wxButton.h"
#include "Analysis/Host/wxHUDPartHost.h"
#include <array>
#include <vector>

namespace sparkplug::reconstruction { class spSubscriptionManager; }

namespace winx::reconstruction
{
    class wxHUDPart : public wxEntity
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID=0x7F2E3F1A;
        wxHUDPart(wxHUDPartHost&,sparkplug::reconstruction::spSubscriptionManager&);
        ~wxHUDPart() override;
        static void SetFactoryContextForAnalysis(wxHUDPartHost*,
            sparkplug::reconstruction::spSubscriptionManager*) noexcept;
        [[nodiscard]] static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        bool vfunc_14(sparkplug::reconstruction::spBaseObject&,
            sparkplug::reconstruction::spCloneManager&) const override;
        void vfunc_0C(const void*) noexcept override;
        void NotifyHUDPartForAnalysis(const sparkplug::reconstruction::spNotificationForAnalysis&);
        [[nodiscard]] wxButton* FindButtonForAnalysis(const char* action) const;
        void SelectButtonForAnalysis(wxButton*);
        [[nodiscard]] const std::vector<wxButton*>& GetButtonsForAnalysis() const noexcept { return buttons_; }
        [[nodiscard]] std::vector<wxButton*>& GetButtonsForAnalysis() noexcept { return buttons_; }
        [[nodiscard]] wxButton* GetInitialButtonForAnalysis() const noexcept { return initial_; }
        [[nodiscard]] wxButton* GetSelectedButtonForAnalysis() const noexcept { return selected_; }
        [[nodiscard]] std::uint32_t GetPartKeyForAnalysis() const noexcept { return key_; }
        [[nodiscard]] std::uint32_t GetLayerWordForAnalysis() const noexcept { return layer_; }
        [[nodiscard]] const std::array<std::uint8_t,3>& GetFlagsForAnalysis() const noexcept { return flags_; }
        // Explicit populated application input, not a recovered setter, init,
        // manager registration or ownership acquisition operation.
        void SetHUDStateForAnalysis(std::vector<wxButton*> buttons,wxButton* initial,
            wxButton* selected,std::uint32_t key,std::uint32_t layer,
            std::array<std::uint8_t,3> flags);
    private:
        wxHUDPartHost& host_;
        // Application-owned, borrowed context. It must remain the current
        // manager and outlive this Part; lazy native startup is not invented.
        sparkplug::reconstruction::spSubscriptionManager& subscriptions_;
        std::vector<wxButton*> buttons_;
        wxButton* initial_=nullptr;
        wxButton* selected_=nullptr;
        std::uint32_t key_=0,layer_=0;
        std::array<std::uint8_t,3> flags_{};
    };
}
