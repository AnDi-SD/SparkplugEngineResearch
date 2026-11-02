#pragma once
#include <optional>

namespace winx::reconstruction
{
    class wxAICommandRecordForAnalysis;
    // Borrowed synchronous storage resolution only. Source calls the actual
    // common record algorithm; host does not reset it or return Exit success.
    class wxStormyAttackAIActionExitHost
    {
    public:
        enum class ProfileForAnalysis { Unqualified, PcCapturedStableCommand };
        virtual ~wxStormyAttackAIActionExitHost() = default;
        virtual ProfileForAnalysis ExitProfileForAnalysis() const noexcept
        { return ProfileForAnalysis::Unqualified; }
        virtual std::optional<void*> ReadActor144ForAnalysis(void* owner) noexcept = 0;
        virtual std::optional<wxAICommandRecordForAnalysis*> ReadCommand130ForAnalysis(void* actor) noexcept = 0;
    };
}
