#include "spPS2MeshPacketEmitter.h"
#include "../../Analysis/PS2/spPS2MeshWeightPacking.h"
#include "../../Analysis/PS2/spPS2MeshPacketFormat.h"

#include <limits>
#include <utility>

namespace sparkplug::reconstruction::ps2packet
{
    namespace
    {
        using Status = spPS2MeshData::PacketBuildStatusForAnalysis;

        std::uint32_t ReadWord(const std::vector<std::byte>& bytes, std::size_t at)
        {
            std::uint32_t result = 0;
            for (unsigned lane = 0; lane < 4; ++lane)
                result |= std::uint32_t{std::to_integer<std::uint8_t>(bytes[at + lane])} << (lane * 8);
            return result;
        }
        struct Writer
        {
            std::vector<std::byte>& bytes;
            std::size_t cursor = 0;
            bool valid = true;
            void Word(std::uint32_t word)
            {
                if (cursor <= bytes.size() && bytes.size() - cursor >= 4)
                    for (unsigned lane = 0; lane < 4; ++lane)
                        bytes[cursor + lane] = static_cast<std::byte>((word >> (lane * 8)) & 255U);
                else valid = false;
                cursor += 4;
            }
            void Qword(std::array<std::uint32_t, 4> words) { for (auto word : words) Word(word); }
            void Align() { cursor = (cursor + 15U) & ~std::size_t{15}; }
        };
        std::uint64_t AlignedQwords(std::uint64_t bytes) { return (bytes + 15U) / 16U; }
    }

