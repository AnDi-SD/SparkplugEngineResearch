#pragma once

// Exact original PC translation-unit path recovered from diagnostics:
// Z:\Sparkplug\Code\Sparkplug\spDXMeshDataSerializer.cpp
// The PS2 executable independently preserves "spDXMeshDataSerializer.cpp".

#include "spMeshDataSerializer.h"

#include <cstdint>
#include <vector>

namespace sparkplug::reconstruction
{
    class spMeshData;

    class spDXMeshDataSerializer final : public spMeshDataSerializer
    {
    public:
        static constexpr spClassID ClassID = 0x77006ABE;
        static constexpr spClassID TargetClassID = 0x3178114C;

        enum class Field : std::uint32_t
        {
            CrossPlatform = 0,
            PlatformSpecific = 1,
        };

        static constexpr std::uint32_t PCNativeLoadFlagMask = 0x00000002;
        static constexpr std::uint32_t PS2NativeLoadFlagMask = 0x00000008;
        static constexpr std::uint32_t ExpandedField30BytesPerVertex = 12;

        struct NativePayloadHeader final
        {
            bool valid = false;
            std::uint32_t fvfCode = 0;
            std::uint32_t vertexCount = 0;
            std::uint32_t vertexDataSize = 0;
            std::uint32_t indexDataSize = 0;
            bool indicesAre32Bit = false;
        };

        spDXMeshDataSerializer() noexcept = default;
        ~spDXMeshDataSerializer() override;

        spDXMeshDataSerializer(const spDXMeshDataSerializer&) = delete;
        spDXMeshDataSerializer& operator=(const spDXMeshDataSerializer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] spClassID GetTargetClassIDForAnalysis() const noexcept override;
        [[nodiscard]] bool ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
            spStream& source, std::uint32_t byteCount, spBaseObject& object, std::string* error) const override;
        [[nodiscard]] bool WritePayloadForAnalysis(spStream&, const spBaseObject&,
            std::string*) const override;
        [[nodiscard]] bool IndexRelationshipsForAnalysis(spBaseObject&) const override;
        [[nodiscard]] bool WritePayloadWithContextForAnalysis(spSerializerManager&, spStream&,
            const spBaseObject&, std::string*) const override;

        [[nodiscard]] std::vector<Field> BuildKnownWritePlanForAnalysis(
            std::uint32_t nativeSerializationMode) const;
        [[nodiscard]] static bool PCLoadsNativePayloadForAnalysis(
            std::uint32_t readerFlags) noexcept;
        [[nodiscard]] static bool PS2LoadsNativePayloadForAnalysis(
            std::uint32_t readerFlags) noexcept;
        [[nodiscard]] static NativePayloadHeader
            BuildNativePayloadHeaderForAnalysis(const spMeshData& meshData) noexcept;
    };
}
