#include "spDXBallisticPFX.h"

#include <array>

namespace sparkplug::reconstruction
{
    namespace
    {
        struct PointState
        {
            std::uint32_t index;
            std::uint32_t value;
        };

        constexpr std::array<PointState, 4> PointStates{{
            {156, 1}, {157, 1}, {155, 0x3F800000}, {154, 0x3F800000},
        }};
    }

    bool spDXBallisticPFX::ApplyPointStatesForAnalysis(std::uint32_t primary,
        ReadRendererForAnalysis readRenderer, void* context) noexcept
    {
        if (primary != CapturedPrimary || !readRenderer)
            return false;

        for (const auto state : PointStates)
        {
            const auto binding = readRenderer(context);
            if (!binding.entries || state.index >= binding.entryCount)
                return false;
            if (!spDXRenderer::ApplyRenderStateCacheEntryForAnalysis(
                    binding.entries[state.index], state.index, state.value, binding.submit, binding.submitContext))
                return false;
        }
        return true;
    }
}
