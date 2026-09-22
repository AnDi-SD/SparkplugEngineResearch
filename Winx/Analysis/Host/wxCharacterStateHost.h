#pragma once

#include <cstdint>

namespace winx::reconstruction
{
    // Explicit host boundary for external game objects. All pointers are
    // borrowed. No successful default stubs stand in for native callees.
    class wxCharacterStateHost
    {
    public:
        virtual ~wxCharacterStateHost() = default;
        // PC 59A5E0 lookup, using owner->field128 and the lazy manager accessor.
        virtual void* ResolveAnimationForAnalysis(void* owner, std::uint32_t key) = 0;
        // Owner PC vtable +38 / PS2 +40: original source name unknown.
        virtual bool OwnerPredicateForAnalysis(void* owner) = 0;
        // PC 4FB400: clear matching completion records before mode-zero play.
        virtual void ResetCompletionForAnalysis(void* consumer, void* handle) = 0;
        // PC 4FB620 / PS2 2A6CF0, including exact fade selector 0 or 2.
        virtual void StartAnimationForAnalysis(void* consumer, void* handle,
            bool mode, std::uint32_t fadeSelector, bool interrupt) = 0;
        virtual void StopAnimationForAnalysis(void* consumer, void* handle) = 0;
        virtual void FadeAnimationForAnalysis(void* consumer, void* handle, float duration) = 0;
        // PC 4FB330 / PS2 2A6C30 can consume completion records with flag=true.
        virtual bool IsPendingAnimationCompleteForAnalysis(void* consumer,
            void* handle, bool flag) = 0;
        // PC *(owner->field12C + 4)=0; PS2 uses owner->field138.
        virtual void ClearOwnerActionControlForAnalysis(void* owner) = 0;
    };
}
