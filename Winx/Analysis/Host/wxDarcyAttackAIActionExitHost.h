#pragma once
#include <cstdint>
#include <optional>

namespace winx::reconstruction
{
    // Our synchronous borrowed storage view. Native owner144/actor130
    // relationships do not assert original C++ class names or inheritance.
    // Records stay live and stable within each admitted Exit; no reentry.
    class wxDarcyAttackAIActionExitHost
    {
    public:
        enum class ProfileForAnalysis { Unqualified, PcCapturedStableCommand };
        virtual ~wxDarcyAttackAIActionExitHost() = default;
        virtual ProfileForAnalysis ExitProfileForAnalysis() const noexcept
        { return ProfileForAnalysis::Unqualified; }
        virtual std::optional<void*> ReadActor144ForAnalysis(void* owner) noexcept = 0;
        virtual std::optional<void*> ReadCommand130ForAnalysis(void* actor) noexcept = 0;
        virtual void WriteCommandByteForAnalysis(void* command, std::uint32_t offset, std::uint8_t value) noexcept = 0;
    };
}
