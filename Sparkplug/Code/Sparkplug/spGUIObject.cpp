#include "spGUIObject.h"
#include "spNode.h"
#include "spCollisionInfo.h"
#include <stdexcept>

namespace sparkplug::reconstruction
{
    namespace
    {
        spGUIHost* factoryHost = nullptr;
        std::unique_ptr<spBaseObject> CreateGUIObject()
        {
            if(!factoryHost) throw std::logic_error("spGUIObject requires an explicit GUI host");
            return std::make_unique<spGUIObject>(*factoryHost);
        }
        const spRTTIRecord record{spGUIObject::ClassID,spEntity::ClassID,
            "spGUIObject",&spEntity::StaticRTTI(),&CreateGUIObject,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    spGUIObject::spGUIObject(spGUIHost& host):spEntity(host,1) {}
    spGUIObject::~spGUIObject()
    {
        auto& host=GUIHostForAnalysis();
        host.DestroyGUIForAnalysis(*this);
        if(auto* node=GetNodeForGUIForAnalysis())
        {
            host.UnsubscribeGUINodeForAnalysis(*node,*this);
            for(std::size_t i=0;i<node->GetChildCountForAnalysis();++i)
                host.UnsubscribeGUINodeForAnalysis(*node->GetChildForAnalysis(i),*this);
        }
        // Native destructor does not restore saved collision groups. Base
        // Entity teardown/release remains in its existing host boundary.
    }
    void spGUIObject::SetFactoryHostForAnalysis(spGUIHost* host) noexcept { factoryHost=host; }
    spGUIHost* spGUIObject::GetFactoryHostForAnalysis() noexcept { return factoryHost; }
    const spRTTIRecord& spGUIObject::StaticRTTI() noexcept { (void)registered;return record; }
    const spRTTIRecord& spGUIObject::vfunc_18() const noexcept { return record; }
    spGUIHost& spGUIObject::GUIHostForAnalysis() const noexcept { return static_cast<spGUIHost&>(HostForAnalysis()); }
    spNode* spGUIObject::GetNodeForGUIForAnalysis() const noexcept { return static_cast<spNode*>(GetReferenceForAnalysis()); }
    std::unique_ptr<spBaseObject> spGUIObject::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spGUIObject>(GUIHostForAnalysis());
        manager.RegisterCloneForAnalysis(*this,*clone);
        if(!vfunc_14(*clone,manager))return nullptr;
        return clone;
    }
    void spGUIObject::CopyGUIStateToForAnalysis(spGUIObject& destination) const { destination.enabled_=enabled_; }
    bool spGUIObject::vfunc_14(spBaseObject& destination,spCloneManager& manager) const
    {
        auto* gui=dynamic_cast<spGUIObject*>(&destination);
        if(!gui||!spNamedObject::vfunc_14(destination,manager))return false;
        CopyGUIStateToForAnalysis(*gui);
        // Entity reference, saved history and manager runtime state are not copied.
        return true;
    }
    void spGUIObject::SetReferenceForAnalysis(void* next)
    {
        auto& host=GUIHostForAnalysis();
        if(auto* old=GetNodeForGUIForAnalysis())
        {
            host.UnsubscribeGUINodeForAnalysis(*old,*this);
            for(std::size_t i=0;i<old->GetChildCountForAnalysis();++i)
                host.UnsubscribeGUINodeForAnalysis(*old->GetChildForAnalysis(i),*this);
            for(std::size_t i=0;i<GetNodeForGUIForAnalysis()->GetCollisionCountForAnalysis()&&i<saved_.size();++i)
                GetNodeForGUIForAnalysis()->GetCollisionForAnalysis(i)->SetGroupForAnalysis(saved_[i]);
        }
        spEntity::SetReferenceForAnalysis(next);
        if(auto* node=GetNodeForGUIForAnalysis())
        {
            for(std::size_t i=0;i<node->GetChildCountForAnalysis();++i)
                host.SubscribeGUINodeForAnalysis(*node->GetChildForAnalysis(i),*this);
            host.SubscribeGUINodeForAnalysis(*node,*this);
            for(std::size_t i=0;i<GetNodeForGUIForAnalysis()->GetCollisionCountForAnalysis();++i)
            {
                saved_.push_back(GetNodeForGUIForAnalysis()->GetCollisionForAnalysis(i)->GetGroupForAnalysis());
                GetNodeForGUIForAnalysis()->GetCollisionForAnalysis(i)->SetGroupForAnalysis(4);
            }
        }
    }
    void spGUIObject::SetEnabledForAnalysis(std::uint8_t value)
    {
        enabled_=value;
        auto* node=GetNodeForGUIForAnalysis();
        if(!node)throw std::logic_error("GUI enabled operation requires the original nonnull Node dependency");
        if(auto* match=node->FindNodeForAnalysis("GUICollision",true,true))
            for(std::size_t i=0;i<match->GetCollisionCountForAnalysis();++i)
                match->GetCollisionForAnalysis(i)->SetGroupForAnalysis(4);
    }
    void spGUIObject::NotifyBaseForAnalysis(const spGUINotificationForAnalysis& n)
    {
        if(n.code==0x18)
        {
            if(!n.payload)throw std::logic_error("GUI notification18 requires a nonnull output cell");
            *reinterpret_cast<spGUIObject**>(n.payload)=this;
        }
        else if(n.code==0x1c)spGUIObject::SetEnabledForAnalysis(enabled_);
    }
    void spGUIObject::vfunc_0C(const void* notification) noexcept
    {
        if(!notification)
        {
            GUIHostForAnalysis().UnrecoveredGUINotificationForAnalysis(*this,notification,"Null GUI notification is outside original input domain");
            return;
        }
        try { NotifyBaseForAnalysis(*static_cast<const spGUINotificationForAnalysis*>(notification)); }
        catch(...) { GUIHostForAnalysis().UnrecoveredGUINotificationForAnalysis(*this,notification,"GUI notification dependency failed"); }
    }
}
