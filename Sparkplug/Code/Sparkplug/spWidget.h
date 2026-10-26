#pragma once
#include "spGUIObject.h"
#include <array>

namespace sparkplug::reconstruction
{
    class spWidget : public spGUIObject
    {
    public:
        static constexpr spClassID ClassID=0x7CB52800;
        explicit spWidget(spGUIHost& host):spGUIObject(host) {}
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        bool vfunc_14(spBaseObject&,spCloneManager&) const override;
        void vfunc_0C(const void*) noexcept override;
        void SetEnabledForAnalysis(std::uint8_t value) override;
        void SetVisibleForAnalysis(std::uint8_t value);
        virtual void RefreshSelectionForAnalysis();
        // Bounded PC protocol, including1C over stable actual Node graphs. Incoming
        // notification identity is preserved across external callbacks.
        void NotifyWidgetForAnalysis(const spGUINotificationForAnalysis&);
        [[nodiscard]] std::uint8_t GetVisibleByteForAnalysis() const noexcept { return visible_; }
        [[nodiscard]] std::uint8_t GetHighlightByteForAnalysis() const noexcept { return highlight_; }
        [[nodiscard]] spNode* GetSelectedNodeForAnalysis() const noexcept { return selected_; }
        [[nodiscard]] std::uint32_t GetSelectionIndexForAnalysis() const noexcept { return index_; }
        [[nodiscard]] const std::array<spNode*,3>& GetStateNodesForAnalysis() const noexcept { return nodes_; }
        // Explicit input of runtime-resolved state. These raw stores do not
        // claim native lookup/binding/notification or acquire ownership.
        void SetSelectionStateForAnalysis(std::uint8_t enabled,std::uint8_t visible,
            std::uint8_t highlight,spNode* selected,
            std::array<spNode*,3> nodes,std::uint32_t index) noexcept;

    protected:
        [[nodiscard]] bool HidePreviousSelectionForAnalysis();
        void SelectAndShowForAnalysis(std::uint32_t index,spNode* node);

    private:
        std::uint8_t visible_=1,highlight_=0;
        spNode* selected_=nullptr;
        std::array<spNode*,3> nodes_{};
        std::uint32_t index_=0;
    };
}
