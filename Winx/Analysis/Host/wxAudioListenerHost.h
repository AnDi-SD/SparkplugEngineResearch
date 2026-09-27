#pragma once
#include <cstdint>

namespace sparkplug::reconstruction { class spCloneManager; }
namespace winx::reconstruction
{
    class wxAudioListener;

    // The game owns the entity, audio managers, and reference-counted native listener.
    class wxAudioListenerHost
    {
    public:
        virtual ~wxAudioListenerHost() = default;
        virtual void ConstructEntityForAnalysis(wxAudioListener&, bool) = 0;
        virtual void ConstructContainerForAnalysis(wxAudioListener&) = 0;
        virtual void* AcquireListenerForAnalysis(wxAudioListener&) = 0;
        virtual void ReleaseListenerForAnalysis(wxAudioListener&, void*) noexcept = 0;
        virtual void DestroyContainerForAnalysis(wxAudioListener&) noexcept = 0;
        virtual void DestroyEntityForAnalysis(wxAudioListener&) noexcept = 0;
        virtual bool CopyEntityForAnalysis(const wxAudioListener&, wxAudioListener&,
            sparkplug::reconstruction::spCloneManager&) const = 0;
        virtual bool IsAudioTeardownForAnalysis() const noexcept = 0; // PC 765C00
        virtual void StopListenerForAnalysis(wxAudioListener&, void*) noexcept = 0; // PC 4217E0
        virtual void UnregisterListenerForAnalysis(wxAudioListener&, void*) noexcept = 0; // PC 48F0C0
        virtual bool HasLinkedEntityForAnalysis(const wxAudioListener&) const noexcept = 0; // entity +24
        virtual void UnlinkEntityForAnalysis(wxAudioListener&) noexcept = 0; // PC 40FB60
        virtual void AttachEntityForAnalysis(wxAudioListener&) = 0; // PC 410050
        virtual void RegisterListenerForAnalysis(wxAudioListener&, void*) = 0; // PC 421A60
        virtual void ActivateListenerForAnalysis(wxAudioListener&, void*) = 0; // PC 48FAD0
    };
}
