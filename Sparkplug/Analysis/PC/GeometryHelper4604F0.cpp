#include "GeometryHelper4604F0.h"

#include <cstring>
#include <limits>

namespace sparkplug::evidence::pc
{
namespace
{
using Helper = GeometryHelper4604F0;
using reconstruction::spIndexBuffer;
using reconstruction::spVertexBuffer;

bool Fail(std::string* error, const char* message)
{
    if (error) *error = message;
    return false;
}

bool SupportedVertices(const spVertexBuffer& vertices) noexcept
{
    return vertices.IsInitializedForAnalysis()
        && vertices.GetComponentFlagsForAnalysis() == 0
        && vertices.GetVertexCountForAnalysis() <= Helper::MaximumVerticesForAnalysis
        && vertices.GetVertexStrideForAnalysis() == 12
        && vertices.GetComponentCountForAnalysis() == 3
        && vertices.GetComponentOffsetsForAnalysis()[0] == 0
        && vertices.GetDataForAnalysis().size()
            == std::size_t(vertices.GetVertexCountForAnalysis()) * 12;
}

bool Validate(const spIndexBuffer& indices, const spVertexBuffer& vertices,
    std::string* error)
{
    // Empty standalone calls have no actual original lifecycle proof in this
    // slice. Do not infer their contract from qsort(count == 0) or allocator(0).
    if (vertices.GetVertexCountForAnalysis() == 0 || indices.GetIndexCountForAnalysis() == 0)
        return Fail(error, "Empty geometry helper inputs remain unsupported by the original evidence");
    if (!SupportedVertices(vertices))
        return Fail(error, "Geometry helper supports only bounded initialized position-only CPU vertices");
    if (!indices.IsInitializedForAnalysis() || indices.GetIndexElementSizeForAnalysis() != 2
        || indices.GetIndexCountForAnalysis() > Helper::MaximumIndicesForAnalysis)
        return Fail(error, "Geometry helper supports only bounded initialized UInt16 CPU indices");
    for (std::uint32_t i = 0; i < indices.GetIndexCountForAnalysis(); ++i)
    {
        const auto index = indices.GetIndexForAnalysis(i);
        if (!index || *index >= vertices.GetVertexCountForAnalysis())
            return Fail(error, "Geometry helper index is outside the input vertex buffer");
    }
    return true;
}

struct CompareContext
{
    const spVertexBuffer& vertices;
    mutable bool valid = true;
};

int Compare(const void* opaque, const std::uint16_t* left, const std::uint16_t* right)
{
    const auto& context = *static_cast<const CompareContext*>(opaque);
    if (!left || !right) { context.valid = false; return 0; }
    const auto result = Helper::ComparePositionsForAnalysis(context.vertices, *left, *right);
    if (!result) { context.valid = false; return 0; }
    return result.value_or(0);
}
}

std::optional<int> GeometryHelper4604F0::ComparePositionsForAnalysis(
    const reconstruction::spVertexBuffer& vertices,
    const std::uint16_t left, const std::uint16_t right) noexcept
{
    if (!SupportedVertices(vertices) || left >= vertices.GetVertexCountForAnalysis()
        || right >= vertices.GetVertexCountForAnalysis()) return std::nullopt;
    const auto& bytes = vertices.GetDataForAnalysis();
    for (std::size_t i = 0; i < 12; ++i)
    {
        const auto a = std::to_integer<unsigned char>(bytes[std::size_t(left) * 12 + i]);
        const auto b = std::to_integer<unsigned char>(bytes[std::size_t(right) * 12 + i]);
        if (a != b) return a < b ? -1 : 1;
    }
    return 0;
}

bool GeometryHelper4604F0::WeldForAnalysis(reconstruction::spIndexBuffer& indices,
    reconstruction::spVertexBuffer& vertices, const SortDispatchForAnalysis& sort,
    ObservationForAnalysis& observation, std::string* error) const
{
    observation = {};
    if (error) error->clear();
    if (!Validate(indices, vertices, error)) return false;
    if (!sort.invoke) return Fail(error, "Original qsort dependency must be supplied explicitly");
    try
    {
        const auto count = vertices.GetVertexCountForAnalysis();
        std::vector<std::uint16_t> sorted(count), inverse(count);
        for (std::uint32_t i = 0; i < count; ++i) sorted[i] = static_cast<std::uint16_t>(i);
        CompareContext context{vertices};
        if (!sort.invoke(sort.context, sorted.data(), count, 2, &Compare, &context)
            || !context.valid)
            return Fail(error, "Explicit sort dependency failed before geometry mutation");

        // Host callback validation does not prescribe the order of equal keys.
        std::vector<bool> seen(count, false);
        for (std::uint32_t i = 0; i < count; ++i)
        {
            if (sorted[i] >= count || seen[sorted[i]])
                return Fail(error, "Sort dependency did not return a permutation of vertex IDs");
            seen[sorted[i]] = true;
            if (i && *ComparePositionsForAnalysis(vertices, sorted[i - 1], sorted[i]) > 0)
                return Fail(error, "Sort dependency returned IDs outside original comparator order");
        }
        observation.sortedIds = sorted;
        // 460CC0: inverse must be built BEFORE duplicate representatives fold.
        for (std::uint32_t i = 0; i < count; ++i)
            inverse[sorted[i]] = static_cast<std::uint16_t>(i);
        for (std::uint32_t i = 1; i < count; ++i)
        {
            if (*ComparePositionsForAnalysis(vertices, sorted[i], sorted[i - 1]) == 0)
            {
                sorted[i] = sorted[i - 1];
                observation.duplicateFound = true;
            }
        }
        if (!observation.duplicateFound) return true;
        // 460D40: remap every IB element before invoking the compactor.
        for (std::uint32_t i = 0; i < indices.GetIndexCountForAnalysis(); ++i)
            if (!indices.SetIndexForAnalysis(i, sorted[inverse[*indices.GetIndexForAnalysis(i)]]))
                return Fail(error, "Host index write failed after duplicate remapping began");
        return CompactForAnalysis(indices, vertices, observation, error);
    }
    catch (...)
    {
        return Fail(error, "Geometry helper host allocation or callback failed; partial mutations are retained");
    }
}

bool GeometryHelper4604F0::CompactForAnalysis(reconstruction::spIndexBuffer& indices,
    reconstruction::spVertexBuffer& vertices, ObservationForAnalysis& observation,
    std::string* error) const
{
    if (error) error->clear();
    if (!Validate(indices, vertices, error)) return false;
    observation.compactionCalled = true;
    observation.compactionAllocationBytes = 0;
    try
    {
        const auto count = vertices.GetVertexCountForAnalysis();
        const auto absent = std::numeric_limits<std::uint32_t>::max();
        std::vector<std::uint32_t> map(count, absent);
        for (std::uint32_t i = 0; i < indices.GetIndexCountForAnalysis(); ++i)
            map[*indices.GetIndexForAnalysis(i)] = 0;
        std::uint32_t retained = 0;
        // 450FF5: compact in ORIGINAL vertex order, not sort or first-use order.
        for (auto& index : map) if (index != absent) index = retained++;
        for (std::uint32_t i = 0; i < indices.GetIndexCountForAnalysis(); ++i)
            if (!indices.SetIndexForAnalysis(i, static_cast<std::uint16_t>(map[*indices.GetIndexForAnalysis(i)])))
                return Fail(error, "Host index write failed after compaction began");
        if (retained == count) return true;

        // Actual 451040..451047 allocates FOUR times the logical extent.
        // Preserve that request in temporary host storage and the observation.
        // The portable buffer owns logical bytes; unused original heap tail
        // bytes are not invented as initialized/serialized vertex data.
        const std::size_t logicalBytes = std::size_t(retained) * 12;
        observation.compactionAllocationBytes = static_cast<std::uint32_t>(logicalBytes * 4);
        std::vector<std::byte> allocation(observation.compactionAllocationBytes);
        const auto& source = vertices.GetDataForAnalysis();
        for (std::uint32_t i = 0; i < count; ++i)
            if (map[i] != absent)
                std::memcpy(allocation.data() + std::size_t(map[i]) * 12,
                    source.data() + std::size_t(i) * 12, 12);
        allocation.resize(logicalBytes);
        // 4510E6 -> 4600E0 transfers the new records after IB mutation. The
        // existing portable buffer initializer supplies modern CPU ownership.
        if (!vertices.InitializeFromDataForAnalysis(0, retained,
                vertices.GetFlagsForAnalysis(), allocation))
            return Fail(error, "Host vertex replacement failed after original-order index compaction");
        return true;
    }
    catch (...)
    {
        return Fail(error, "Geometry compaction allocation failed; prior index mutations are retained");
    }
}
}
