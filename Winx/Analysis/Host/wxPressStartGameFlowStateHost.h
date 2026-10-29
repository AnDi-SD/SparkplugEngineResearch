#pragma once
#include "wxMenuGameFlowStateHost.h"

namespace winx::reconstruction
{
    // Borrowed structural input binding, not an implementation of device
    // polling. Every call is a separate observation, including repeated loads
    // in one Tick. No input ownership or reference transfer is implied.
    class wxPressStartGameFlowStateHost : public wxMenuGameFlowStateHost
    {
    public:
        virtual std::uint8_t ReadPressStartCachedInputByte1BForAnalysis(void* input)=0;
        enum class ExitProfileForAnalysis {Unqualified,PCStableMenuAndSubscriptions};
        // Caller supplies live stable Menu foreign bindings and an existing
        // shared SubscriptionManager, with stable subscriptions while sending.
        // Callbacks may mutate the borrowed packet/observed fields. Manager
        // replacement, list mutation/reentry and cold creation stay outside.
        // Source separately admits owned4C null or final empty/null-name level
        // before any parent effect. No native receiver/high24 binding is used.
        virtual ExitProfileForAnalysis PressStartExitProfileForAnalysis() const noexcept
        {return ExitProfileForAnalysis::Unqualified;}
        enum class EntryProfileForAnalysis {Unqualified,PCEmptyOwnedGameLevel};
        // Caller attests live, stable Core/Scene bindings to an actual empty
        // PCProjectionManager and a plain detached Node with the explicitly
        // captured zero local orientation and dirty transform. No general
        // native matrix startup is qualified. The parent executes
        // their real bodies; this guard is never a parent success callback.
        // Source separately checks final owned GameLevel/null-name/empty-list.
        virtual EntryProfileForAnalysis PressStartEntryProfileForAnalysis() const noexcept
        {return EntryProfileForAnalysis::Unqualified;}
        virtual void InvokePressStartWindowConsumer55D570ForAnalysis(void*,std::uint32_t)
        {throw std::logic_error("PressStart Entry foreign window55D570 is unbound");}
        virtual void* ResolvePressStartDXInputManagerForAnalysis()
        {throw std::logic_error("PressStart Entry DXInputManager binding is missing");}
        virtual void* InvokePressStartDXInputManagerView18Slot0CForAnalysis(void*)
        {throw std::logic_error("PressStart Entry foreign DXInput view18/slot0C is unbound");}
        virtual void SetPressStartDXInputDeviceSlot20ForAnalysis(void*,std::uint32_t)
        {throw std::logic_error("PressStart Entry foreign input device slot20 is unbound");}
        // A synchronous live structural field borrow, followed by source =1;
        // this is not a recovered wxUserInput method or an extra callback.
        virtual std::uint8_t& BorrowPressStartInputOwnerFlag20ForAnalysis(void*)
        {throw std::logic_error("PressStart Entry owner flag20 binding is missing");}
        virtual std::uint32_t ReadPressStartTimerClockForAnalysis()
        {throw std::logic_error("PressStart Entry WINMM clock binding is missing");}
        // Parent Menu's legacy Audio-named callbacks refer to the confirmed
        // spDXInputManager embedded input view/device boundary. Admitted Entry
        // and Exit reach them through the real Menu parent; own Entry uses the
        // explicit DXInput names above. Their foreign bodies stay unqualified.
    };
}
