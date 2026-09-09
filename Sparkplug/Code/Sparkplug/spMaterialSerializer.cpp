#include "spMaterialSerializer.h"
#include "spMaterialData.h"
#include "../SparkplugDX/spDXMaterial.h"
#include "spMaterialPassLayer.h"
#include "spStdLayer.h"
#include "spMaterialTexture.h"
#include "spTexture.h"
#include "spAnimTexController.h"
#include "spUVController.h"
#include "spMaterialColorController.h"
#include "spSerializerManager.h"
#include "spResourceManager.h"
#include "spDataBlockSerializer.h"
#include "Analysis/PC/spSectionCursor.h"
#include <cmath>

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
            spRTTIManager::Instance().RegisterDeferredForAnalysis(MaterialSerializerRecord);

        bool VerifiedStandardShape(const spMaterial& material)
        {
            if(material.GetMaterialColorControllerForAnalysis()
                &&!dynamic_cast<const spMaterialColorController*>(material.GetMaterialColorControllerForAnalysis()))return false;
            for(std::size_t i=0;i<material.GetPassCountForAnalysis();++i)
            {
                const auto* pass=dynamic_cast<const spMaterialPassLayer*>(material.GetPassForAnalysis(i));if(!pass)return false;
                for(std::size_t j=0;j<pass->GetLayerCountForAnalysis();++j)
                {
                    const auto* layer=dynamic_cast<const spStdLayer*>(pass->GetLayerForAnalysis(j).get());
                    if(!layer)return false;const auto* texture=layer->GetMaterialTextureForAnalysis().get();
                    if(!texture||!texture->IsExactly(spMaterialTexture::ClassID))return false;
                    if(texture->GetUVControllerForAnalysis()
                        &&texture->GetUVControllerOwnerForAnalysis().get()!=texture->GetUVControllerForAnalysis())return false;
                    if(texture->GetAnimTextureControllerForAnalysis()
                        &&texture->GetAnimTextureControllerOwnerForAnalysis().get()!=texture->GetAnimTextureControllerForAnalysis())return false;
                    if(texture->GetFallBackTextureForAnalysis()
                        &&texture->GetFallBackTextureOwnerForAnalysis().get()!=texture->GetFallBackTextureForAnalysis())return false;
                }
            }
            return true;
        }
    }

    spMaterialSerializer::spMaterialSerializer() noexcept
    {
        // Host static-library reachability, not native cold RTTI startup.
        (void)spMaterialData::StaticRTTI();
        (void)spDXMaterial::StaticRTTI();
        (void)spStdLayer::StaticRTTI();
    }

    spMaterialSerializer::~spMaterialSerializer() = default;

    bool spMaterialSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& source,std::uint32_t size,spBaseObject& object,std::string* error) const
    { return ReadMaterialFieldsForAnalysis(context,source,size,object,error,nullptr); }

    bool spMaterialSerializer::InspectPayloadForAnalysis(spStream& source,std::uint32_t size,
        spBaseObject& object,InspectionForAnalysis& observation,std::string* error) const
    {
        spSerializerManager manager;spResourceManager resources;
        spSerializerReadContextForAnalysis context(manager,resources);observation={};
        return ReadMaterialFieldsForAnalysis(context,source,size,object,error,&observation);
    }

    bool spMaterialSerializer::ReadMaterialFieldsForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& source,std::uint32_t size,spBaseObject& object,std::string* error,
        InspectionForAnalysis* observation) const
    {
        using sparkplug::evidence::pc::serialization::SectionCursor;
        if(error)error->clear();SectionCursor cursor(context,source,size,true,error);
        std::uint32_t sectionStart=0;
        if(observation&&!source.GetCurrentPosition(sectionStart))return cursor.Fail("Cannot locate material inspection payload");
        auto* material=dynamic_cast<spMaterial*>(&object);
        if(!material||(!object.IsExactly(spMaterialData::ClassID)&&!object.IsExactly(spDXMaterial::ClassID)))return cursor.Fail("PC material target mismatch");
        const auto lastPass=[&]() -> spMaterialPassLayer*
        {return material->GetPassCountForAnalysis()?dynamic_cast<spMaterialPassLayer*>(material->GetPassForAnalysis(material->GetPassCountForAnalysis()-1)):nullptr;};
        const auto lastTexture=[&]() -> spMaterialTexture*
        {auto* pass=lastPass();if(!pass||!pass->GetLayerCountForAnalysis())return nullptr;
            const auto* layer=dynamic_cast<spStdLayer*>(pass->GetLayerForAnalysis(pass->GetLayerCountForAnalysis()-1).get());
            return layer?layer->GetMaterialTextureForAnalysis().get():nullptr;};
        const auto observeScalar=[&](const spDataBlockHeaderForAnalysis& header,
            const spMaterialPassLayer* pass,const spMaterialTexture* texture,bool applied)
        {
            if(!observation)return;
            InspectedScalarFieldForAnalysis field;
            field.field=static_cast<Field>(header.fieldID);
            field.payloadOffset=header.dataStreamPosition-sectionStart;
            field.payloadSize=header.payloadSize;
            field.assignmentOrder=static_cast<std::uint32_t>(observation->scalarFields.size());
            field.pass=pass;field.texture=texture;field.applied=applied;
            if(pass)
            {
                field.passIndex=static_cast<std::uint32_t>(material->GetPassCountForAnalysis()-1);
                if(texture)
                {
                    field.layerIndex=static_cast<std::uint32_t>(pass->GetLayerCountForAnalysis()-1);
                    field.layer=static_cast<const spStdLayer*>(pass->GetLayerForAnalysis(field.layerIndex).get());
                }
            }
            observation->scalarFields.push_back(field);
        };
        const auto color=[](std::uint32_t argb)
        {
            constexpr float unit=1.0F/255.0F; // PC424700 multiplies this float constant
            return spMaterialData::ColorRGBA{float((argb>>16)&255)*unit,float((argb>>8)&255)*unit,float(argb&255)*unit,float(argb>>24)*unit};
        };
        while(const auto* header=cursor.Next())
        {
            if(header->IsTerminator())return true;
            switch(static_cast<Field>(header->fieldID))
            {
            case Field::RenderStates:
            {
                spMaterial::RenderStates states{};if(!cursor.Read(states))return cursor.Fail("Material requires eleven render states");
                for(std::size_t i=0;i<states.size();++i)(void)material->SetRenderStateForAnalysis(i,states[i]);
                observeScalar(*header,nullptr,nullptr,true);break;
            }
            case Field::VertexAlpha:
            {
                std::uint8_t value=0;if(!cursor.Read(value))return cursor.Fail("Material alpha requires one byte");
                material->SetVertexAlphaByteForAnalysis(value);break;
            }
            case Field::Color:
            {
                ColorPayloadForAnalysis value{};static_assert(sizeof(value)==20);
                if(!cursor.Read(value))return cursor.Fail("Material color field requires twenty bytes");
                if(observation)observation->color=value;
                material->SetAmbientColorForAnalysis(color(value.ambient));material->SetDiffuseColorForAnalysis(color(value.diffuse));
                material->SetSpecularColorForAnalysis(color(value.specular));material->SetEmissiveColorForAnalysis(color(value.emissive));
                material->SetSpecularPowerForAnalysis(value.power);break;
            }
            case Field::Pass:
            {
                std::uint32_t blend=0;if(!cursor.Read(blend)||material->GetPassCountForAnalysis()>=spMaterial::MaximumPassCount)
                    return cursor.Fail("Material pass field exceeds size or eight-slot capacity");
                auto pass=std::make_shared<spMaterialPassLayer>();pass->SetFinalBlendOperationForAnalysis(blend);
                if(!material->SetPassForAnalysis(material->GetPassCountForAnalysis(),pass))return cursor.Fail("Cannot retain material pass");
                observeScalar(*header,pass.get(),nullptr,true);break;
            }
            case Field::Layer:
            {
                std::uint32_t identity=0;auto* pass=lastPass();
                if(!cursor.Read(identity)||!pass||pass->GetLayerCountForAnalysis()>=spMaterialPassLayer::MaximumLayerCount)
                    return cursor.Fail("Material layer requires preceding pass and bounded slot");
                if(identity!=spStdLayer::ClassID)return cursor.Fail("Material layer class is not restored");
                auto created=spRTTIManager::Instance().Create(identity);auto* layer=dynamic_cast<spStdLayer*>(created.get());
                if(!layer)return cursor.Fail("Material layer factory mismatch");
                created.release();if(!pass->SetLayerForAnalysis(pass->GetLayerCountForAnalysis(),std::unique_ptr<spMaterialTextureLayer>(layer)))
                    return cursor.Fail("Cannot own material layer");break;
            }
            case Field::LegacyTextureStates:
            case Field::TextureStates:
            {
                auto* texture=lastTexture();if(!texture)
                {
                    if(!cursor.Skip())return cursor.Fail("Cannot skip orphan texture states");
                    observeScalar(*header,lastPass(),nullptr,false);break;
                }
                std::array<std::uint32_t,9> states{};if(!cursor.Read(states))return cursor.Fail("PC material texture requires nine states");
                if(observation)observation->layers[texture].textureStatesField=static_cast<std::int32_t>(header->fieldID);
                for(std::size_t i=0;i<states.size();++i)texture->SetTextureStateForAnalysis(i,states[i]);
                observeScalar(*header,lastPass(),texture,true);break;
            }
            case Field::StaticUVTransform:
            {
                auto* texture=lastTexture();if(!texture){if(!cursor.Skip())return cursor.Fail("Cannot skip orphan UV field");break;}
                struct UV{std::uint32_t enabled;std::array<float,9> matrix;};UV uv{};static_assert(sizeof(uv)==40);
                if(!cursor.Read(uv))return cursor.Fail("Static UV requires flag and nine floats");
                if(observation)observation->layers[texture].hasUVField=true;
                // Native consumes the matrix even if disabled; zero leaves
                // prior matrix/flag intact, not ClearStaticUVTransform.
                if(uv.enabled)texture->SetStaticUVTransformForAnalysis(uv.matrix);break;
            }
            case Field::ColorController:
            case Field::AnimationController:
            case Field::UVController:
            case Field::Texture:
            {
                const auto field=static_cast<Field>(header->fieldID);
                auto* holder=field==Field::ColorController?nullptr:lastTexture();
                if(field!=Field::ColorController&&!holder)
                {if(!cursor.Skip())return cursor.Fail("Cannot skip orphan material reference");break;}
                const auto expected=field==Field::ColorController?spMaterialColorController::ClassID:
                    field==Field::AnimationController?spAnimTexController::ClassID:
                    field==Field::UVController?spUVController::ClassID:spTexture::ClassID;
                spBaseObject* resolved=nullptr;bool nonnull=false;InspectedReferenceForAnalysis inspected{};
                if(observation)
                {
                    if(!evidence::pc::serialization::InspectReference(source,header->payloadSize,true,inspected,error))
                        return cursor.Fail("Cannot inspect bounded material reference");
                    nonnull=inspected.id!=0;
                }
                else
                {
                    resolved=spSerializer::ReadFieldReferenceForAnalysis(context,expected,source,*header,error);
                    if(context.failed)return false;
                    nonnull=resolved!=nullptr;
                }
                // One shared original policy: controllers preserve on NULL;
                // fallback Texture calls41E870 even for NULL and clears it.
                if(!nonnull&&field!=Field::Texture)break;
                if(observation)
                {
                    if(field==Field::ColorController)observation->colorController=inspected;
                    else observation->layers[holder].references[field==Field::Texture?0:field==Field::AnimationController?1:2]=inspected;
                    break;
                }
                if(field==Field::ColorController)
                {
                    auto* controller=dynamic_cast<spMaterialColorController*>(resolved);
                    auto owner=context.ShareObjectForAnalysis(resolved);
                    if(!controller||!owner)return cursor.Fail("Material color reference requires canonical confirmed target");
                    material->SetMaterialColorControllerForAnalysis(std::move(owner));
                }
                else if(field==Field::AnimationController)
                {
                    auto owned=std::dynamic_pointer_cast<spAnimTexController>(context.ShareObjectForAnalysis(resolved));
                    if(!owned)return cursor.Fail("Material animation controller has no canonical owner");
                    holder->SetOwnedAnimTextureControllerForAnalysis(std::move(owned));
                }
                else if(field==Field::UVController)
                {
                    auto owned=std::dynamic_pointer_cast<spUVController>(context.ShareObjectForAnalysis(resolved));
                    if(!owned)return cursor.Fail("Material UV controller has no canonical owner");
                    holder->SetOwnedUVControllerForAnalysis(std::move(owned));
                }
                else
                {
                    auto owned=std::dynamic_pointer_cast<spTexture>(context.ShareObjectForAnalysis(resolved));
                    if(resolved&&!owned)return cursor.Fail("Material texture reference has no canonical texture owner");
                    holder->SetOwnedFallBackTextureForAnalysis(std::move(owned));
                }
                break;
            }
            default:
                if(IsKnownReadFieldForAnalysis(header->fieldID))return cursor.Fail("Material layer dependency is not restored");
                if(!cursor.Skip())return cursor.Fail("Cannot skip unknown material field");break;
            }
        }
        return false;
    }

    bool spMaterialSerializer::WritePayloadForAnalysis(spStream& destination,const spBaseObject& object,std::string* error) const
    {spSerializerManager manager;return WriteMaterialFieldsForAnalysis(&manager,destination,object,error);}

    bool spMaterialSerializer::WritePayloadWithContextForAnalysis(spSerializerManager& manager,
        spStream& destination,const spBaseObject& object,std::string* error) const
    {return WriteMaterialFieldsForAnalysis(&manager,destination,object,error);}

    bool spMaterialSerializer::WriteRenderStatesFieldForAnalysis(spStream& stream,
        const spMaterial& material,std::string* error)
    {
        if(error)error->clear();
        // PC476BDC..476C2D: field0 contains material+18's eleven words.
        const auto& states=material.GetRenderStatesForAnalysis();
        static_assert(sizeof(states)==RenderStateCount*sizeof(std::uint32_t));
        if(!spDataBlockSerializer{}.WriteFieldForAnalysis(stream,0,states.data(),sizeof(states)))
        {if(error)*error="Cannot write material states";return false;}
        return true;
    }

    bool spMaterialSerializer::WritePassBlendFieldForAnalysis(spStream& stream,
        const spMaterialPassLayer& pass,std::string* error)
    {
        if(error)error->clear();
        // PC476CB1/476CC2: field3 contains the actual pass's blend word+10.
        const auto blend=pass.GetFinalBlendOperationForAnalysis();
        if(!spDataBlockSerializer{}.WriteFieldForAnalysis(stream,3,&blend,sizeof(blend)))
        {if(error)*error="Cannot write material pass";return false;}
        return true;
    }

    bool spMaterialSerializer::WriteTextureStatesFieldForAnalysis(spStream& stream,
        const spMaterialTexture& texture,std::string* error)
    {
        if(error)error->clear();
        // PC476270..476286: field17 copies exactly 36 bytes from holder+10.
        // The portable holder has twelve slots; the PC wire owns only nine.
        const auto& states=texture.GetTextureStatesForAnalysis();
        if(!spDataBlockSerializer{}.WriteFieldForAnalysis(stream,17,states.data(),TextureStateCount*sizeof(std::uint32_t)))
        {if(error)*error="Cannot write standard layer states";return false;}
        return true;
    }

    bool spMaterialSerializer::WriteMaterialFieldsForAnalysis(spSerializerManager* manager,
        spStream& stream,const spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();const auto* material=dynamic_cast<const spMaterial*>(&object);
        const auto fail=[&](const char* text){if(error)*error=text;return false;};
        if(!material||(!object.IsExactly(spMaterialData::ClassID)&&!object.IsExactly(spDXMaterial::ClassID)))return fail("PC material writer target mismatch");
        if(!material->HasInitializedSpecularPowerForAnalysis())return fail("Native DX material specular power was never initialized");
        if(!VerifiedStandardShape(*material))return fail("Unrestored material layer/controller/texture or sparse graph");
        // PC x87 scales by255 then _ftol truncates, storing only low bytes.
        // Safe host admits normalized finite colors; out-of-range CRT behavior
        // remains open and must not invoke undefined floating-to-int conversion.
        const auto pack=[](const spMaterialData::ColorRGBA& color,std::uint32_t& argb)
        {
            constexpr unsigned shifts[]{16,8,0,24};argb=0;
            for(std::size_t i=0;i<4;++i)
            {if(!std::isfinite(color[i])||color[i]<0||color[i]>1)return false;
                argb|=static_cast<std::uint32_t>(double(color[i])*255.0)<<shifts[i];}
            return true;
        };
        struct Colors{std::uint32_t ambient,diffuse,specular,emissive;float power;};Colors colors{};
        if(!pack(material->GetAmbientColorForAnalysis(),colors.ambient)||!pack(material->GetDiffuseColorForAnalysis(),colors.diffuse)
            ||!pack(material->GetSpecularColorForAnalysis(),colors.specular)||!pack(material->GetEmissiveColorForAnalysis(),colors.emissive))
            return fail("Material color outside verified finite normalized range");
        colors.power=material->GetSpecularPowerForAnalysis();spDataBlockSerializer blocks;
        if(!blocks.BeginObjectForAnalysis(stream,material))return fail("Cannot begin material");
        const auto alpha=material->GetVertexAlphaByteForAnalysis();
        if(alpha&&!blocks.WriteFieldForAnalysis(stream,1,&alpha,sizeof(alpha)))return fail("Cannot write material alpha");
        if(!WriteRenderStatesFieldForAnalysis(stream,*material,error))return false;
        for(std::size_t i=0;i<material->GetPassCountForAnalysis();++i)
        {
            const auto* pass=static_cast<const spMaterialPassLayer*>(material->GetPassForAnalysis(i));
            if(!WritePassBlendFieldForAnalysis(stream,*pass,error))return false;
            for(std::size_t j=0;j<pass->GetLayerCountForAnalysis();++j)
            {
                const auto* layer=static_cast<const spStdLayer*>(pass->GetLayerForAnalysis(j).get());const auto* texture=layer->GetMaterialTextureForAnalysis().get();
                const std::uint32_t identity=spStdLayer::ClassID;
                if(!blocks.WriteFieldForAnalysis(stream,4,&identity,4))return fail("Cannot write standard layer states");
                if(!WriteTextureStatesFieldForAnalysis(stream,*texture,error))return false;
                if(texture->HasStaticTransformForAnalysis())
                {
                    struct UV{std::uint32_t enabled;std::array<float,9> matrix;};const UV uv{1,texture->GetUVTransformForAnalysis()};
                    if(!blocks.WriteFieldForAnalysis(stream,9,&uv,sizeof(uv)))return fail("Cannot write static UV");
                }
                if(const auto* fallback=texture->GetFallBackTextureForAnalysis())
                {
                    if(!manager||!blocks.WriteBeginForAnalysis(10,spDataBlockSerializer::SizeCode::UInt32)
                        ||!spSerializer::WriteReferenceForAnalysis(*manager,stream,fallback,error)
                        ||!blocks.WriteEndForAnalysis(10))return fail("Cannot write material texture reference");
                }
                if(const auto* animation=texture->GetAnimTextureControllerForAnalysis())
                {
                    if(!manager||!blocks.WriteBeginForAnalysis(11,spDataBlockSerializer::SizeCode::UInt32)
                        ||!spSerializer::WriteReferenceForAnalysis(*manager,stream,animation,error)
                        ||!blocks.WriteEndForAnalysis(11))return fail("Cannot write material animation controller reference");
                }
                if(const auto* uv=texture->GetUVControllerForAnalysis())
                {
                    if(!manager||!blocks.WriteBeginForAnalysis(12,spDataBlockSerializer::SizeCode::UInt32)
                        ||!spSerializer::WriteReferenceForAnalysis(*manager,stream,uv,error)
                        ||!blocks.WriteEndForAnalysis(12))return fail("Cannot write material UV controller reference");
                }
            }
        }
        if(!blocks.WriteFieldForAnalysis(stream,2,&colors,sizeof(colors))
            ||!blocks.WriteBeginForAnalysis(6,spDataBlockSerializer::SizeCode::UInt32)
            ||!manager||!spSerializer::WriteReferenceForAnalysis(*manager,stream,material->GetMaterialColorControllerForAnalysis(),error)
            ||!blocks.WriteEndForAnalysis(6)||!blocks.FinalizeObjectForAnalysis())return fail("Cannot finalize material section");
        return true;
    }

    bool spMaterialSerializer::IndexRelationshipsWithContextForAnalysis(spSerializerManager& manager,spBaseObject& object) const
    {
        const auto* material=dynamic_cast<const spMaterial*>(&object);
        if(!material||(!object.IsExactly(spMaterialData::ClassID)&&!object.IsExactly(spDXMaterial::ClassID))||!VerifiedStandardShape(*material))return false;
        for(std::size_t i=0;i<material->GetPassCountForAnalysis();++i)
        {
            const auto* pass=static_cast<const spMaterialPassLayer*>(material->GetPassForAnalysis(i));
            for(std::size_t j=0;j<pass->GetLayerCountForAnalysis();++j)
            {
                const auto* holder=pass->GetLayerForAnalysis(j)->GetMaterialTextureForAnalysis().get();
                if(!spSerializer::IndexReferenceForAnalysis(manager,holder->GetFallBackTextureForAnalysis())
                    ||!spSerializer::IndexReferenceForAnalysis(manager,holder->GetAnimTextureControllerForAnalysis())
                    ||!spSerializer::IndexReferenceForAnalysis(manager,holder->GetUVControllerForAnalysis()))return false;
            }
        }
        return spSerializer::IndexReferenceForAnalysis(manager,material->GetMaterialColorControllerForAnalysis());
    }

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
