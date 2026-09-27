#pragma once
#include <cstdint>
#include <vector>

namespace sparkplug::reconstruction { class spCloneManager; }
namespace winx::reconstruction
{
    class wxAudioEmitter;

    // This view represents the selected native map node, not its byte layout.
    struct wxAudioEntryForAnalysis final
    {
        std::uint32_t kind = 0;            // PC node +10
        std::uint32_t previousIndex = 0;   // PC node +14
        std::vector<const void*> sounds;
    };

    class wxAudioEmitterHost
    {
    public:
        virtual ~wxAudioEmitterHost() = default;
        virtual void ConstructEntityForAnalysis(wxAudioEmitter&, bool) = 0;
        virtual void ConstructAudioStateForAnalysis(wxAudioEmitter&) = 0;
        virtual void DestroyAudioStateForAnalysis(wxAudioEmitter&) noexcept = 0;
        virtual void DestroyEntityForAnalysis(wxAudioEmitter&) noexcept = 0;
        virtual bool CopyEntityForAnalysis(const wxAudioEmitter&, wxAudioEmitter&,
            sparkplug::reconstruction::spCloneManager&) const = 0;
        virtual void CopyOwnedResourceForAnalysis(const wxAudioEmitter&, wxAudioEmitter&) const = 0;
        virtual std::int32_t GameStateForAnalysis() const = 0;
        // Native lookup uses the unusual comparator exposed by CompareKeysForAnalysis.
        virtual wxAudioEntryForAnalysis* FindEntryForAnalysis(
            wxAudioEmitter&, const char* name, std::uint32_t variant,
            std::uint32_t constant) = 0;
        virtual std::uint32_t RandomForAnalysis() = 0;
        virtual void PlayKind10ForAnalysis(const void* firstSound) = 0;
        virtual void PlaySoundForAnalysis(const void* sound) = 0;
        virtual void InitializeForAnalysis(wxAudioEmitter&) noexcept = 0;
        virtual void HandleCode30ForAnalysis(wxAudioEmitter&) noexcept = 0;
        virtual void SetAudioActiveForAnalysis(wxAudioEmitter&, bool active) noexcept = 0;
        virtual void HandleOtherNotificationForAnalysis(wxAudioEmitter&, const void*) noexcept = 0;
    };
}
