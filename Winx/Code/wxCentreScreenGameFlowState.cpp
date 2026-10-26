#include "wxCentreScreenGameFlowState.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        wxCentreScreenGameFlowStateHost* factoryHost=nullptr;
        std::unique_ptr<spBaseObject> CreateState()
        {
            if(!factoryHost)throw std::logic_error("wxCentreScreenGameFlowState requires a factory host");
            return std::make_unique<wxCentreScreenGameFlowState>(*factoryHost);
        }
        const spRTTIRecord record{wxCentreScreenGameFlowState::ClassID,wxGameFlowState::ClassID,
            "wxCentreScreenGameFlowState",&wxGameFlowState::StaticRTTI(),&CreateState,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        template<class T> T* Require(T* value)
        {
            if(!value)throw std::logic_error("wxCentreScreenGameFlowState requires a typed material or borrowed service");
            return value; // Analytical safety guard, not native fault/lazy-startup behavior.
        }
    }
    wxCentreScreenGameFlowState::wxCentreScreenGameFlowState(wxCentreScreenGameFlowStateHost& host)
        :wxGameFlowState(host),centreHost_(host){}
    void wxCentreScreenGameFlowState::SetFactoryHostForAnalysis(wxCentreScreenGameFlowStateHost* host) noexcept
    {factoryHost=host;}
    const spRTTIRecord& wxCentreScreenGameFlowState::StaticRTTI() noexcept{(void)registered;return record;}
    const spRTTIRecord& wxCentreScreenGameFlowState::vfunc_18() const noexcept{return record;}
    std::unique_ptr<spBaseObject> wxCentreScreenGameFlowState::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<wxCentreScreenGameFlowState>(centreHost_);
        manager.RegisterCloneForAnalysis(*this,*clone);
        return vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
    spMaterial* wxCentreScreenGameFlowState::ReadMaterialForAnalysis() const
    {
        if(!centre_.material3CKnown)throw std::logic_error("CentreScreen material3C is unwritten; an explicit input is required");
        return centre_.material3C;
    }
    void wxCentreScreenGameFlowState::FlowSlot20ForAnalysis()
    {
        auto* material=Require(centreHost_.CreateCentreMaterialForAnalysis());
        // There is no release of the previous raw pointer, even on repeated Entry.
        centre_.material3C=material;centre_.material3CKnown=true;
        auto& refs=centreHost_.BorrowCentreMaterialRefCount8ForAnalysis(*material);
        refs=static_cast<std::uint16_t>(refs+1u);
        Require(ReadMaterialForAnalysis())->SetRenderOverrideByteForAnalysis(1);
        (void)Require(ReadMaterialForAnalysis())->SetRenderStateForAnalysis(8,2);
        (void)Require(ReadMaterialForAnalysis())->SetRenderStateForAnalysis(7,2);
        (void)Require(ReadMaterialForAnalysis())->SetRenderStateForAnalysis(3,0);
        auto pass=centreHost_.CreateCentrePassForAnalysis();Require(pass.get());
        // Native reloads own3C after the pass factory. Retain that observation.
        material=Require(ReadMaterialForAnalysis());
        if(!material->SetPassForAnalysis(material->GetPassCountForAnalysis(),pass))
            throw std::domain_error("CentreScreen material pass capacity exceeded");
        pass->SetFinalBlendOperationForAnalysis(2);
        auto layer=centreHost_.CreateCentreStdLayerForAnalysis();Require(layer.get());
        if(!pass->SetLayerForAnalysis(pass->GetLayerCountForAnalysis(),std::move(layer)))
            throw std::domain_error("CentreScreen layer capacity exceeded");
        auto* owner=Require(centreHost_.ResolveCentreAppOwnerForAnalysis());
        centre_.word40=centreHost_.ReadCentreAppWord64ForAnalysis(owner);
        owner=Require(centreHost_.ResolveCentreAppOwnerForAnalysis());
        centre_.word44=centreHost_.ReadCentreAppWord68ForAnalysis(owner);
    }
    void wxCentreScreenGameFlowState::FlowSlot24ForAnalysis()
    {
        if(auto* material=ReadMaterialForAnalysis())
        {
            auto& refs=centreHost_.BorrowCentreMaterialRefCount8ForAnalysis(*material);
            refs=static_cast<std::uint16_t>(refs-1u);
            if(refs==0)centreHost_.DestroyCentreMaterialForAnalysis(*material,1);
            // This store follows the virtual delete, including its mutations.
            centre_.material3C=nullptr;centre_.material3CKnown=true;
        }
        auto* window=Require(centreHost_.ResolveCentreWindowForAnalysis());
        centreHost_.BorrowCentreWindowWord1294ForAnalysis(window)=0;
    }
    bool wxCentreScreenGameFlowState::FlowSlot30ForAnalysis()
    {throw std::logic_error("CentreScreen Slot30 is outside the recovered ownership contract");}
}
