#pragma once
#include "wxMenuGameFlowStateHost.h"

namespace winx::reconstruction
{
    // Synchronous borrowed data observations, not device polling or a
    // wxUserInput implementation. Existing Menu1B/flow methods are reused.
    class wxGameOverGameFlowStateHost : public wxMenuGameFlowStateHost
    {
    public:
        enum class TickProfileForAnalysis{Unqualified,PCStableBorrowedInputAndFlow};
        enum class EntryProfileForAnalysis{Unqualified,PCEmptyOwnedGameLevel};
        enum class ExitProfileForAnalysis{Unqualified,PCStableMenuBindings};
        enum class Slot28ProfileForAnalysis{Unqualified,PCStableGUIBindings};
        enum class Slot2CProfileForAnalysis{Unqualified,PCStableInputGUIBindings};
        enum class Slot38ProfileForAnalysis{Unqualified,PCStableOwnedLifetime};
        enum class Slot40ProfileForAnalysis{Unqualified,PCStableDisjointOwnedPath};
        // Start with null3C; subsequent buffers are owned results of earlier
        // admitted40 calls on the same object. Null4C, both real timers and
        // borrowed inputs remain alive. Supplied paths are live NUL-terminated
        // arrays, disjoint from owned buffers and stable during storage calls.
        // Storage succeeds for at most64 supplied bytes; callbacks neither
        // mutate receiver/path state nor reenter. Alias, growth, allocation
        // failure, path loading and other hook profiles remain unqualified.
        virtual Slot40ProfileForAnalysis GameOverSlot40ProfileForAnalysis() const noexcept
        {return Slot40ProfileForAnalysis::Unqualified;}
        // Own wrapper38 calls the real common literal38 body, then returns
        // true. The bounded PC domain keeps both parent timers alive and
        // own3C/4C null; cached5C is a borrowed, possibly null decoy. No input
        // polling, GUI dependency or other active-hook admission follows.
        virtual Slot38ProfileForAnalysis GameOverSlot38ProfileForAnalysis() const noexcept
        {return Slot38ProfileForAnalysis::Unqualified;}
        // Separate PC2C admission: input owners/records/hooks, GUI/window,
        // DXInput views/devices and structural shared45 records stay live.
        // Own3C/4C remain null. Observations/callbacks may change borrowed
        // globals and measured view/mode/25/input bytes; no foreign body,
        // cold factory, concurrent observation or native fault is modeled.
        virtual Slot2CProfileForAnalysis GameOverSlot2CProfileForAnalysis() const noexcept
        {return Slot2CProfileForAnalysis::Unqualified;}
        // Existing GUI and returned window stay live during synchronous
        // observations. Callbacks may replace globals and mutate state.
        // The bounded ownership domain has null own3C/4C; this admission is
        // analytical, not a native read by inherited28. Foreign GUI bodies
        // and cold creation remain unqualified.
        virtual Slot28ProfileForAnalysis GameOverSlot28ProfileForAnalysis() const noexcept
        {return Slot28ProfileForAnalysis::Unqualified;}
        // All borrowed context/target/GUI/DXInput/service/input records remain
        // alive for their observations. Callbacks may replace global bindings
        // and mutate fields. Own4C is null or an exact final empty/null-name
        // GameLevel, checked before effects as ownership admission. The real
        // inherited Menu body runs; unknown foreign bodies stay unqualified.
        virtual ExitProfileForAnalysis GameOverExitProfileForAnalysis() const noexcept
        {return ExitProfileForAnalysis::Unqualified;}
        // Caller attests stable existing Core/Scene with an actual empty PC
        // ProjectionManager and a plain detached Node: captured nine-zero
        // local orientation and dirty transform, not general matrix startup.
        // Source separately checks its final empty/null-name owned GameLevel.
        // Actual parent bodies execute. GUI, DXInput and input1C stay foreign.
        virtual EntryProfileForAnalysis GameOverEntryProfileForAnalysis() const noexcept
        {return EntryProfileForAnalysis::Unqualified;}
        // Live structural byte20 borrow followed by source =1. This is not
        // an invented wxUserInput method or an operation-success callback.
        virtual std::uint8_t& BorrowGameOverInputOwnerFlag20ForAnalysis(void*)
        {throw std::logic_error("GameOver owner flag20 structural binding is missing");}
        // Entry reuses Menu's legacy Audio-named APIs for the confirmed
        // foreign DXInputManager embedded view/device, without audio/body
        // qualification. Later missing bindings preserve completed prefixes;
        // analytical refusal does not model a native fault or rollback.
        virtual TickProfileForAnalysis GameOverTickProfileForAnalysis() const noexcept
        {return TickProfileForAnalysis::Unqualified;}
        virtual std::uint8_t ReadGameOverInputByte2AForAnalysis(void*)
        {throw std::logic_error("GameOver borrowed input byte2A is unbound");}
        // Caller attests live input records and an existing foreign flow
        // binding. Callbacks may mutate own state or global bindings; source
        // preserves native capture/reload order. No flow-body qualification.
        // A missing flow after25clear leaves that completed prefix intact;
        // analytical refusal does not model native lazy creation or rollback.
    };
}
