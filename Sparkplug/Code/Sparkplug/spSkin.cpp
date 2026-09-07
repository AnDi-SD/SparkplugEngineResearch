#include "spSkin.h"

#include "spNode.h"
#include "spMaterialPassLayer.h"
#include "spFog.h"
#include "Analysis/PC/spSkinRenderContext.h"
#include "../SparkplugDX/spDXMesh.h"
#include "../SparkplugDX/spDXMaterial.h"

#include <algorithm>
#include <utility>
#include <cstring>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateSkin()
        {
            return std::make_unique<spSkin>();
        }

        const spRTTIRecord SkinRecord{
            spSkin::ClassID,
            spModel::ClassID,
            "spSkin",
            &spModel::StaticRTTI(),
            &CreateSkin,
            nullptr,
        };

        const bool SkinRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(SkinRecord);
    }

    bool spSkin::BoneBinding::operator==(const BoneBinding& other) const noexcept
    {
        return bone == other.bone && inverseBindMatrix == other.inverseBindMatrix;
    }

    spSkin::~spSkin() = default;

    const spRTTIRecord& spSkin::StaticRTTI() noexcept
    {
        (void)SkinRegistered;
        return SkinRecord;
    }

    std::unique_ptr<spBaseObject> spSkin::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spSkin>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    bool spSkin::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        auto* skin = dynamic_cast<spSkin*>(&destination);
        // Native self-copy frees its own source arrays before reading them.
        if (skin == nullptr || skin == this || !spModel::vfunc_14(destination, manager))
        {
            return false;
        }

        skin->weightCount_ = weightCount_;
        skin->boneBindings_ = boneBindings_;

        // Native PC46A650->412C40 reuses a map hit or CLONES on a miss.
        // Root transaction depth determines whether repeated references can
        // reuse a new clone. Shared ownership is explicit host storage.
        for (auto& binding : skin->boneBindings_)
        {
            if (binding.bone == nullptr)
            {
                continue;
            }
            auto mapped=std::dynamic_pointer_cast<spNode>(manager.CloneReferenceForAnalysis(*binding.bone));
            if(!mapped)return false; // host guard for unavailable/cyclic owner
            binding.bone=std::move(mapped);
        }
        return true;
    }

    const spRTTIRecord& spSkin::vfunc_18() const noexcept
    {
        return SkinRecord;
    }

    bool spSkin::SetPaletteForAnalysis(
        const std::uint32_t weightCount,
        std::vector<BoneBinding> bindings)
    {
        if (std::any_of(bindings.begin(), bindings.end(),
                [](const BoneBinding& binding) { return binding.bone == nullptr; }))
        {
            return false;
        }
        weightCount_ = weightCount;
        boneBindings_ = std::move(bindings);
        return true;
    }

    std::uint32_t spSkin::GetWeightCountForAnalysis() const noexcept
    {
        return weightCount_;
    }

    std::size_t spSkin::GetBoneCountForAnalysis() const noexcept
    {
        return boneBindings_.size();
    }

    const std::vector<spSkin::BoneBinding>&
        spSkin::GetBoneBindingsForAnalysis() const noexcept
    {
        return boneBindings_;
    }

    bool spSkin::RenderForAnalysis(sparkplug::evidence::pc::SkinRenderContextForAnalysis& state,
        spCamera* camera,void* support)
    {
        using R=spDXRenderer;
        // Native423FD0 tests alpha routing BEFORE callbacks and all draw input
        // reads. Skin returns false even when enqueue succeeded or was disabled.
        const auto* initialMaterial=dynamic_cast<const spDXMaterial*>(GetMaterialForAnalysis().get());
        const auto* initialPass=initialMaterial?dynamic_cast<const spMaterialPassLayer*>(initialMaterial->GetPassForAnalysis(0)):nullptr;
        if(initialPass&&initialPass->GetFinalBlendOperationForAnalysis()!=0&&IsAlphaSortEnabledForAnalysis())
        {
            if(!state.alphaQueue)return false; // unavailable renderer alpha state
            if(!state.alphaQueue->flushing&&state.alphaQueue->sortTransparent)
            {
                (void)R::EnqueueAlphaForAnalysis(*state.alphaQueue,this,state.alphaSupport,state.alphaCamera,GetPriorityForAnalysis());
                return false;
            }
        }
        if(!DispatchCallbackPhaseForAnalysis(CallbackPhaseForAnalysis::Pre,camera,support))return false;
        const auto* mesh=dynamic_cast<const spDXMesh*>(GetBaseMeshForAnalysis().get());
        const auto supportedMaterial=[&]()
        {
            if(!GetMaterialForAnalysis())return true;
            const auto* material=dynamic_cast<const spDXMaterial*>(GetMaterialForAnalysis().get());
            const auto* pass=material?dynamic_cast<const spMaterialPassLayer*>(material->GetPassForAnalysis(0)):nullptr;
            return pass
                &&(!material->GetRenderOverrideByteForAnalysis()||state.sharedSavedRenderStateByte);
        };
        if(!supportedMaterial()||(GetFogForAnalysis()&&!dynamic_cast<const spFog*>(GetFogForAnalysis().get()))||!mesh||!state.fallback||!state.shaders
            ||!state.setMatrix||state.constants.blendMatrices.size()<boneBindings_.size())return false;
        // Native423FD0 re-reads the material after the user callback.
        auto* material=dynamic_cast<spDXMaterial*>(GetMaterialForAnalysis().get());
        if(material&&material->GetRenderOverrideByteForAnalysis())
        {*state.sharedSavedRenderStateByte=state.renderStateByte;state.renderStateByte=0;}
        if(!state.preserveMaterialSelection)
        {state.selectedMaterial=material?material:state.fallback;state.packedColor=GetField28ForAnalysis();}
        if(!state.selectedMaterial)return false;
        // Original pre ignores SetFog's false result (including invalid type),
        // while its identity publication and device cache changes remain.
        (void)R::ApplyFogForAnalysis(state.fog,dynamic_cast<const spFog*>(GetFogForAnalysis().get()),
            state.submission.deviceStates,state.device.render,state.device.context);
        for(std::size_t i=0;i<boneBindings_.size();++i)
        {
            const auto& binding=boneBindings_[i];
            const auto palette=ComposePaletteMatrixForAnalysis(binding.inverseBindMatrix,binding.bone->GetWorldMatrixForAnalysis());
            std::memcpy(state.constants.blendMatrices[i].data(),palette.data(),sizeof(palette));
        }
        R::MatrixStateForAnalysis::RawMatrix identity{};
        identity[0]=identity[5]=identity[10]=identity[15]=0x3f800000;
        // Original publishes the bone count AFTER world SetTransform. Device
        // HRESULT is ignored by the original setter and its shared source core.
        if(!R::SetInputMatrixForAnalysis(state.matrices,0,identity,0,state.setMatrix,state.device.context))return false;
        state.activeBoneCount=static_cast<std::uint32_t>(boneBindings_.size());
        state.constants.material=state.selectedMaterial;state.constants.rendererMatrices=&state.matrices;
        state.constants.constantColor=state.packedColor;
        state.submission.lighting.packedColorC194=state.packedColor;
        const auto declaration=mesh->GetVertexDeclarationForAnalysis();
        const auto* resolved=declaration?declaration.get():state.sharedDeclaration;
        if(!R::SubmitGeometryForAnalysis(state.submission,resolved,mesh->GetDXIndexBufferForAnalysis(),
            mesh->GetDXVertexBufferForAnalysis(),state.geometryHandles,mesh->GetVertexStrideForAnalysis(),
            {static_cast<std::uint32_t>(mesh->GetIndexTypeForAnalysis()),mesh->GetIndexBeginForAnalysis(),
             mesh->GetPrimitiveCountForAnalysis(),mesh->GetVertexBeginForAnalysis(),mesh->GetVertexCountForAnalysis(),
             mesh->GetVertexComponentFlagsForAnalysis(),0},*state.selectedMaterial,*state.fallback,*state.shaders,
             state.device,&state.constants,state.shaderGeneration,state.lights))return false;
        // Native4240D0 checks the current material and restores before either
        // grouped or direct post callback, including a false direct result.
        material=dynamic_cast<spDXMaterial*>(GetMaterialForAnalysis().get());
        if(material&&material->GetRenderOverrideByteForAnalysis())
        {if(!state.sharedSavedRenderStateByte)return false;state.renderStateByte=*state.sharedSavedRenderStateByte;}
        if(!DispatchCallbackPhaseForAnalysis(CallbackPhaseForAnalysis::Post,camera,support))return false;
        // Original failure exits preserve the published count; success clears.
        state.activeBoneCount=0;return true;
    }

    spSkin::Matrix4 spSkin::ComposePaletteMatrixForAnalysis(
        const Matrix4& inverseBind, const Matrix4& boneWorld) noexcept
    {
        Matrix4 result{};
        // Exact flattened index mapping. Float arithmetic is behavioral, not
        // a bit-for-bit emulation of each original x87 accumulation order.
        for (std::size_t row = 0; row < 4; ++row)
        {
            for (std::size_t column = 0; column < 4; ++column)
            {
                for (std::size_t inner = 0; inner < 4; ++inner)
                {
                    result[4 * row + column] +=
                        inverseBind[4 * row + inner] * boneWorld[4 * inner + column];
                }
            }
        }
        return result;
    }
}
