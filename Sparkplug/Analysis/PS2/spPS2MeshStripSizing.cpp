#include "spPS2MeshStripSizing.h"
#include "spPS2MeshPacketFormat.h"

namespace sparkplug::evidence::ps2
{
    MeshStripSizingResultForAnalysis ComputeMeshStripReservationForAnalysis(
        const MeshStripSizingInputsForAnalysis& inputs, std::uint32_t maximumRecords) noexcept
    {
        MeshStripSizingResultForAnalysis result;
        if (inputs.grouping != 1)
        {
            // Native160788..79C returns before reading any list nodes.
            result.qwords = inputs.streamCount * inputs.argumentA2 + inputs.declaredStripCount * 2U;
        }
        else
        {
            if (inputs.traversedCounts.size() > maximumRecords) return result;
            result.componentCountersProduced = true;
            for (auto count : inputs.traversedCounts)
            {
                for (unsigned component = 0; component < 22; ++component)
                {
                    if (component >= 3 && component <= 5) continue;
                    if (component && !(inputs.effectiveFlags & (1U << (component - 1)))) continue;
                    const auto bytes = MeshPacketComponentByteWidthForAnalysis(inputs.descriptors[component])
                        * std::uint32_t{count};
                    result.alignedComponentBytes[component] += (bytes + 15U) & ~std::uint32_t{15};
                }
                ++result.recordsVisited;
            }
            std::uint32_t totalBytes = 0;
            for (auto bytes : result.alignedComponentBytes) totalBytes += bytes;
            const auto payloadQwords = (totalBytes >> 4) + ((totalBytes & 15U) != 0 ? 1U : 0U);
            result.qwords = inputs.streamCount * inputs.declaredStripCount
                + payloadQwords + inputs.declaredStripCount;
        }
        result.completed = true;
        result.status = MeshStripSizingStatusForAnalysis::Complete;
        return result;
    }
}
