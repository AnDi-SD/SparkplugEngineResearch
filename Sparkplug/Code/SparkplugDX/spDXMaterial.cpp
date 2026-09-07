#include "spDXMaterial.h"
#include "../Sparkplug/spMaterialColorController.h"
#include <utility>
namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateDXMaterial(){return std::make_unique<spDXMaterial>();}
        const spRTTIRecord Record{spDXMaterial::ClassID,spMaterial::ClassID,"spDXMaterial",&spMaterial::StaticRTTI(),&CreateDXMaterial,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spDXMaterial::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spDXMaterial::vfunc_18() const noexcept{return Record;}
    bool spDXMaterial::UpdateColorForFrameForAnalysis(std::uint32_t frame,bool force,bool* evaluated)
    {
        if(evaluated)*evaluated=false;
        if(GetOpaqueRuntimeFieldForAnalysis()==frame&&!force)return true;
        SetOpaqueRuntimeFieldForAnalysis(frame);
        auto* object=GetMaterialColorControllerForAnalysis();if(!object)return true;
        auto* controller=dynamic_cast<spMaterialColorController*>(object);if(!controller)return false;
        if(controller->GetAppliedTimeForAnalysis()==controller->GetAccumulatedTimeForAnalysis())return true;
        if(evaluated)*evaluated=true;return controller->UpdateForRenderForAnalysis();
    }
    std::unique_ptr<spBaseObject> spDXMaterial::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spDXMaterial>();manager.RegisterClone(*this,*clone);
        return vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
    bool spDXMaterial::vfunc_14(spBaseObject& destination,spCloneManager&) const
    {
        auto* target=dynamic_cast<spDXMaterial*>(&destination);
        // Actual4A9570 calls Material423880's clone graph, then copies17 words.
        // Shared pass/controller clone policy is not reproduced by aliasing it.
        if(!target||GetPassCountForAnalysis()||target->GetPassCountForAnalysis()
            ||GetMaterialColorControllerForAnalysis()||target->GetMaterialColorControllerForAnalysis())return false;
        for(std::size_t i=0;i<RenderStateCount;++i)(void)target->SetRenderStateForAnalysis(i,GetRenderStateForAnalysis(i));
        target->SetVertexAlphaByteForAnalysis(GetVertexAlphaByteForAnalysis());
        target->SetRenderOverrideByteForAnalysis(GetRenderOverrideByteForAnalysis());
        target->diffuse_=diffuse_;target->ambient_=ambient_;target->specular_=specular_;target->emissive_=emissive_;
        target->power_=power_;target->powerInitialized_=powerInitialized_;
        // Physical name, Base auxiliary pointer and runtime +70 are not copied.
        return true;
    }
}
