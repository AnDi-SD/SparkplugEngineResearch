#include "wxHUDPart.h"
#include "Code/SparkBase/spSubscriptionManager.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        wxHUDPartHost* factoryHost=nullptr;
        spSubscriptionManager* factorySubscriptions=nullptr;
        std::unique_ptr<spBaseObject> CreatePart()
        {
            if(!factoryHost||!factorySubscriptions)throw std::logic_error("wxHUDPart requires an explicit host and current SubscriptionManager");
            return std::make_unique<wxHUDPart>(*factoryHost,*factorySubscriptions);
        }
        const spRTTIRecord record{wxHUDPart::ClassID,wxEntity::ClassID,
            "wxHUDPart",&wxEntity::StaticRTTI(),&CreatePart,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    wxHUDPart::wxHUDPart(wxHUDPartHost& host,spSubscriptionManager& subscriptions)
        :wxEntity(host,1),host_(host),subscriptions_(subscriptions) {}
    wxHUDPart::~wxHUDPart()
    {
        // Native frees its pointer vector before unsubscribing and clears it
        // again after the foreign HUDManager call. Entries remain borrowed.
        std::vector<wxButton*>().swap(buttons_);
        (void)subscriptions_.UnsubscribeForAnalysis(3,*this);
        auto* manager=host_.ResolveHUDManagerForPartForAnalysis();
        if(!manager)
        {
            host_.UnrecoveredHUDPartDependencyForAnalysis(*this,nullptr,
                "HUDPart foreign HUDManager resolve is unqualified");
            return;
        }
        // Native loads13C after its lazy manager resolve, which may call out.
        const auto key=key_;
        host_.RemoveHUDPartFromManagerForAnalysis(manager,key);
        std::vector<wxButton*>().swap(buttons_);
    }
    void wxHUDPart::SetFactoryContextForAnalysis(wxHUDPartHost* host,spSubscriptionManager* subscriptions) noexcept
    { factoryHost=host;factorySubscriptions=subscriptions; }
    const spRTTIRecord& wxHUDPart::StaticRTTI() noexcept { (void)registered;return record; }
    const spRTTIRecord& wxHUDPart::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxHUDPart::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<wxHUDPart>(host_,subscriptions_);
        manager.RegisterCloneForAnalysis(*this,*clone);
        if(!vfunc_14(*clone,manager))return nullptr;
        return clone;
    }
    bool wxHUDPart::vfunc_14(spBaseObject& destination,spCloneManager& manager) const
    {
        auto* part=dynamic_cast<wxHUDPart*>(&destination);
        if(part==this)throw std::logic_error("HUDPart self-copy is outside the qualified source domain");
        if(!part||!wxEntity::vfunc_14(*part,manager))return false;
        if(!part->buttons_.empty())std::vector<wxButton*>().swap(part->buttons_);
        for(auto* button:buttons_)part->buttons_.push_back(button);
        part->initial_=initial_;
        part->key_=key_;
        part->flags_[1]=flags_[1];
        part->flags_[0]=flags_[0];
        part->layer_=layer_;
        // selected138 and initialized146 are not copied.
        host_.CompleteHUDPartCopyForAnalysis(ClassID);
        return true;
    }
    void wxHUDPart::NotifyHUDPartForAnalysis(const spNotificationForAnalysis& n)
    {
        if(n.code==0x2715)
        {
            auto* button=const_cast<wxButton*>(static_cast<const wxButton*>(n.sender));
            if(!button)throw std::logic_error("HUDPart2715 requires a live wxButton sender");
            std::uint32_t key;
            std::memcpy(&key,button->GetOwnBytesForAnalysis().data(),4);
            if(key_==key&&std::find(buttons_.begin(),buttons_.end(),button)==buttons_.end())
                buttons_.push_back(button);
        }
        else if(n.code==0x1c&&!flags_[2])
            host_.UnrecoveredHUDPartDependencyForAnalysis(*this,&n,
                "HUDPart init calls external SubscriptionManager/HUDManager;not qualified");
    }
    wxButton* wxHUDPart::FindButtonForAnalysis(const char* action) const
    {
        if(!action)throw std::invalid_argument("HUDPart lookup requires a nonnull action string");
        for(auto* button:buttons_)if(button->GetStringForAnalysis()==action)return button;
        return nullptr;
    }
    void wxHUDPart::SelectButtonForAnalysis(wxButton* candidate)
    {
        if(!candidate)throw std::invalid_argument("HUDPart selection requires a live wxButton");
        auto* previous=initial_;
        if(previous&&previous!=candidate)previous->SetStateForAnalysis(0);
        const bool disabled=candidate->GetSelectionIndexForAnalysis()==1;
        selected_=candidate;
        initial_=disabled?nullptr:candidate;
        if(auto* current=initial_)current->SetStateForAnalysis(2);
        // Capture each recipient before the virtual call; reread the live end
        // after return. Qualification requires stable vector storage.
        for(std::size_t i=0;i<buttons_.size();++i)
        {
            auto* button=buttons_[i];
            button->RefreshSelectionForAnalysis();
        }
    }
    void wxHUDPart::SetHUDStateForAnalysis(std::vector<wxButton*> buttons,wxButton* initial,
        wxButton* selected,std::uint32_t key,std::uint32_t layer,std::array<std::uint8_t,3> flags)
    { buttons_=std::move(buttons);initial_=initial;selected_=selected;key_=key;layer_=layer;flags_=flags; }
    void wxHUDPart::vfunc_0C(const void* notification) noexcept
    {
        if(!notification)
        { host_.UnrecoveredHUDPartDependencyForAnalysis(*this,notification,"Null HUDPart notification");return; }
        try{NotifyHUDPartForAnalysis(*static_cast<const spNotificationForAnalysis*>(notification));}
        catch(...){host_.UnrecoveredHUDPartDependencyForAnalysis(*this,notification,"HUDPart notification dependency failed");}
    }
}
