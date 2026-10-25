#include "spDXRenderer.h"
#include "../Sparkplug/spPalette.h"
#include <new>

namespace sparkplug::reconstruction
{
    spDXRenderer::LifecycleResultForAnalysis spDXRenderer::RegisterPaletteForAnalysis(
        DevicePalettePoolForAnalysis& pool, const std::uintptr_t device,
        spPalette& palette, const DevicePaletteEntriesForAnalysis submit, void* const context)
    {
        if (device == 0 || submit == nullptr || !palette.HasInitializedEntriesForAnalysis()
            || pool.freeIndices.size() > pool.maximumFreeIndices) return {};
        std::uint32_t index;
        if (pool.freeIndices.empty())
        {
            index = pool.nextIndex;
            ++pool.nextIndex; // Native unsigned32 wrap, including FFFFFFFF.
        }
        else
        {
            index = pool.freeIndices.front();
            // Protected4CDED0 resolves458C20 and removes every matching value.
            // Copy the first value before removal, preserving duplicate behavior.
            pool.freeIndices.remove(index);
        }
        (void)submit(context, device, index, palette.GetEntriesForAnalysis().data());
        palette.SetDeviceIndexForAnalysis(index); // Even failed HRESULT.
        return {true, true};
    }

    spDXRenderer::LifecycleResultForAnalysis spDXRenderer::RecyclePaletteIndexForAnalysis(
        DevicePalettePoolForAnalysis& pool, const spPalette& palette)
    {
        const auto index = palette.GetIndexForAnalysis();
        if (index == 0xFFFFFFFFu) return {true, true};
        if (pool.freeIndices.size() >= pool.maximumFreeIndices) return {};
        try { pool.freeIndices.push_back(index); }
        catch (const std::bad_alloc&) { return {}; }
        // Native4BB840 deliberately does not reset palette.index toFFFFFFFF.
        return {true, true};
    }
}
