#pragma once

namespace winx::reconstruction
{
    class wxAIActionCommandExitHost;
    class wxGameTimer;
    enum class wxYetiAttackAIActionPhaseProfileForAnalysis
    { Unqualified, PcCapturedStablePhase };
    enum class wxYetiAttackAIActionPhaseResultForAnalysis
    {
        Completed, UnqualifiedProfile, MissingOwner, MissingOwnerStorage,
        MissingActorStorage, MissingCommandStorage, CommandNotAdmitted,
        MissingExistingTimer,
    };
    // Our borrowed data binding, separate from Exit admission. Inputs remain
    // live and stable within a call. The timer is read only on the native
    // zero-deadline branch; no clock sample or phase result callback exists.
    struct wxYetiAttackAIActionPhaseBindingsForAnalysis final
    {
        wxYetiAttackAIActionPhaseProfileForAnalysis profile =
            wxYetiAttackAIActionPhaseProfileForAnalysis::Unqualified;
        wxAIActionCommandExitHost* ownerStorage = nullptr;
        wxGameTimer* existingTimer = nullptr;
    };
}
