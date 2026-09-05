#pragma once

// The original implementation shares the exact PC translation-unit path
// Z:\Sparkplug\Code\SparkplugDX\spDXSharedMeshData.cpp with the target class.
// The serializer and all related strings are absent from the PS2 executable.

#include "../Sparkplug/spSerializer.h"

#include <cstddef>
#include <cstdint>

namespace sparkplug::reconstruction
{
    class spDXCombinedVB;
    class spDXSharedMeshData;
    class spStream;

    class spDXSharedMeshDataSerializer final : public spSerializer
    {
    public:
        static constexpr spClassID ClassID = 0x506F8A8C;
        static constexpr spClassID TargetClassID = 0x293A2681;
        static constexpr spClassID SourceClassID = 0x4B18E622;

        spDXSharedMeshDataSerializer() noexcept = default;
        ~spDXSharedMeshDataSerializer() override;

        spDXSharedMeshDataSerializer(const spDXSharedMeshDataSerializer&) = delete;
        spDXSharedMeshDataSerializer& operator=(
            const spDXSharedMeshDataSerializer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // PC 0x004C1DE0 ignores the incoming serialized class ID and maps the
        // runtime spDXCombinedVB source object to spDXSharedMeshData on load.
        [[nodiscard]] spClassID ResolveClassIDForAnalysis(
            spClassID serializedClassID) const noexcept override;

        [[nodiscard]] spClassID GetTargetClassIDForAnalysis() const noexcept;
        [[nodiscard]] spClassID GetSourceClassIDForAnalysis() const noexcept;

        // Exact wire order: index byte count, vertex byte count, all index
        // bytes, then all vertex bytes. The native reader takes raw pointers
        // into a contiguous stream and ignores target Init failure. This safe
        // counterpart performs bounded sequential reads and propagates it.
        [[nodiscard]] bool WritePayloadForAnalysis(
            spStream& destination,
            const spDXCombinedVB& payload) const;
        [[nodiscard]] bool ReadPayloadForAnalysis(
            spStream& source,
            spDXSharedMeshData& target) const;
    };
}
