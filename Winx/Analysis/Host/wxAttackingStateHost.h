#pragma once

#include "wxCharacterStateHost.h"

#include <cstdint>
#include <string_view>

namespace winx::reconstruction
{
    // Portable boundary for owner fields, global attack mode and event targets.
    // All pointers are borrowed; the original C++ types remain unidentified.
    class wxAttackingStateHost : public wxCharacterStateHost
    {
    public:
        virtual bool IsAttackOverrideActiveForAnalysis() = 0;
        virtual float GetAttackMotionForAnalysis(void* owner) = 0;
        virtual float GetAttackAngleForAnalysis(void* owner) = 0;
        virtual std::uint32_t GetOwnerModeForAnalysis(void* owner) = 0;
        virtual std::uint32_t GetOwnerActionCountForAnalysis(void* owner) = 0;
        virtual std::uint32_t GetFirstOwnerActionForAnalysis(void* owner) = 0;

        virtual std::string_view GetEventNameForAnalysis(const void* event) = 0;
        virtual bool IsOrbImmediateForAnalysis(void* owner) = 0;
        virtual bool CanTriggerOrbForAnalysis(void* owner, float value) = 0;
        virtual void TriggerEventForAnalysis(void* owner, std::uint32_t code) = 0;
        virtual std::uint32_t GetSnowballCountForAnalysis() = 0;
        // Original code writes the new count and notifies its global consumer.
        virtual void SetSnowballCountForAnalysis(std::uint32_t count) = 0;
    };
}
