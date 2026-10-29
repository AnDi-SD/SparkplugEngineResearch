#include "wxDroidWanderAIAction.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateDroid()
        {throw std::logic_error("DroidWander requires an explicit post-construction command binding");}
        const spRTTIRecord record{wxDroidWanderAIAction::ClassID,wxAIAction::ClassID,
            "wxDroidWanderAIAction",&wxAIAction::StaticRTTI(),&CreateDroid,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxDroidWanderAIAction::wxDroidWanderAIAction(wxDroidWanderAIActionHost& host,void* owner) noexcept:host_(host)
    {SetOwnerForAnalysis(owner);}
    const spRTTIRecord& wxDroidWanderAIAction::StaticRTTI() noexcept{(void)registered;return record;}
    const spRTTIRecord& wxDroidWanderAIAction::vfunc_18() const noexcept{return record;}
    std::unique_ptr<spBaseObject> wxDroidWanderAIAction::vfunc_10(spCloneManager&) const
    {throw std::logic_error("DroidWander Clone remains unqualified");}
    bool wxDroidWanderAIAction::vfunc_14(spBaseObject&,spCloneManager&) const
    {return false;}
    bool wxDroidWanderAIAction::AdmitForAnalysis() noexcept
    {
        if(host_.CommandProfileForAnalysis()!=wxDroidWanderAIActionHost::ProfileForAnalysis::PcStableBorrowedCommandData
            ||GetCurrentActionForAnalysis()!=nullptr||GetOwnedActionCountForAnalysis()!=0)
        {lastResult_=wxDroidWanderResultForAnalysis::UnqualifiedProfile;return false;}
        return true;
    }
    wxDroidWanderResultForAnalysis wxDroidWanderAIAction::PublishCommandForAnalysis() noexcept
    {
        if(!AdmitForAnalysis())return lastResult_;
        auto* owner=GetOwnerForAnalysis();
        if(!owner)return lastResult_=wxDroidWanderResultForAnalysis::MissingOwner;
        auto* command=host_.ReadCommandForAnalysis(owner);
        if(!command)return lastResult_=wxDroidWanderResultForAnalysis::MissingCommand;
        host_.WriteCommandWordForAnalysis(command,4,0);
        // The whole chain is reloaded after the first store, including own20.
        owner=GetOwnerForAnalysis();
        if(!owner)return lastResult_=wxDroidWanderResultForAnalysis::MissingOwner;
        command=host_.ReadCommandForAnalysis(owner);
        if(!command)return lastResult_=wxDroidWanderResultForAnalysis::MissingCommand;
        host_.WriteCommandByteForAnalysis(command,0x1C,1);
        return lastResult_=wxDroidWanderResultForAnalysis::Completed;
    }
    wxDroidWanderResultForAnalysis wxDroidWanderAIAction::ClearCommandForAnalysis() noexcept
    {
        if(!AdmitForAnalysis())return lastResult_;
        auto* owner=GetOwnerForAnalysis();
        if(!owner)return lastResult_=wxDroidWanderResultForAnalysis::MissingOwner;
        auto* command=host_.ReadCommandForAnalysis(owner);
        if(!command)return lastResult_=wxDroidWanderResultForAnalysis::MissingCommand;
        host_.WriteCommandByteForAnalysis(command,0x1C,0);
        return lastResult_=wxDroidWanderResultForAnalysis::Completed;
    }
    bool wxDroidWanderAIAction::vfunc_24() noexcept
    {return PublishCommandForAnalysis()==wxDroidWanderResultForAnalysis::Completed;}
    void wxDroidWanderAIAction::vfunc_34_ClearForAnalysis() noexcept{(void)ClearCommandForAnalysis();}
    void wxDroidWanderAIAction::vfunc_0C(const void*) noexcept{lastResult_=wxDroidWanderResultForAnalysis::UnqualifiedNotify;}
    bool wxDroidWanderAIAction::vfunc_28() noexcept{lastResult_=wxDroidWanderResultForAnalysis::UnqualifiedTick;return false;}
    void wxDroidWanderAIAction::vfunc_30() noexcept{lastResult_=wxDroidWanderResultForAnalysis::UnqualifiedEntry;}
    void wxDroidWanderAIAction::vfunc_38() noexcept{lastResult_=wxDroidWanderResultForAnalysis::UnqualifiedQuery;}
}
