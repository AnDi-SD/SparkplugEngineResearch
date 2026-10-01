#include "wxGlyphState.h"
#include "Analysis/Host/wxGlyphStateHost.h"

#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateGlyphState(){return std::make_unique<wxGlyphState>();}
        const spRTTIRecord record{wxGlyphState::ClassID,wxCharacterState::ClassID,
            "wxGlyphState",&wxCharacterState::StaticRTTI(),&CreateGlyphState,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxGlyphState::wxGlyphState() noexcept{SetStateSelectorForConstruction(StateSelector);}
    const spRTTIRecord& wxGlyphState::StaticRTTI() noexcept{(void)registered;return record;}
    const spRTTIRecord& wxGlyphState::vfunc_18() const noexcept{return record;}
    std::unique_ptr<spBaseObject> wxGlyphState::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<wxGlyphState>();manager.RegisterCloneForAnalysis(*this,*clone);
        if(!vfunc_14(*clone,manager))return nullptr;return clone;
    }
    std::uint32_t wxGlyphState::ComposeKeyForAnalysis(std::uint32_t key,std::uint32_t classification) noexcept
    {
        key&=0xFF87FF80u;
        switch(classification)
        {
        case 15:return key&0xF07FFFFFu;
        case 34:return (key&0xF0FFFFFFu)|0x00800000u;
        case 44:return (key&0xF17FFFFFu)|0x01000000u;
        case 46:return (key&0xF1FFFFFFu)|0x01800000u;
        default:return key;
        }
    }
    void wxGlyphState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        request.packedKey&=0xFF87FF80u;
        auto* host=dynamic_cast<wxGlyphStateHost*>(&RequireHostForAnalysis());
        if(!host)throw std::logic_error("wxGlyphState requires a glyph-state host");
        request.packedKey=ComposeKeyForAnalysis(request.packedKey,
            host->ReadOwnerClassificationWordForAnalysis(GetOwnerForAnalysis()));
        void* const handle=host->ResolveAnimationForAnalysis(GetOwnerForAnalysis(),request.packedKey);
        if(handle!=GetPendingHandleForAnalysis())
        {
            // Original PC5A7D13 / PS22CD7F8 does not release the old handle.
            QueuePendingFromState(handle,false,true);
            SetPendingHandleFromState(handle);
        }
        ClearOwnerActionControlFromState();
    }
    bool wxGlyphState::vfunc_34(std::uint32_t)
    {
        return RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
            GetCompletionConsumerForAnalysis(),GetPendingHandleForAnalysis(),true);
    }
}
