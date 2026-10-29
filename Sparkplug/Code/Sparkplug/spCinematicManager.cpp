#include "spCinematicManager.h"
#include "../../Analysis/Host/spCinematicManagerHost.h"
#include <stdexcept>

namespace sparkplug::reconstruction
{
    spCinematicManager::spCinematicManager(spCinematicCapturedOwnerContextForAnalysis& context)
    {
        const auto& state=context.state_;
        if(!state||state->binding||state->initialCount>registry_.size())
            throw std::logic_error("Cinematic context is unavailable or already bound");
        for(std::size_t i=0;i<state->initialCount;++i)
        {
            std::string_view name;
            if(!state->owners[i]||!spCinematicCapturedOwnerContextForAnalysis::ReadPlainNameForAnalysis(state->owners[i]->GetName(),name))
                throw std::logic_error("Cinematic context requires intact retained Names");
        }
        // All checks precede binding; fixed borrowed slots do not allocate.
        for(std::size_t i=0;i<state->initialCount;++i)registry_[i]=state->owners[i].get();
        count_=state->initialCount;context_=state;state->binding=this;
    }
    spCinematicManager::~spCinematicManager()
    {
        if(auto state=context_.lock();state&&state->binding==this)state->binding=nullptr;
        registry_.fill(nullptr);count_=0;
        // Do not reset any payload owner. Context disposal belongs to caller.
    }
    const spRTTIRecord& spCinematicManager::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID,spBaseObject::ClassID,"spCinematicManager",
            &spBaseObject::StaticRTTI(),nullptr,nullptr};
        static const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;return record;
    }
    const spRTTIRecord& spCinematicManager::vfunc_18() const noexcept{return StaticRTTI();}
    std::unique_ptr<spBaseObject> spCinematicManager::vfunc_10(spCloneManager&) const
    {throw std::logic_error("CinematicManager Clone/original construction remain unqualified");}
    const spCinematic* spCinematicManager::GetCinematicForAnalysis(std::size_t index) const noexcept
    {return index<count_&&!context_.expired()?registry_[index]:nullptr;}
    void spCinematicManager::ValidateForOperation(const std::shared_ptr<spCinematicCapturedOwnerStateForAnalysis>& state) const
    {
        if(!state||state->binding!=this||count_>registry_.size())
            throw std::logic_error("CinematicManager requires its live exclusive captured context");
        std::size_t ownedCount=0;
        for(const auto& owner:state->owners)ownedCount+=bool(owner);
        if(ownedCount!=count_)throw std::logic_error("CinematicManager captured owners no longer match registry");
        for(std::size_t i=0;i<count_;++i)
        {
            const auto* object=registry_[i];bool owned=false;std::string_view name;
            for(const auto& owner:state->owners)owned|=owner.get()==object&&bool(owner);
            if(!owned||!spCinematicCapturedOwnerContextForAnalysis::ReadPlainNameForAnalysis(object->GetName(),name))
                throw std::logic_error("CinematicManager requires retained plain Names");
            for(std::size_t j=0;j<i;++j)if(registry_[j]==object)
                throw std::logic_error("CinematicManager captured registry identity is not unique");
        }
    }
    bool spCinematicManager::RemoveByNamePcForAnalysis(std::string_view name)
    {
        if(removing_||!spCinematicCapturedOwnerContextForAnalysis::IsPlainQueryForAnalysis(name))
            throw std::logic_error("CinematicManager Name query/reentry is outside captured profile");
        auto state=context_.lock();ValidateForOperation(state);
        removing_=true;struct Reset{bool& value;~Reset(){value=false;}} reset{removing_};
        for(std::size_t i=0;i<count_;++i)
        {
            std::string_view current;
            (void)spCinematicCapturedOwnerContextForAnalysis::ReadPlainNameForAnalysis(registry_[i]->GetName(),current);
            if(current!=name)continue;
            for(auto& owner:state->owners)if(owner.get()==registry_[i])
            {
                owner.reset(); // Actual C++ lifetime ends BEFORE registry unlink.
                break;
            }
            for(std::size_t j=i+1;j<count_;++j)registry_[j-1]=registry_[j];
            registry_[--count_]=nullptr;return true;
        }
        return false;
    }
}
