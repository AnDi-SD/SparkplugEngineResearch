#pragma once

#include "Analysis/Host/spNotificationHost.h"
#include <cstdint>

namespace winx::reconstruction
{
    // Explicit borrowed HUD/window/input boundary. Navigation and its retry
    // conditions remain in the common operation. Singleton startup, window
    // lookup/selection bodies and widget factories are separate operations.
    class wxGameFlowNavigationHost : public sparkplug::reconstruction::spNotificationHost
    {
    public:
        virtual float ReadNavigationMagnitudeForAnalysis(void* cachedInput) = 0;
        virtual float ReadNavigationAngleForAnalysis(void* cachedInput) = 0;
        virtual void* ResolveNavigationWindowForAnalysis(std::uint32_t view) = 0;
        virtual void* ReadInitialNavigationWidgetForAnalysis(void* window) = 0; // +134
        virtual void* ReadSelectedNavigationWidgetForAnalysis(void* window) = 0; // +138
        // Native inline64-byte link fields A4/E4/124/164. Return a borrowed,
        // NUL-terminated buffer with stable address/lifetime through foreign
        // calls. Its contents may change during those calls and are reread.
        virtual const char* ReadNavigationLinkForAnalysis(void* widget, unsigned direction) = 0;
        virtual void* FindNavigationWidgetForAnalysis(void* window, const char* name) = 0;
        virtual std::uint8_t ReadNavigationWidgetEnabledForAnalysis(void* widget) = 0;
        virtual void* ResolveNavigationNotificationReceiverForAnalysis() = 0; // profile+2B4; may be null
        virtual void SelectNavigationWidgetForAnalysis(void* window, void* widget) = 0;
    };
}
