#include "spPCLensFlareManager.h"

namespace sparkplug::reconstruction
{
    namespace
    {
        constexpr std::uint32_t CapturedPrimary = 0x006F2A5C;
        constexpr std::uint32_t OcclusionQuery = 9;
        constexpr std::uint32_t QueryNotAvailable = 0x8876086A;
    }

    bool spPCLensFlareManager::InitExistingEmptyMapForAnalysis(
        CapturedStateForAnalysis& state, QueryCapabilityForAnalysis query, void* context) noexcept
    {
        auto& map = state.map;
        if (state.primary != CapturedPrimary || !query || state.borrowedDevice == 0
            || state.outstandingQueries != 0 || map.sentinel == 0 || map.sentinelIsNil != 1
            || map.count != 0 || map.first != map.sentinel || map.root != map.sentinel
            || map.last != map.sentinel)
        {
            return false;
        }

        // 4C67B0 takes the empty map reset route before the provider call.
        map.root = map.sentinel;
        map.count = 0;
        map.first = map.sentinel;
        map.last = map.sentinel;
        state.enabled = 0;

        const auto status = query(context, state.borrowedDevice, OcclusionQuery);
        state.capability = status != QueryNotAvailable;
        state.enabled = 1;
        return true;
    }
}
