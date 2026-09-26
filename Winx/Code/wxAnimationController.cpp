#include "wxAnimationController.h"
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        wxAnimationControllerHost* factoryHost = nullptr;
        std::unique_ptr<spBaseObject> CreateController()
        {
            if(!factoryHost) throw std::logic_error("wxAnimationController requires a factory host");
            return std::make_unique<wxAnimationController>(*factoryHost);
        }
        // Registration ancestry only, without factories for missing parents.
        const spRTTIRecord entityRecord{0x22875AA1,spNamedObject::ClassID,"spEntity",&spNamedObject::StaticRTTI(),nullptr,nullptr};
        const spRTTIRecord wxEntityRecord{0x796A1869,0x22875AA1,"wxEntity",&entityRecord,nullptr,nullptr};
        const spRTTIRecord record{wxAnimationController::ClassID,0x796A1869,"wxAnimationController",&wxEntityRecord,&CreateController,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxAnimationController::wxAnimationController(wxAnimationControllerHost& host):host_(host)
    {
        host_.ConstructEntityForAnalysis(*this,true);
        state_.request.mode=0;state_.request.weight=0;
        state_.request.fadeInDuration=state_.request.fadeOutDuration=0.5f;
    }
    wxAnimationController::~wxAnimationController()
    {
        ReleaseActorForAnalysis();host_.DestroyEntityForAnalysis(*this);
    }
    void wxAnimationController::SetFactoryHostForAnalysis(wxAnimationControllerHost* h) noexcept {factoryHost=h;}
    const spRTTIRecord& wxAnimationController::StaticRTTI() noexcept {(void)registered;return record;}
    const spRTTIRecord& wxAnimationController::vfunc_18() const noexcept {return record;}
    std::unique_ptr<spBaseObject> wxAnimationController::vfunc_10(spCloneManager& cm) const
    {
        auto copy=std::make_unique<wxAnimationController>(host_);
        cm.RegisterCloneForAnalysis(*this,*copy);
        if(!vfunc_14(*copy,cm))return nullptr;
        return copy;
    }
    bool wxAnimationController::vfunc_14(spBaseObject& destination,spCloneManager& cm) const
    {
        auto* target=dynamic_cast<wxAnimationController*>(&destination);
        if(!target)return false; // portable guard; native requires valid type
        if(!host_.CopyEntityForAnalysis(*this,*target,cm))return false;
        if(target->state_.actor)
        {
            target->host_.DestroyActorForAnalysis(target->state_.actor);
            target->state_.actor=nullptr;
        }
        // No StopAll, no own state copy; native clone-marker is an empty PC hook.
        return true;
    }
    void wxAnimationController::ReleaseActorForAnalysis() noexcept
    {
        if(state_.actor)
        {
            host_.StopAllForAnalysis(state_.actor);
            if(state_.actor)host_.DestroyActorForAnalysis(state_.actor);
            state_.actor=nullptr;
        }
    }
    void wxAnimationController::InitializeForAnalysis()
    {
        state_.forceEnable=1;
        if(!state_.actor)
        {
            state_.actor=host_.CreateActorForAnalysis();
            host_.BindActorForAnalysis(state_.actor,host_.GetEntityRootForAnalysis(state_.entity));
            host_.AddActorObserverForAnalysis(state_.actor,*this);
        }
        state_.character=host_.FindCharacterForAnalysis(state_.entity,0x0003CC73);
    }
    std::uint32_t wxAnimationController::StartForAnalysis(const Animation* animation,std::uint32_t mode,std::uint32_t fadeMode,std::uint8_t interrupt)
    {
        if(interrupt&&state_.old)host_.StopActorForAnalysis(state_.actor,state_.old,false);
        state_.old=state_.recent;state_.recent=animation;
        state_.request.animation=animation;state_.request.mode=mode;state_.request.reverse=false;state_.request.fadeMode=fadeMode;
        auto& flags=host_.ActorFlagsForAnalysis(state_.actor);flags.enabled1C=flags.changed24=1;
        const auto result=host_.StartActorForAnalysis(state_.actor,state_.request);
        ++state_.count; // includes actor rejection; observes callback modifications
        return result;
    }
    void wxAnimationController::StopForAnalysis(const Animation* a) {host_.StopActorForAnalysis(state_.actor,a,false);}
    void wxAnimationController::StopAllForAnalysis() {host_.StopAllForAnalysis(state_.actor);state_.count=0;}
    void wxAnimationController::FadeStopForAnalysis(const Animation* a,float d) {host_.FadeStopActorForAnalysis(state_.actor,a,d,0);}
    void wxAnimationController::RestartReverseForAnalysis(const Animation* a)
    {
        float field54,field34;
        if(!host_.FindPlaybackTimesForAnalysis(state_.actor,a,field54,field34))return;
        state_.request.reverse=true;state_.request.initialTime=static_cast<float>(double(field54)-field34);
        host_.StopActorForAnalysis(state_.actor,a,false);
        state_.request.animation=a;
        (void)host_.StartActorForAnalysis(state_.actor,state_.request);
    }
    void wxAnimationController::SetTimeMultiplierForAnalysis(float v) {host_.SetActorTimeMultiplierForAnalysis(state_.actor,v);}
    bool wxAnimationController::HasMarkedForAnalysis(const Animation* a,std::uint8_t consume) noexcept
    {
        if(!a)return true;
        if(state_.mark3&&state_.mark3==a)
        {
            if(consume) {state_.mark3=nullptr;if(state_.mark9==a)state_.mark9=nullptr;}
            return true;
        }
        if(state_.mark9&&state_.mark9==a)
        {if(consume)state_.mark9=nullptr;return true;}
        return false;
    }
    void wxAnimationController::ClearMarksForAnalysis(const Animation* a) noexcept
    {if(state_.mark3==a)state_.mark3=nullptr;if(state_.mark9==a)state_.mark9=nullptr;}
    bool wxAnimationController::vfunc_34_UpdateActorFlagsForAnalysis(const void*) noexcept
    {
        const auto flags=host_.GetEntityFlagsForAnalysis(state_.entity);
        const auto mask=state_.character&&!host_.CharacterField148NonzeroForAnalysis(state_.character)?8u:0x1Au;
        const bool allow=state_.forceEnable||!(flags&mask);
        if(state_.actor)
        {
            auto& actorFlags=host_.ActorFlagsForAnalysis(state_.actor);
            if(allow)
            {
                if(state_.recent&&!actorFlags.enabled1C)actorFlags.enabled1C=actorFlags.changed24=1;
            }
            else if(actorFlags.enabled1C){actorFlags.enabled1C=0;actorFlags.changed24=1;}
        }
        state_.forceEnable=0;return false;
    }
    void wxAnimationController::MarkForAnalysis(const Animation* a,bool code3) noexcept
    {
        if(state_.old==a){state_.old=nullptr;--state_.count;}
        else if(state_.recent==a){state_.recent=nullptr;--state_.count;}
        (code3?state_.mark3:state_.mark9)=a;
    }
    void wxAnimationController::vfunc_0C(const void* notification) noexcept
    {
        const auto& message=*static_cast<const wxAnimationMessageForAnalysis*>(notification);
        switch(message.code)
        {
        case 0x1C:InitializeForAnalysis();break;
        case 3:MarkForAnalysis(message.animation1C,true);host_.ForwardForAnalysis(*this,message);break;
        case 9:MarkForAnalysis(message.animation1C,false);break;
        case 11:
            if(state_.character)
            {
                host_.DispatchStateTagForAnalysis(state_.character,message);
                // Re-read character after the state callback, as the original does.
                auto* character=state_.character;
                if(auto* audio=host_.GetAudioEmitterForAnalysis(character))
                    host_.DispatchAudioTagForAnalysis(audio,message,host_.GetAudioVariantForAnalysis(character));
            }
            break;
        default:break;
        }
    }
}
