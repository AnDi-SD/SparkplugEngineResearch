#pragma once
#include <optional>

namespace winx::reconstruction
{
    class wxAICommandRecordForAnalysis;
    enum class wxAIActionCommandExitProfileForAnalysis { Unqualified, PcCapturedStableCommand };
    enum class wxAIActionCommandExitResultForAnalysis
    {
        Completed, UnqualifiedProfile, MissingOwner, MissingActorStorage,
        MissingCommandStorage, CommandNotAdmitted, UnqualifiedNotify, UnqualifiedHook,
    };
    // Storage-only contract for the captured Golem/Yeti Exit consumers.
    // Native owner144/actor130 offsets do not assert foreign C++ inheritance.
    // Host and all returned records stay live and stable within each call.
    class wxAIActionCommandExitHost
    {
    public:
        virtual ~wxAIActionCommandExitHost() = default;
        virtual std::optional<void*> ReadActor144ForAnalysis(void* owner) noexcept = 0;
        virtual std::optional<wxAICommandRecordForAnalysis*> ReadCommand130ForAnalysis(void* actor) noexcept = 0;
    };
}
