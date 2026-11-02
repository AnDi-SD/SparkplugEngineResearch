#include "wxMosquitoAttackAIAction.h"
#include "wxCharacterMoveCtrl.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateMosquitoAttack()
        {throw std::logic_error("Mosquito Exit requires explicit captured state and borrowed bindings");}
        const spRTTIRecord record{wxMosquitoAttackAIAction::ClassID,wxAIAction::ClassID,
            "wxMosquitoAttackAIAction",&wxAIAction::StaticRTTI(),&CreateMosquitoAttack,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxMosquitoAttackAIAction::wxMosquitoAttackAIAction(wxMosquitoAttackAIActionHost& host,
        const wxMosquitoAttackAIActionCapturedStateForAnalysis& state,ProfileForAnalysis profile) noexcept
        :host_(host),profile_(profile),state_(state){}
    const spRTTIRecord& wxMosquitoAttackAIAction::StaticRTTI() noexcept{(void)registered;return record;}
    const spRTTIRecord& wxMosquitoAttackAIAction::vfunc_18() const noexcept{return record;}
    std::unique_ptr<spBaseObject> wxMosquitoAttackAIAction::vfunc_10(spCloneManager&) const
    {throw std::logic_error("Mosquito Clone remains unqualified");}
    bool wxMosquitoAttackAIAction::vfunc_14(spBaseObject&,spCloneManager&) const{return false;}
    wxMosquitoAttackAIActionResultForAnalysis wxMosquitoAttackAIAction::ExitForAnalysis() noexcept
    {
        using Result=wxMosquitoAttackAIActionResultForAnalysis;
        if(profile_!=ProfileForAnalysis::PcCapturedStableBorrowedExit
            ||GetCurrentActionForAnalysis()!=nullptr||GetOwnedActionCountForAnalysis()!=0)
            return lastResult_=Result::UnqualifiedProfile;
        auto* owner=GetOwnerForAnalysis();if(!owner)return lastResult_=Result::MissingOwner;
        auto* character=host_.ReadCharacter144ForAnalysis(owner);if(!character)return lastResult_=Result::MissingCharacter;
        auto* command=host_.ReadCommand130ForAnalysis(character);if(!command)return lastResult_=Result::MissingCommand;
        auto* cell=host_.BorrowCommandByte1DForAnalysis(command);if(!cell)return lastResult_=Result::MissingCommandCell;
        *cell=0;
        // Each native chain starts from own20 again; no captured owner or
        // character/command pointer is reused for a later destination.
        owner=GetOwnerForAnalysis();if(!owner)return lastResult_=Result::MissingOwner;
        character=host_.ReadCharacter144ForAnalysis(owner);if(!character)return lastResult_=Result::MissingCharacter;
        command=host_.ReadCommand130ForAnalysis(character);if(!command)return lastResult_=Result::MissingCommand;
        cell=host_.BorrowCommandByte20ForAnalysis(command);if(!cell)return lastResult_=Result::MissingCommandCell;
        *cell=0;
        owner=GetOwnerForAnalysis();if(!owner)return lastResult_=Result::MissingOwner;
        character=host_.ReadCharacter144ForAnalysis(owner);if(!character)return lastResult_=Result::MissingCharacter;
        auto* controller=host_.ReadController12CForAnalysis(character);if(!controller)return lastResult_=Result::MissingController;
        // Real shared captured controller storage, not a successful movement
        // callback or the controller's unrelated15-word reset operation.
        controller->ClearWord1E0ForAnalysis();
        state_.borrowedTarget3A8=nullptr;
        return lastResult_=Result::Completed;
    }
    void wxMosquitoAttackAIAction::vfunc_34_ClearForAnalysis() noexcept{(void)ExitForAnalysis();}
    void wxMosquitoAttackAIAction::vfunc_0C(const void*) noexcept{lastResult_=wxMosquitoAttackAIActionResultForAnalysis::UnqualifiedNotify;}
    bool wxMosquitoAttackAIAction::RefuseHookForAnalysis() noexcept{lastResult_=wxMosquitoAttackAIActionResultForAnalysis::UnqualifiedHook;return false;}
    bool wxMosquitoAttackAIAction::vfunc_24() noexcept{return RefuseHookForAnalysis();}
    bool wxMosquitoAttackAIAction::vfunc_28() noexcept{return RefuseHookForAnalysis();}
    bool wxMosquitoAttackAIAction::vfunc_2C() noexcept{return RefuseHookForAnalysis();}
    void wxMosquitoAttackAIAction::vfunc_30() noexcept{(void)RefuseHookForAnalysis();}
    void wxMosquitoAttackAIAction::vfunc_38() noexcept{(void)RefuseHookForAnalysis();}
    bool wxMosquitoAttackAIAction::vfunc_3C(const void*) noexcept{return RefuseHookForAnalysis();}
    bool wxMosquitoAttackAIAction::vfunc_40(const void*) noexcept{return RefuseHookForAnalysis();}
}
