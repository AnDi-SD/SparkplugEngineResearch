#pragma once
#include <cstdint>
#include <optional>

namespace winx::reconstruction
{
    class wxAICommandRecordForAnalysis;
    // Our borrowed storage view, stable within each synchronous Exit.
    // The foreign profile class, native extent and global producer are open.
    // The last getter returns a live reference to its measured byte510.
    class wxIcyAttackAIActionExitHost
    {
    public:
        enum class ProfileForAnalysis { Unqualified, PcCapturedStableCommandAndExistingProfile };
        virtual ~wxIcyAttackAIActionExitHost() = default;
        virtual ProfileForAnalysis ExitProfileForAnalysis() const noexcept
        { return ProfileForAnalysis::Unqualified; }
        virtual std::optional<void*> ReadActor144ForAnalysis(void* owner) noexcept = 0;
        virtual std::optional<wxAICommandRecordForAnalysis*> ReadCommand130ForAnalysis(void* actor) noexcept = 0;
        // Called after the actual shared command Reset has completed.
        // Null is analytical missing-storage admission, not a cold constructor.
        virtual std::uint8_t* ReadExistingProfileByte510ForAnalysis() noexcept = 0;
    };
}
