#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>
#include <vector>

namespace sparkplug::reconstruction
{
    inline constexpr std::uint32_t spPCDXExpandedPackedFieldBytes = 12;
    inline constexpr std::size_t spPCDXPackedFieldComponentOffsetIndex = 6;

    // Shared recovered PC 0x4AA000 vertex-copy arithmetic, extracted from
    // spDXMesh::BuildVertexBytesForAnalysis. Weights, COLOR0, normals and UVs
    // retain their authored bits. Only component bit0x20 expands four uint8
    // palette indices to four unnormalized float32 values in-place in layout.
    // Pointer/size/overflow checks are host bounds, not extra native behavior.
    // Source and destination storage must be distinct and stable during this call.
    template<class Byte>
    [[nodiscard]] bool BuildPCDXVertexBytesForAnalysis(
        const void* const sourceBytes,
        const std::size_t availableBytes,
        const std::uint32_t sourceStride,
        const std::uint32_t vertexCount,
        const std::uint32_t componentFlags,
        const std::uint32_t packedOffsetWords,
        std::vector<Byte>& destination,
        std::uint32_t& destinationStride)
    {
        static_assert(std::is_same_v<Byte, std::byte>
            || std::is_same_v<Byte, std::uint8_t>);
        const std::uint64_t sourceSize =
            static_cast<std::uint64_t>(sourceStride) * vertexCount;
        if (sourceSize > availableBytes || (sourceSize != 0 && sourceBytes == nullptr))
        {
            return false;
        }

        const bool expandsPackedField = (componentFlags & 0x20U) != 0;
        if (expandsPackedField && sourceStride
            > (std::numeric_limits<std::uint32_t>::max)() - spPCDXExpandedPackedFieldBytes)
        {
            return false;
        }
        destinationStride = sourceStride
            + (expandsPackedField ? spPCDXExpandedPackedFieldBytes : 0U);
        const std::uint64_t destinationSize =
            static_cast<std::uint64_t>(destinationStride) * vertexCount;
        if (destinationSize > (std::numeric_limits<std::uint32_t>::max)())
        {
            return false;
        }

        try
        {
            destination.resize(static_cast<std::size_t>(destinationSize));
        }
        catch (...)
        {
            return false;
        }

        const auto* const input = static_cast<const std::byte*>(sourceBytes);
        if (!expandsPackedField)
        {
            if (sourceSize != 0)
            {
                std::memcpy(destination.data(), input, static_cast<std::size_t>(sourceSize));
            }
            return true;
        }

        const std::uint64_t packedOffset64 =
            static_cast<std::uint64_t>(packedOffsetWords) * sizeof(std::uint32_t);
        if (packedOffset64 > sourceStride || sourceStride - packedOffset64 < 4)
        {
            return false;
        }
        const auto packedOffset = static_cast<std::size_t>(packedOffset64);
        const auto tailSize = sourceStride - packedOffset - 4U;
        for (std::uint32_t vertex = 0; vertex < vertexCount; ++vertex)
        {
            const auto* const inputVertex = input
                + static_cast<std::size_t>(vertex) * sourceStride;
            auto* const outputVertex = destination.data()
                + static_cast<std::size_t>(vertex) * destinationStride;
            std::memcpy(outputVertex, inputVertex, packedOffset);
            for (std::size_t component = 0; component < 4; ++component)
            {
                const float expanded = static_cast<float>(
                    std::to_integer<std::uint8_t>(inputVertex[packedOffset + component]));
                std::memcpy(outputVertex + packedOffset + component * sizeof(float),
                    &expanded, sizeof(expanded));
            }
            std::memcpy(outputVertex + packedOffset + 4U * sizeof(float),
                inputVertex + packedOffset + 4U, tailSize);
        }
        return true;
    }
}
