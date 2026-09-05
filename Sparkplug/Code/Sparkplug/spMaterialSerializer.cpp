#include "spMaterialSerializer.h"

#include <memory>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateMaterialSerializer()
        {
            return std::make_unique<spMaterialSerializer>();
        }

        const spRTTIRecord MaterialSerializerRecord{
            spMaterialSerializer::ClassID,
            spSerializer::ClassID,
            "spMaterialSerializer",
            &spSerializer::StaticRTTI(),
            &CreateMaterialSerializer,
            nullptr,
        };

        const bool MaterialSerializerRegistered =
            spRTTIManager::Instance().Register(MaterialSerializerRecord);
    }

    spMaterialSerializer::~spMaterialSerializer() = default;

    const spRTTIRecord& spMaterialSerializer::StaticRTTI() noexcept
    {
        (void)MaterialSerializerRegistered;
        return MaterialSerializerRecord;
    }

    std::unique_ptr<spBaseObject> spMaterialSerializer::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spMaterialSerializer>();
        manager.RegisterClone(*this, *clone);
        return spSerializer::vfunc_14(*clone, manager)
            ? std::move(clone)
            : nullptr;
    }

    const spRTTIRecord& spMaterialSerializer::vfunc_18() const noexcept
    {
        return MaterialSerializerRecord;
    }

    spMaterialSerializer::WritePlan
    spMaterialSerializer::BuildStandardWritePlanForAnalysis(
        const MaterialWriteShape& material)
    {
        WritePlan plan;
        if (material.useVertexAlpha)
        {
            plan.fields.push_back(Field::VertexAlpha);
        }
        plan.fields.push_back(Field::RenderStates);

        for (const auto& pass : material.passes)
        {
            (void)pass.finalBlendOperation;
            plan.fields.push_back(Field::Pass);
            for (const auto& layer : pass.layers)
            {
                plan.fields.push_back(Field::Layer);

                if (layer.classID == MovieLayerClassID)
                {
                    plan.fields.push_back(Field::Movie);
                    continue;
                }

                const bool commonTexture =
                    layer.classID == StandardLayerClassID
                    || layer.classID == EnvironmentMapLayerClassID
                    || layer.classID == CubeEnvironmentMapLayerClassID
                    || layer.classID == CameraViewLayerClassID
                    || layer.classID == MirrorLayerClassID;
                if (!commonTexture)
                {
                    plan.fields.clear();
                    return plan;
                }
                plan.fields.push_back(Field::TextureStates);
                if (layer.hasStaticUVTransform)
                {
                    plan.fields.push_back(Field::StaticUVTransform);
                }
                if (layer.hasTexture)
                {
                    plan.fields.push_back(Field::Texture);
                }
                if (layer.hasAnimationController)
                {
                    plan.fields.push_back(Field::AnimationController);
                }
                if (layer.hasUVController)
                {
                    plan.fields.push_back(Field::UVController);
                }

                if (layer.classID == EnvironmentMapLayerClassID)
                {
                    plan.fields.push_back(Field::UVGeneration);
                }
                else if (layer.classID == CameraViewLayerClassID)
                {
                    plan.fields.push_back(Field::RenderTarget);
                    if (layer.hasCameraName)
                    {
                        plan.fields.push_back(Field::Camera);
                    }
                }
                else if (layer.classID == MirrorLayerClassID)
                {
                    plan.fields.push_back(Field::RenderTarget);
                    plan.fields.push_back(Field::CubeMap);
                }
            }
        }

        plan.fields.push_back(Field::Color);
        plan.fields.push_back(Field::ColorController);
        plan.valid = true;
        return plan;
    }

    std::vector<spMaterialSerializer::Relationship>
    spMaterialSerializer::BuildStandardIndexPlanForAnalysis(
        const MaterialWriteShape& material)
    {
        std::vector<Relationship> plan;
        for (const auto& pass : material.passes)
        {
            for (const auto& layer : pass.layers)
            {
                if (layer.classID == MovieLayerClassID)
                {
                    continue;
                }
                if (layer.classID != StandardLayerClassID
                    && layer.classID != EnvironmentMapLayerClassID
                    && layer.classID != CubeEnvironmentMapLayerClassID
                    && layer.classID != CameraViewLayerClassID
                    && layer.classID != MirrorLayerClassID)
                {
                    return {};
                }
                if (layer.hasTexture)
                {
                    plan.push_back(Relationship::Texture);
                }
                if (layer.hasUVController)
                {
                    plan.push_back(Relationship::UVController);
                }
                if (layer.hasAnimationController)
                {
                    plan.push_back(Relationship::AnimationController);
                }
            }
        }
        plan.push_back(Relationship::ColorController);
        return plan;
    }

    bool spMaterialSerializer::IsKnownReadFieldForAnalysis(
        const std::uint32_t fieldID) noexcept
    {
        switch (static_cast<Field>(fieldID))
        {
        case Field::RenderStates:
        case Field::VertexAlpha:
        case Field::Color:
        case Field::Pass:
        case Field::Layer:
        case Field::ColorController:
        case Field::UVGeneration:
        case Field::LegacyTextureStates:
        case Field::StaticUVTransform:
        case Field::Texture:
        case Field::AnimationController:
        case Field::UVController:
        case Field::RenderTarget:
        case Field::Camera:
        case Field::CubeMap:
        case Field::Movie:
        case Field::TextureStates:
            return true;
        default:
            return false;
        }
    }
}
