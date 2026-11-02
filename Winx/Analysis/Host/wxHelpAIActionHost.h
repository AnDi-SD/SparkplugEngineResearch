#pragma once
#include <cstdint>
#include <optional>

namespace winx::reconstruction
{
    // Storage-only borrowed owner+144 -> actor+130 -> command protocol.
    // No original owner/actor/command C++ type is inferred. All records stay
    // live for the synchronous call. nullopt is a missing wrapper; an engaged
    // null is captured native null data, which this Entry does not admit.
    class wxHelpAIActionHost
    {
    public:
        enum class ProfileForAnalysis { Unqualified, PcCapturedBorrowedEntryData };
        virtual ~wxHelpAIActionHost() = default;
        virtual ProfileForAnalysis EntryProfileForAnalysis() const noexcept
        { return ProfileForAnalysis::Unqualified; }
        virtual std::optional<void*> ReadCommandForAnalysis(void* capturedOwner) noexcept = 0;
        // Declared input rebinding may occur at the start of this write,
        // after command capture. The supplied command remains the destination.
        virtual void WriteCommandWordForAnalysis(void* capturedCommand,
            std::uint32_t offset, std::uint32_t value) noexcept = 0;
    };
}
