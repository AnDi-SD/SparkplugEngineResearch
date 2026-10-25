#include "spPS2MeshStripPreparation.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace sparkplug::evidence::ps2
{
    MeshStripPreparationResultForAnalysis PreparePrepopulatedMeshStripsForAnalysis(
        const std::vector<std::vector<std::uint16_t>>& strips,
        std::uint32_t capacity, std::uint32_t maximumBytes)
    {
        using Status = MeshStripPreparationStatusForAnalysis;
        const auto reject = [](Status status) { return MeshStripPreparationResultForAnalysis{false, status, {}, {}}; };
        if (capacity < 3 || capacity > 65535) return reject(Status::InvalidCapacity);
        if (strips.size() > std::numeric_limits<std::uint32_t>::max()) return reject(Status::InvalidStripCount);
        std::uint64_t bytes = 0;
        for (const auto& strip : strips)
        {
            if (strip.size() < 3 || strip.size() > 65535) return reject(Status::InvalidStripCount);
            if (strip.size() > capacity) return reject(Status::StripExceedsCapacity);
            bytes += std::uint64_t{strip.size()} * 3U;
            if (bytes > maximumBytes) return reject(Status::ByteBudgetExceeded);
        }
        try
        {
            MeshStripPreparationResultForAnalysis result;
            std::vector<PreparedMeshStripForAnalysis> even, odd;
            for (std::size_t at = 0; at < strips.size(); ++at)
            {
                PreparedMeshStripForAnalysis copy{static_cast<std::uint32_t>(at), strips[at],
                    std::vector<std::uint8_t>(strips[at].size(), 0)};
                (copy.indices.size() & 1U ? odd : even).push_back(std::move(copy));
            }
            const auto combine = [&](std::vector<PreparedMeshStripForAnalysis>& group)
            {
                // Native15FD30 has separate processed/absorbed byte arrays.
                // For its same-list calls, both forbid the same candidates;
                // one explicit flag is sufficient for the captured values.
                std::vector<bool> unavailable(group.size(), false);
                for (std::size_t anchor = 0; anchor < group.size(); ++anchor)
                {
                    if (unavailable[anchor]) continue;
                    auto& current = group[anchor];
                    for (std::size_t next = 0; next < group.size()
                        && capacity - current.indices.size() >= 3; ++next)
                    {
                        if (next == anchor || unavailable[next]) continue;
                        const auto& candidate = group[next];
                        if (candidate.indices.size() > capacity - current.indices.size()) continue;
                        const auto first = static_cast<std::uint32_t>(current.indices.size());
                        const bool reverse = (first & 1U) != 0;
                        if (reverse) current.indices.insert(current.indices.end(), candidate.indices.rbegin(), candidate.indices.rend());
                        else current.indices.insert(current.indices.end(), candidate.indices.begin(), candidate.indices.end());
                        current.adc.resize(current.indices.size(), 0);
                        current.adc[first] = current.adc[first + 1] = 1;
                        result.joins.push_back({current.sourceOrdinal, candidate.sourceOrdinal, first, reverse});
                        unavailable[next] = true;
                    }
                    unavailable[anchor] = true;
                    result.strips.push_back(std::move(current));
                }
            };
            combine(even);
            combine(odd);
            // Native comparator15D560 is unsigned(right.count)<left.count.
            // Its merge keeps left on equal comparison, preserving ties.
            std::stable_sort(result.strips.begin(), result.strips.end(),
                [](const auto& first, const auto& second) { return first.indices.size() > second.indices.size(); });
            result.completed = true;
            result.status = Status::Prepared;
            return result;
        }
        catch (...) { return reject(Status::AllocationFailed); }
    }
}
