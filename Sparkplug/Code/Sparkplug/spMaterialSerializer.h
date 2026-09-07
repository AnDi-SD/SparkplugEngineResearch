#pragma once

// Exact original PC translation-unit path recovered from diagnostics:
// Z:\Sparkplug\Code\Sparkplug\spMaterialSerializer.cpp

#include "spSerializer.h"

#include <cstdint>
#include <vector>

namespace sparkplug::reconstruction
{
    class spMaterialSerializer : public spSerializer
    {
    public:
        static constexpr spClassID ClassID = 0x2A14745F;
        static constexpr spClassID StandardLayerClassID = 0x234C576B;
        static constexpr spClassID EnvironmentMapLayerClassID = 0x427C7480;
        static constexpr spClassID CubeEnvironmentMapLayerClassID = 0x4DED3E44;
        static constexpr spClassID CameraViewLayerClassID = 0x194613E1;
        static constexpr spClassID MirrorLayerClassID = 0x46B61C67;
        static constexpr spClassID MovieLayerClassID = 0x075F3EB6;
        static constexpr std::uint32_t RenderStateCount = 11;
        static constexpr std::uint32_t TextureStateCount = 9;

        enum class Field : std::uint32_t
        {
            RenderStates = 0,
            VertexAlpha = 1,
            Color = 2,
            Pass = 3,
            Layer = 4,
            ColorController = 6,
            UVGeneration = 7,
            LegacyTextureStates = 8,
            StaticUVTransform = 9,
            Texture = 10,
            AnimationController = 11,
            UVController = 12,
            RenderTarget = 13,
            Camera = 14,
            CubeMap = 15,
            Movie = 16,
            TextureStates = 17,
        };

        enum class Relationship : std::uint8_t
        {
            Texture,
            UVController,
            AnimationController,
            ColorController,
        };

        struct LayerWriteShape final
        {
            spClassID classID = StandardLayerClassID;
            bool hasStaticUVTransform = false;
            bool hasTexture = false;
            bool hasAnimationController = false;
            bool hasUVController = false;
            bool hasCameraName = false;
        };

        struct PassWriteShape final
        {
            std::uint32_t finalBlendOperation = 0;
            std::vector<LayerWriteShape> layers;
        };

        struct MaterialWriteShape final
        {
            bool useVertexAlpha = false;
            std::vector<PassWriteShape> passes;
        };

        struct WritePlan final
        {
            bool valid = false;
            std::vector<Field> fields;
        };

        spMaterialSerializer() noexcept;
        ~spMaterialSerializer() override;

        spMaterialSerializer(const spMaterialSerializer&) = delete;
        spMaterialSerializer& operator=(const spMaterialSerializer&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        [[nodiscard]] bool ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
            spStream& source,std::uint32_t byteCount,spBaseObject& object,std::string* error) const override;
        [[nodiscard]] bool WritePayloadForAnalysis(spStream& destination,
            const spBaseObject& object,std::string* error) const override;
        [[nodiscard]] bool WritePayloadWithContextForAnalysis(spSerializerManager& manager,
            spStream& destination,const spBaseObject& object,std::string* error) const override;
        [[nodiscard]] bool IndexRelationshipsWithContextForAnalysis(spSerializerManager& manager,
            spBaseObject& object) const override;

    private:
        [[nodiscard]] bool WriteMaterialFieldsForAnalysis(spSerializerManager* manager,
            spStream& destination,const spBaseObject& object,std::string* error) const;

    public:

        // This base owns the common material grammar but does not expose a
        // confirmed target-ID hook. Concrete material-data serializers add it.
        [[nodiscard]] static WritePlan BuildStandardWritePlanForAnalysis(
            const MaterialWriteShape& material);
        [[nodiscard]] static std::vector<Relationship>
            BuildStandardIndexPlanForAnalysis(const MaterialWriteShape& material);
        [[nodiscard]] static bool IsKnownReadFieldForAnalysis(
            std::uint32_t fieldID) noexcept;
    };
}
