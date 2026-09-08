#pragma once

// Exact original PC translation-unit path recovered from diagnostics:
// Z:\Sparkplug\Code\Sparkplug\spMeshDataSerializer.cpp
// The PS2 executable independently preserves "spMeshDataSerializer.cpp".

#include "spSerializer.h"

#include <cstdint>
#include <array>
#include <functional>
#include <vector>

namespace sparkplug::reconstruction
{
    class spIndexBuffer;
    class spVertexBuffer;
    class spMeshDataSerializer : public spSerializer
    {
    public:
        static constexpr spClassID ClassID = 0x66380037;
        static constexpr spClassID TargetClassID = 0x33C34CF0;

        enum class Field : std::uint32_t
        {
            CrossPlatform = 0,
        };

        spMeshDataSerializer() noexcept;
        ~spMeshDataSerializer() override;

        spMeshDataSerializer(const spMeshDataSerializer&) = delete;
        spMeshDataSerializer& operator=(const spMeshDataSerializer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        [[nodiscard]] virtual spClassID GetTargetClassIDForAnalysis() const noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> ReadObjectHeaderAndCreateForAnalysis(
            spStream& source, spSerializerObjectHeaderForAnalysis* observedHeader = nullptr) const override;
        [[nodiscard]] bool ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
            spStream& source, std::uint32_t byteCount, spBaseObject& object, std::string* error) const override;
        [[nodiscard]] bool WritePayloadForAnalysis(spStream&, const spBaseObject&, std::string*) const override;
        [[nodiscard]] bool IndexRelationshipsForAnalysis(spBaseObject&) const override;
        [[nodiscard]] bool WritePayloadWithContextForAnalysis(spSerializerManager&, spStream&,
            const spBaseObject&, std::string*) const override;
        static constexpr std::uint32_t MaximumPayloadBytesForAnalysis = 32u*1024u*1024u;

        // The original selector type/name is not yet known. PS2 proves that
        // native values 0 and 2 emit field 0; all other values omit it.
        [[nodiscard]] static bool EmitsCrossPlatformPayloadForAnalysis(
            std::uint32_t nativeSerializationMode) noexcept;
        [[nodiscard]] std::vector<Field> BuildKnownWritePlanForAnalysis(
            std::uint32_t nativeSerializationMode) const;
        // Host observations of the actual reader, not original object members.
        struct BufferReadObservationForAnalysis {
            std::uint32_t fieldID=0,fieldPayloadOffset=0,indexPayloadOffset=0,vertexPayloadOffset=0;
            std::array<std::uint32_t,4> planningWords{};
            std::uint8_t planningByte=0;
        };
        using BufferReadObserverForAnalysis=std::function<void(const spIndexBuffer&,
            const spVertexBuffer&,const BufferReadObservationForAnalysis&)>;
        // PC429A40/portable buffer branch. Also used by direct field inspectors.
        [[nodiscard]] static bool ReadBuffersForAnalysis(spStream&,std::uint32_t,bool native,
            spIndexBuffer&,spVertexBuffer&,std::string*,BufferReadObservationForAnalysis* = nullptr);
        [[nodiscard]] bool ReadMeshFieldsForAnalysis(spSerializerReadContextForAnalysis& context,
            spStream& source, std::uint32_t byteCount, spBaseObject& object,
            bool dxFields, std::string* error,const BufferReadObserverForAnalysis& observer={}) const;
    };
}
