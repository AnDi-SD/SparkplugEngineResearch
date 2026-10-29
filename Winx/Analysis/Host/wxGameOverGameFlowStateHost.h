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
