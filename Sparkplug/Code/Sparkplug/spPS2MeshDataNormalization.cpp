#include "spPS2MeshData.h"

#include <algorithm>
#include <limits>

namespace sparkplug::reconstruction
{
    spPS2MeshData::VertexNormalizationForAnalysis
        spPS2MeshData::NormalizeVertexBufferForAnalysis(const spVertexBuffer& source,
            const std::vector<std::byte>* capturedAllocation, std::uint32_t maximumBytes)
    {
        VertexNormalizationForAnalysis result;
        if (!source.IsInitializedForAnalysis()) return result;
        const auto flags = source.GetComponentFlagsForAnalysis();
        const bool uv = (flags & 0x800U) != 0;
        const bool normal = (flags & 0x40U) != 0;
        const bool color = (flags & 0x100U) != 0;
        if (uv && (normal || color))
        {
            result.completed = result.borrowed = true;
            result.buffer = &source;
            return result;
        }
        const auto effective = flags | 0x800U | ((normal || color) ? 0U : 0x100U);
        spVertexBuffer layout;
        if (!layout.InitializeForAnalysis(effective, 0, source.GetFlagsForAnalysis())) return result;
        const auto size = std::uint64_t{layout.GetVertexStrideForAnalysis()} * source.GetVertexCountForAnalysis();
        if (size > maximumBytes || size > std::numeric_limits<std::uint32_t>::max()
            || (capturedAllocation && capturedAllocation->size() != size)) return result;
        try
        {
            result.expanded = std::make_unique<spVertexBuffer>();
            if (!result.expanded->InitializeForAnalysis(effective, source.GetVertexCountForAnalysis(),
                    source.GetFlagsForAnalysis())) return {};
            auto bytes = capturedAllocation ? *capturedAllocation : result.expanded->GetDataForAnalysis();
            result.writtenBytes.resize(bytes.size(), 0);
            const auto& input = source.GetDataForAnalysis();
            const auto& sourceOffsets = source.GetComponentOffsetsForAnalysis();
            const auto& outputOffsets = result.expanded->GetComponentOffsetsForAnalysis();
            const auto copy = [&](std::uint32_t vertex, unsigned component, std::size_t count, bool zero)
            {
                const std::uint64_t from = std::uint64_t{vertex} * source.GetVertexStrideForAnalysis()
                    + std::uint64_t{sourceOffsets[component]} * 4;
                const std::uint64_t to = std::uint64_t{vertex} * layout.GetVertexStrideForAnalysis()
                    + std::uint64_t{outputOffsets[component]} * 4;
                if (to > bytes.size() || count > bytes.size() - to
                    || (!zero && (from > input.size() || count > input.size() - from))) return false;
                const auto outputAt = static_cast<std::size_t>(to);
                if (zero) std::fill_n(bytes.begin() + outputAt, count, std::byte{0});
                else std::copy_n(input.begin() + static_cast<std::size_t>(from), count, bytes.begin() + outputAt);
                std::fill_n(result.writtenBytes.begin() + outputAt, count, std::uint8_t{1});
                return true;
            };
            for (std::uint32_t vertex = 0; vertex < source.GetVertexCountForAnalysis(); ++vertex)
            {
                if (!copy(vertex, 0, 12, false)
                    || !copy(vertex, 9, 4, !color)
                    || (normal && !copy(vertex, 7, 12, false))) return {};
                // Do not fix the absent-color offset0 write above: it can
                // overwrite position.x even when only Normal supplies the test.
                if (uv)
                {
                    for (unsigned component = 12; component < 20; ++component)
                        if ((flags & (1U << (component - 1))) && !copy(vertex, component, 8, false)) return {};
                }
                else if (!copy(vertex, 12, 8, true)) return {};
            }
            if (!result.expanded->SetDataForAnalysis(bytes)) return {};
            result.capturedAllocationUsed = capturedAllocation != nullptr;
            result.buffer = result.expanded.get();
            result.completed = true;
            return result;
        }
        catch (...) { return {}; }
    }
}
