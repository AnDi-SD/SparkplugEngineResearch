#include "spButton.h"
#include "spNode.h"
#include "spCollisionInfo.h"
#include <stdexcept>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateButton()
        {
            auto* host=spGUIObject::GetFactoryHostForAnalysis();
            if(!host)throw std::logic_error("spButton requires an explicit GUI host");
            return std::make_unique<spButton>(*host);
        }
        const spRTTIRecord record{spButton::ClassID,spWidget::ClassID,
            "spButton",&spWidget::StaticRTTI(),&CreateButton,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    const spRTTIRecord& spButton::StaticRTTI() noexcept { (void)registered;return record; }
    const spRTTIRecord& spButton::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> spButton::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spButton>(GUIHostForAnalysis());
        manager.RegisterCloneForAnalysis(*this,*clone);
        if(!vfunc_14(*clone,manager))return nullptr;
        return clone;
    }
    bool spButton::vfunc_14(spBaseObject& destination,spCloneManager& manager) const
    {
        auto* button=dynamic_cast<spButton*>(&destination);
        if(!button||!spWidget::vfunc_14(destination,manager))return false;
        button->string_=string_;button->pressed_=pressed_;button->pressedNode_=pressedNode_;
        return true;
    }
    void spButton::RefreshSelectionForAnalysis()
    {
        if(!HidePreviousSelectionForAnalysis())return;
        if(pressed_&&pressedNode_)SelectAndShowForAnalysis(GetSelectionIndexForAnalysis(),pressedNode_);
        else
        {
            const auto index=!GetEnabledByteForAnalysis()?1u:GetHighlightByteForAnalysis()?2u:0u;
            SelectAndShowForAnalysis(index,GetStateNodesForAnalysis()[index]);
        }
    }
    void spButton::NotifyButtonForAnalysis(const spGUINotificationForAnalysis& n)
    {
        if(n.code==0x1c)
        {
            if(auto* root=GetNodeForGUIForAnalysis())
            {
                pressedNode_=root->FindNodeForAnalysis("PUSHED",true,true);
                if(pressedNode_)
                {
                    GUIHostForAnalysis().SetGUINodeEnabledForAnalysis(*pressedNode_,0,true);
                    for(std::size_t i=0;i<pressedNode_->GetCollisionCountForAnalysis();++i)
                        pressedNode_->GetCollisionForAnalysis(i)->SetGroupForAnalysis(4);
                }
            }
            NotifyWidgetForAnalysis(n);
            return;
        }
        if(n.code==0x0f&&GetEnabledByteForAnalysis())
        {
            const auto event=n.payload;
            if(!event)throw std::logic_error("Button notificationF requires a nonnull native pointed event");
            const auto eventWord=GUIHostForAnalysis().ReadGUIInputWord18ForAnalysis(event);
            if(!eventWord)
            {
                // PC reads ownedString54 before owner24. Preserve that order
                // even when a host getter exposes callback-visible state.
                const auto payload=reinterpret_cast<std::uintptr_t>(string_.c_str());
                auto* target=GUIHostForAnalysis().GetGUINotificationTargetForAnalysis(*this);
                SendDirectedNotificationForAnalysis(GUIHostForAnalysis(),this,target,0x19,payload,0);
                pressed_=1;
                RefreshSelectionForAnalysis();
            }
        }
        else if((n.code==0x10||n.code==0x12)&&GetEnabledByteForAnalysis()&&pressed_)
        {
            pressed_=0;
            RefreshSelectionForAnalysis();
        }
        // Original re-enters Widget, rereading enabled and the live input code.
        //12 can therefore refresh twice; F can emit19 then forward original F.
        NotifyWidgetForAnalysis(n);
    }
    void spButton::vfunc_0C(const void* n) noexcept
    {
        if(!n){GUIHostForAnalysis().UnrecoveredGUINotificationForAnalysis(*this,n,"Null Button notification");return;}
        try{NotifyButtonForAnalysis(*static_cast<const spGUINotificationForAnalysis*>(n));}
        catch(...){GUIHostForAnalysis().UnrecoveredGUINotificationForAnalysis(*this,n,"Button notification dependency failed");}
    }
}
