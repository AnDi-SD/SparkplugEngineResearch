#pragma once

// Exact original PC translation-unit path recovered from diagnostics:
// Z:\Sparkplug\Code\Sparkplug\spTextureDataSerializer.cpp
// The PS2 executable independently preserves "spTextureDataSerializer.cpp".

#include "spSerializer.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace sparkplug::reconstruction
{
    class spTextureData;
    class spTextureBuffer;
    class spDXTexture;

    class spTextureDataSerializer : public spSerializer
    {
    public:
        static constexpr spClassID ClassID = 0x1C4C75BA;
        static constexpr spClassID TargetClassID = 0x78EA082B;

        // The original spelling "Embeded" (one d) survives in diagnostics.
        enum class Field : std::uint32_t
        {
            CrossPlatform = 0,
            PlatformSpecific = 1,
            SourceNone = 2,
            SourceEmbeded = 3,
            SourceReference = 4,
            PlatformType = 6,
        };

        enum class DataSourceKind : std::uint8_t
        {
            None,
            EmbeddedMemoryStream,
            ReferencedStream,
        };

        static constexpr std::uint32_t CrossPlatformType = 1;

        struct CrossPlatformPayloadHeader final
        {
            bool valid = false;
            std::uint32_t width = 0;
            std::uint32_t height = 0;
            std::uint32_t pixelFormat = 0;
            std::uint32_t pixelSize = 0;
            std::uint32_t payloadSize = 0;
        };

        spTextureDataSerializer() noexcept = default;
        ~spTextureDataSerializer() override;

        spTextureDataSerializer(const spTextureDataSerializer&) = delete;
        spTextureDataSerializer& operator=(const spTextureDataSerializer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        [[nodiscard]] virtual spClassID GetTargetClassIDForAnalysis() const noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> ReadObjectHeaderAndCreateForAnalysis(
            spStream&,spSerializerObjectHeaderForAnalysis* observedHeader=nullptr) const override;
        [[nodiscard]] bool ReadPayloadForAnalysis(spSerializerReadContextForAnalysis&,spStream&,
            std::uint32_t,spBaseObject&,std::string*) const override;
        [[nodiscard]] bool WritePayloadForAnalysis(spStream&,const spBaseObject&,std::string*) const override;
        [[nodiscard]] bool WritePayloadWithContextForAnalysis(spSerializerManager&,spStream&,
            const spBaseObject&,std::string*) const override;
        [[nodiscard]] bool IndexRelationshipsWithContextForAnalysis(spSerializerManager&,spBaseObject&) const override;

        // Models the confirmed source-selection wrapper and, for SourceNone,
        // the base serializer's optional cross-platform body. Modes 0 and 2
        // emit that body; the other native modes close after SourceNone.
        [[nodiscard]] std::vector<Field> BuildKnownWritePlanForAnalysis(
            DataSourceKind sourceKind,
            std::uint32_t nativeSerializationMode) const;

        // Safe description of field 0 / field 5. The native writer multiplies
        // width*height*pixelSize and deliberately does not use buffer depth.
        [[nodiscard]] static CrossPlatformPayloadHeader
            BuildCrossPlatformPayloadHeaderForAnalysis(
                const spTextureData& textureData) noexcept;
        // Same field5 reader for explicit inspection. Observation is the input
        // pixel offset after the actual four-word header; no runtime state.
        using CrossReadObserverForAnalysis=std::function<void(const spTextureBuffer&,std::uint32_t pixelOffset)>;
        [[nodiscard]] static bool ReadCrossSectionForAnalysis(spSerializerReadContextForAnalysis&,spStream&,
            std::uint32_t,const std::function<bool(const spTextureBuffer&)>& initialize,bool& initialized,std::string*,
            const CrossReadObserverForAnalysis& observer={});
    protected:
        [[nodiscard]] static bool InitializeCrossDXForAnalysis(spSerializerReadContextForAnalysis&,
            const spTextureBuffer&,spDXTexture&);
        [[nodiscard]] static bool WriteSourceNoneForAnalysis(spStream&,const spTextureData&);
        [[nodiscard]] static bool WriteCrossSectionForAnalysis(spStream&,const spTextureData&);
        // Actual42EA50: same selected virtual reader recurses for field3;
        // no new object header. Context depth/extent are explicit host guards.
        [[nodiscard]] bool ReadSourceWrapperForAnalysis(spSerializerReadContextForAnalysis&,
            spStream&,std::uint32_t,spBaseObject&,std::uint32_t& remaining,bool& handled,std::string*) const;
    };
}
