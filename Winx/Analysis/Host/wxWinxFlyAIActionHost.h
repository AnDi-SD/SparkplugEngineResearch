#pragma once
#include <cstdint>
#include <optional>

namespace winx::reconstruction
{
    class wxCharacterRegistry;
    // Live borrowed storage bindings only. Owner/actor/command source types
    // and the native global Registry producer are not reconstructed here.
    class wxWinxFlyAIActionHost
    {
    public:
        enum class ProfileForAnalysis { Unqualified, PcCapturedExistingRegistryEntry };
        virtual ~wxWinxFlyAIActionHost() = default;
        virtual ProfileForAnalysis EntryProfileForAnalysis() const noexcept
        { return ProfileForAnalysis::Unqualified; }
        // owner144 -> actor130 -> command. Engaged null is admitted native
        // command data and skips the command write; nullopt is missing storage.
        virtual std::optional<void*> ReadCommandForAnalysis(void* capturedOwner) noexcept = 0;
        virtual void WriteCommandWordForAnalysis(void* capturedCommand,
            std::uint32_t offset, std::uint32_t value) noexcept = 0;
        // Fresh existing binding after the optional command store. This returns
        // the common Registry object, never a substituted selected-target result.
        virtual wxCharacterRegistry* ReadExistingRegistryForAnalysis() noexcept = 0;
    };
}