    Emission Emit(const spVertexBuffer& source,
        const spPS2MeshData::PreparationPlan& plan,
        const std::array<std::uint32_t, spPS2MeshData::FieldTableCount>& descriptors,
        const std::array<std::int32_t, spPS2MeshData::FieldTableCount>& componentOrder,
        std::uint32_t grouping, const std::vector<Chunk>& chunks,
        bool includeRet, std::uint32_t maximumBytes)
    {
        const auto reject = [](Status status) { return Emission{status, {}}; };
        const auto& bytes = source.GetDataForAnalysis();
        const auto stride = source.GetVertexStrideForAnalysis();
        if (!source.IsInitializedForAnalysis()
            || std::uint64_t{stride} * source.GetVertexCountForAnalysis() > bytes.size())
            return reject(Status::InvalidInput);
        const auto effective = plan.effectiveComponentFlags;
        const bool weighted = plan.field3C != 0;
        if (effective != source.GetComponentFlagsForAnalysis()) return reject(Status::InvalidInput);
        const auto weightBits = (effective >> 1) & 15U;
        unsigned weights = 0;
        for (unsigned lane = 0; lane < 4; ++lane) weights += (weightBits >> lane) & 1U;
        if (weights != plan.field3C || ((effective & 0x3EU)
            && (!weighted || !(effective & 0x20U) || (weightBits & (weightBits + 1U)))))
            return reject(Status::UnsupportedWeights);
        constexpr std::uint32_t supported = 0x7F800U | 0x17EU;
        if (effective & ~supported) return reject(Status::UnsupportedComponents);
        bool missingUv = false;
        for (unsigned uv = 0; uv < 8; ++uv)
        {
            const bool present = (effective & (0x800U << uv)) != 0;
            if (present && missingUv) return reject(Status::UnsupportedComponents);
            if (!present) missingUv = true;
        }
        if (grouping == 1 && (effective & 0x40U)) return reject(Status::UnsupportedCompressedNormal);
        if (plan.field20 < (weighted ? 2U : 0U)) return reject(Status::InvalidInput);
        const auto ordinaryStreams = plan.field20 - (weighted ? 2U : 0U);
        std::array<unsigned, 11> components{};
        std::size_t componentCount = 0;
        for (auto component : componentOrder)
            if (component >= 0 && component < 22 &&
                (component == 0 || (effective & (1U << (component - 1)))))
            {
                if (componentCount == components.size()) return reject(Status::UnsupportedComponents);
                components[componentCount++] = static_cast<unsigned>(component);
            }
        if (componentCount != ordinaryStreams) return reject(Status::UnsupportedComponents);
        const auto chunkQwords = [&](std::uint32_t n) -> std::uint64_t
        {
            if (grouping != 1) return std::uint64_t{n} * plan.field20 + (weighted ? 3U : 2U);
            std::uint64_t size = componentCount + 1 + (weighted ? 1U + std::uint64_t{n} * 2U : 0U);
            for (std::size_t stream = 0; stream < componentCount; ++stream)
            {
                const auto component = components[stream];
                size += AlignedQwords(std::uint64_t{n}
                    * sparkplug::evidence::ps2::MeshPacketComponentByteWidthForAnalysis(descriptors[component]));
            }
            return size;
        };
        const std::uint64_t maximumQwords = maximumBytes / 16U;
        std::uint64_t qwords = includeRet ? 1U : 0U;
        if (qwords > maximumQwords) return reject(Status::ByteBudgetExceeded);
        for (const auto& chunk : chunks)
        {
            if (chunk.indices.empty() || chunk.indices.size() > std::numeric_limits<std::uint32_t>::max()
                || chunk.adc.size() != chunk.indices.size()) return reject(Status::InvalidInput);
            for (auto index : chunk.indices)
                if (index >= source.GetVertexCountForAnalysis()) return reject(Status::InvalidInput);
            const auto size = chunkQwords(static_cast<std::uint32_t>(chunk.indices.size()));
            if (size > maximumQwords - qwords) return reject(Status::ByteBudgetExceeded);
            qwords += size;
        }
        try
        {
            std::vector<std::byte> packet(static_cast<std::size_t>(qwords * 16U), std::byte{0});
            Writer writer{packet};
            if (includeRet)
                writer.Qword({0x60000000U | (static_cast<std::uint32_t>(qwords - 1) & 0xFFFFU),
                    0, 0x11000000U, 0x11000000U});
            const auto& offsets = source.GetComponentOffsetsForAnalysis();
            const auto word = [&](std::uint32_t vertex, unsigned component, unsigned lane)
            {
                return ReadWord(bytes, std::size_t{vertex} * stride + std::size_t{offsets[component]} * 4 + lane * 4);
            };
            for (const auto& chunk : chunks)
            {
                const auto n = static_cast<std::uint32_t>(chunk.indices.size());
                const auto weightPayload = [&](std::uint32_t addressStreams)
                {
                    writer.Qword({0, 0, 0x01010101U, 0x6C008000U | (n << 17) | (addressStreams * n)});
                    for (auto vertex : chunk.indices)
                    {
                        std::array<std::uint32_t, 4> raw{};
                        for (unsigned lane = 0; lane < plan.field3C; ++lane) raw[lane] = word(vertex, 2 + lane, 0);
                        const auto record = sparkplug::evidence::ps2::PackMeshWeightsForAnalysis(
                            raw, word(vertex, 6, 0), plan.field3C);
                        if (!record) { writer.valid = false; return; }
                        for (auto value : *record) writer.Word(value);
                    }
                };
                if (grouping == 1)
                {
                    unsigned ordinal = 0;
                    for (std::size_t stream = 0; stream < componentCount; ++stream)
                    {
                        const auto component = components[stream];
                        const auto descriptor = descriptors[component];
                        const auto usn = (descriptor >> 16) == 0 ? 0x4000U : 0U;
                        writer.Qword({0, 0, 0x01000000U | (n << 16) | 0x100U | ordinaryStreams,
                            ((descriptor & 0xFFFFU) << 24) | (n << 16) | 0x8000U | usn | ordinal});
                        for (std::uint32_t at = 0; at < n; ++at)
                        {
                            const auto vertex = chunk.indices[at];
                            if (component == 9)
                            {
                                const auto color = word(vertex, 9, 0);
                                writer.Word((color & 0xFF00FF00U) | ((color & 255U) << 16) | ((color >> 16) & 255U));
                            }
                            else
                            {
                                writer.Word(word(vertex, component, 0)); writer.Word(word(vertex, component, 1));
                                if (component == 0)
                                {
                                    writer.Word(word(vertex, 0, 2));
                                    writer.Word(chunk.adc[at] ? 0x8000U : 0U);
                                }
                            }
                        }
                        writer.Align();
                        ++ordinal;
                    }
                    if (weighted) weightPayload(ordinaryStreams);
                    writer.Qword({0, 0x04000000U | n, 0x01010101U, 0x17000000U});
                }
                else
                {
                    writer.Qword({0, 0, 0, 0x6C008000U | ((plan.field20 * n) << 16)});
                    for (std::uint32_t at = 0; at < n; ++at)
                    {
                        const auto vertex = chunk.indices[at];
                        for (unsigned uv = 0; uv < 8 && (effective & (0x800U << uv)); ++uv)
                            writer.Qword({word(vertex, 12 + uv, 0), word(vertex, 12 + uv, 1), 0x3F800000U, 0});
                        if (effective & 0x100U)
                        {
                            const auto color = word(vertex, 9, 0);
                            writer.Qword({(color >> 16) & 255U, (color >> 8) & 255U, color & 255U, color >> 24});
                        }
                        if (effective & 0x40U) writer.Qword({word(vertex, 7, 0), word(vertex, 7, 1), word(vertex, 7, 2), 0});
                        writer.Qword({word(vertex, 0, 0), word(vertex, 0, 1), word(vertex, 0, 2), chunk.adc[at] ? 0x8000U : 0U});
                    }
                    if (weighted) weightPayload(plan.field20);
                    writer.Qword({0, 0, 0x04000000U | n, 0x17000000U});
                }
            }
            if (!writer.valid || writer.cursor != packet.size()) return reject(Status::InternalSizeMismatch);
            return {Status::Built, std::move(packet)};
        }
        catch (...) { return reject(Status::AllocationFailed); }
    }
}
