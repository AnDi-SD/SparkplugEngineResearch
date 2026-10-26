#pragma once

#include "Code/Sparkplug/spTaskTimer.h"
#include <array>

namespace winx::reconstruction
{
    // Portable storage for the three distinct original PC global arrays.
    // Tokens may represent native string addresses; this layer never owns them.
    struct wxGameFlowResourcesForAnalysis final
    {
        std::array<const void*, 56> sourceNames{}; // original PC 742DE4 order
        std::array<const void*, 56> names{};       // original PC 768110
        std::array<void*, 56> cache{};             // original PC 768250
    };

    // External input singleton and global clock remain required dependencies.
    // This interface does not substitute resource loading or menu navigation.
    class wxGameFlowStateHost
    {
    public:
        virtual ~wxGameFlowStateHost() = default;
        virtual void* BorrowInputForAnalysis() = 0; // singleton member18
        virtual sparkplug::reconstruction::spTaskTimer::ClockReadingForAnalysis
            ReadClockForAnalysis() = 0;
        virtual wxGameFlowResourcesForAnalysis& ResourcesForAnalysis() = 0;
    };
}
