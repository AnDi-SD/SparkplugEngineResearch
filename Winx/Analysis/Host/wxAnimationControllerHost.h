#pragma once
#include "Code/Sparkplug/spActor.h"
#include <cstdint>

namespace winx::reconstruction
{
    class wxAnimationController;
    struct wxAnimationMessageForAnalysis
    {
        std::uint32_t code = 0;
        const void* payload18 = nullptr;
        const sparkplug::reconstruction::spAnimation* animation1C = nullptr;
        const void* originalContext = nullptr; // optional full external message
    };
    struct wxAnimationActorFlagsForAnalysis
    {
        std::uint8_t enabled1C = 0, changed24 = 0;
    };
    // Mandatory engine/entity boundary. Views and handles are our adapter API,
    // not a replacement implementation of spActor or the wxEntity hierarchy.
    class wxAnimationControllerHost
    {
    public:
        using Animation = sparkplug::reconstruction::spAnimation;
        using Request = sparkplug::reconstruction::spActor::StartRequestForAnalysis;
        virtual ~wxAnimationControllerHost() = default;
        virtual void ConstructEntityForAnalysis(wxAnimationController&, bool) = 0;
        virtual void DestroyEntityForAnalysis(wxAnimationController&) noexcept = 0;
        virtual bool CopyEntityForAnalysis(const wxAnimationController&, wxAnimationController&,
            sparkplug::reconstruction::spCloneManager&) const = 0;
        virtual void* CreateActorForAnalysis() = 0;
        virtual void DestroyActorForAnalysis(void*) noexcept = 0;
        virtual void StopAllForAnalysis(void*) noexcept = 0;
        virtual void BindActorForAnalysis(void* actor, void* root) = 0;
        virtual void AddActorObserverForAnalysis(void*, wxAnimationController&) = 0;
        virtual void* GetEntityRootForAnalysis(void* entity) noexcept = 0;
        virtual std::uint32_t GetEntityFlagsForAnalysis(void* entity) noexcept = 0;
        virtual void* FindCharacterForAnalysis(void* entity, std::uint32_t classID) = 0;
        virtual bool CharacterField148NonzeroForAnalysis(void*) noexcept = 0;
        virtual wxAnimationActorFlagsForAnalysis& ActorFlagsForAnalysis(void*) noexcept = 0;
        virtual std::uint32_t StartActorForAnalysis(void*, Request&) = 0;
        virtual void StopActorForAnalysis(void*, const Animation*, bool suppressEvent) = 0;
        virtual void FadeStopActorForAnalysis(void*, const Animation*, float duration, float fallbackRate) = 0;
        virtual bool FindPlaybackTimesForAnalysis(void*, const Animation*, float& field54, float& field34) = 0;
        virtual void SetActorTimeMultiplierForAnalysis(void*, float) = 0;
        virtual void ForwardForAnalysis(wxAnimationController&, const wxAnimationMessageForAnalysis&) noexcept = 0;
        virtual void DispatchStateTagForAnalysis(void* character, const wxAnimationMessageForAnalysis&) noexcept = 0;
        virtual void* GetAudioEmitterForAnalysis(void* character) noexcept = 0;
        virtual std::uint8_t GetAudioVariantForAnalysis(void* character) noexcept = 0;
        virtual void DispatchAudioTagForAnalysis(void* audio, const wxAnimationMessageForAnalysis&, std::uint8_t variant) noexcept = 0;
    };
}
