#include "wxKikoMovingAIAction.h"
#include "Analysis/PC/wxKikoMovingAIActionAbi.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateCapturedMessageAction()
        { throw std::logic_error("wxKikoMovingAIAction requires supplied captured state"); }
        const spRTTIRecord record{wxKikoMovingAIAction::ClassID,wxAIAction::ClassID,
            "wxKikoMovingAIAction",&wxAIAction::StaticRTTI(),&CreateCapturedMessageAction,nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxKikoMovingAIAction::wxKikoMovingAIAction(const CapturedState& state,
        ProfileForAnalysis profile) noexcept : state_(state),profile_(profile) {}
    const spRTTIRecord& wxKikoMovingAIAction::StaticRTTI() noexcept { (void)registered;return record; }
    const spRTTIRecord& wxKikoMovingAIAction::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxKikoMovingAIAction::vfunc_10(spCloneManager&) const
    { throw std::logic_error("KikoMoving native Clone remains unqualified"); }
    bool wxKikoMovingAIAction::vfunc_14(spBaseObject&,spCloneManager&) const { return false; }
    std::uint32_t wxKikoMovingAIAction::WordForAnalysis(std::size_t offset) const noexcept
    {
        std::uint32_t value = 0;
        for (unsigned byte=0;byte!=4;++byte) value |= std::uint32_t{state_[offset+byte]} << (byte*8);
        return value;
    }
    void wxKikoMovingAIAction::StoreWordForAnalysis(std::size_t offset,std::uint32_t value) noexcept
    {
        for (unsigned byte=0;byte!=4;++byte) state_[offset+byte]=static_cast<std::uint8_t>(value>>(byte*8));
        accesses_[accessCount_++]={AccessForAnalysis::Kind::Write,AccessForAnalysis::Role::CapturedState,
            static_cast<std::uint32_t>(offset),4,value};
    }
    wxKikoMovingAIAction::ResultForAnalysis wxKikoMovingAIAction::HandleMessageForAnalysis(
        const wxAIActionMessageForAnalysis message) noexcept
    {
        accessCount_=0;
        if (profile_!=ProfileForAnalysis::PcCapturedReadOnlyDisjointMessage
            || GetCurrentActionForAnalysis()!=nullptr || GetOwnedActionCountForAnalysis()!=0)
            return result_=ResultForAnalysis::UnqualifiedProfile;
        if (WordForAnalysis(0)!=evidence::pc::wxKikoMovingAIActionPrimary)
            return result_=ResultForAnalysis::CapturedPrimaryNotAdmitted;
        accesses_[accessCount_++]={AccessForAnalysis::Kind::Read,AccessForAnalysis::Role::MessageValue,0,4,message.code};
        if (message.code==0x000027AAU)
        {
            StoreWordForAnalysis(0x3A8,3);
            StoreWordForAnalysis(0x3AC,2);
        }
        return result_=ResultForAnalysis::Completed;
    }
    // Generic void* delivery and all unrelated virtual hooks remain unqualified.
    void wxKikoMovingAIAction::vfunc_0C(const void*) noexcept { result_=ResultForAnalysis::UnqualifiedHook; }
    bool wxKikoMovingAIAction::RefuseHookForAnalysis() noexcept { result_=ResultForAnalysis::UnqualifiedHook;return false; }
    bool wxKikoMovingAIAction::vfunc_24() noexcept { return RefuseHookForAnalysis(); }
    bool wxKikoMovingAIAction::vfunc_28() noexcept { return RefuseHookForAnalysis(); }
    bool wxKikoMovingAIAction::vfunc_2C() noexcept { return RefuseHookForAnalysis(); }
    void wxKikoMovingAIAction::vfunc_30() noexcept { (void)RefuseHookForAnalysis(); }
    void wxKikoMovingAIAction::vfunc_34_ClearForAnalysis() noexcept { (void)RefuseHookForAnalysis(); }
    void wxKikoMovingAIAction::vfunc_38() noexcept { (void)RefuseHookForAnalysis(); }
    bool wxKikoMovingAIAction::vfunc_3C(const void*) noexcept { return RefuseHookForAnalysis(); }
    bool wxKikoMovingAIAction::vfunc_40(const void*) noexcept { return RefuseHookForAnalysis(); }
}
