#include "wxBirdMovingState.h"
#include "Analysis/Host/wxBirdMovingStateHost.h"

#include <cstring>
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateBirdMovingState()
        { return std::make_unique<wxBirdMovingState>(); }
        const spRTTIRecord record{wxBirdMovingState::ClassID, wxCharacterState::ClassID,
            "wxBirdMovingState", &wxCharacterState::StaticRTTI(), &CreateBirdMovingState, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        wxBirdMovingStateHost& RequireBirdHost(wxCharacterStateHost& host)
        {
            auto* adapter = dynamic_cast<wxBirdMovingStateHost*>(&host);
            if (!adapter) throw std::logic_error("wxBirdMovingState requires a bird-moving host");
            return *adapter;
        }
    }
    const spRTTIRecord& wxBirdMovingState::StaticRTTI() noexcept
    { (void)registered; return record; }
    const spRTTIRecord& wxBirdMovingState::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxBirdMovingState::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxBirdMovingState>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxBirdMovingState::vfunc_1C(wxAnimationRequestForAnalysis& request)
    {
        cycleCount_=0;
        needsPlayback_=true;
        return wxCharacterState::vfunc_1C(request);
    }
    void wxBirdMovingState::vfunc_30(wxAnimationRequestForAnalysis& request)
    {
        if (needsPlayback_)
        {
            if (cycleCount_==0)
            {
                auto& host=RequireBirdHost(RequireHostForAnalysis());
                switch (host.DrawRandomForAnalysis()%3u)
                {
                case 0: request.packedKey &= 0xF07FFF8Fu; break;
                case 1: request.packedKey = (request.packedKey & 0xF0FFFF8Fu) | 0x00800000u; break;
                case 2:
                    cycleCount_=(host.DrawRandomForAnalysis()&3u)+1u;
                    request.packedKey=(request.packedKey&0xF07FFFDFu)|0x50u;
                    break;
                }
            }
            request.packedKey &= 0xFF9FFFF0u;
            void* const handle=RequireHostForAnalysis().ResolveAnimationForAnalysis(GetOwnerForAnalysis(),request.packedKey);
            // Playback neither compares nor releases the previous handle.
            QueuePendingFromState(handle,false,true);
            SetPendingHandleFromState(handle);
            needsPlayback_=false;
            return;
        }
        if (RequireHostForAnalysis().IsPendingAnimationCompleteForAnalysis(
                GetCompletionConsumerForAnalysis(),GetPendingHandleForAnalysis(),true))
        {
            needsPlayback_=true;
            if (cycleCount_!=0) --cycleCount_;
        }
    }
    void wxBirdMovingState::vfunc_3C(const void* event)
    {
        auto& host=RequireBirdHost(RequireHostForAnalysis());
        const char* const name=host.EventTagNameForAnalysis(event);
        if (!name) throw std::logic_error("wxBirdMovingState host returned a null event tag name");
        if (std::strcmp(name,"event_jump")==0) host.JumpFromEventForAnalysis(GetOwnerForAnalysis());
        else if (std::strcmp(name,"event_land")==0) host.LandFromEventForAnalysis(GetOwnerForAnalysis());
    }
}
