#pragma once
#include <cstdint>
#include <optional>

namespace sparkplug::reconstruction { class spNode; }
namespace winx::reconstruction
{
    // Storage-only supplied records. All owner/entity/actor/command records and
    // exact plain Nodes remain live through the synchronous operation. Engaged
    // null is a captured payload; nullopt means an unqualified/missing wrapper.
    class wxGhoulScriptAIActionHost
    {
    public:
        enum class ProfileForAnalysis { Unqualified, PcCapturedBorrowedEntryData };
        virtual ~wxGhoulScriptAIActionHost() = default;
        virtual ProfileForAnalysis EntryProfileForAnalysis() const noexcept
        { return ProfileForAnalysis::Unqualified; }
        virtual std::optional<sparkplug::reconstruction::spNode*>
            ReadOwnerNodeForAnalysis(void* capturedOwner) noexcept = 0;
        // Declared input changes may occur at START of this read, after the
        // action's Node cache and before this same cached owner's actor read.
        virtual std::optional<void*> ReadCommandForAnalysis(void* capturedOwner) noexcept = 0;
        virtual void WriteOwnerByteForAnalysis(void* capturedOwner,
            std::uint32_t offset, std::uint8_t value) noexcept = 0;
    };
}
