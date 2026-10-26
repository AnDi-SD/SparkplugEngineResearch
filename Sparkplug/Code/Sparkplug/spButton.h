#pragma once
#include "spWidget.h"
#include <string>
#include <utility>

namespace sparkplug::reconstruction
{
    class spButton : public spWidget
    {
    public:
        static constexpr spClassID ClassID=0x2CD270BB;
        explicit spButton(spGUIHost& host):spWidget(host) {}
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        bool vfunc_14(spBaseObject&,spCloneManager&) const override;
        void vfunc_0C(const void*) noexcept override;
        void RefreshSelectionForAnalysis() override;
        void NotifyButtonForAnalysis(const spGUINotificationForAnalysis&);
        [[nodiscard]] const std::string& GetStringForAnalysis() const noexcept { return string_; }
        [[nodiscard]] std::uint8_t GetPressedByteForAnalysis() const noexcept { return pressed_; }
        [[nodiscard]] spNode* GetPressedNodeForAnalysis() const noexcept { return pressedNode_; }
        // Host-supplied populated source for the recovered copy/selection calls.
        // No button message generation or original setter is inferred.
        void SetButtonStateForAnalysis(std::string text,std::uint8_t pressed,spNode* node)
        { string_=text.c_str();pressed_=pressed;pressedNode_=node; }
    private:
        std::string string_; // Native owned empty C string; ctor allocates2 bytes.
        std::uint8_t pressed_=0;
        spNode* pressedNode_=nullptr;
    };
}
