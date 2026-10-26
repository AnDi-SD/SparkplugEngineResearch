#include "spWidget.h"
#include "spNode.h"
#include "spCollisionInfo.h"
#include <stdexcept>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateWidget()
        {
            auto* host=spGUIObject::GetFactoryHostForAnalysis();
            if(!host)throw std::logic_error("spWidget requires an explicit GUI host");
            return std::make_unique<spWidget>(*host);
        }
        const spRTTIRecord record{spWidget::ClassID,spGUIObject::ClassID,
            "spWidget",&spGUIObject::StaticRTTI(),&CreateWidget,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    const spRTTIRecord& spWidget::StaticRTTI() noexcept { (void)registered;return record; }
    const spRTTIRecord& spWidget::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> spWidget::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spWidget>(GUIHostForAnalysis());
        manager.RegisterCloneForAnalysis(*this,*clone);
        if(!vfunc_14(*clone,manager))return nullptr;
        return clone;
    }
    bool spWidget::vfunc_14(spBaseObject& destination,spCloneManager& manager) const
    {
        auto* widget=dynamic_cast<spWidget*>(&destination);
        if(!widget||!spGUIObject::vfunc_14(destination,manager))return false;
        widget->highlight_=highlight_;widget->selected_=selected_;
        widget->nodes_=nodes_;widget->index_=index_;
        // Native copy does not assign visible. Nodes are shallow borrowed pointers.
        return true;
    }
    void spWidget::SetEnabledForAnalysis(std::uint8_t value)
    { StoreEnabledByteForAnalysis(value);RefreshSelectionForAnalysis(); }
    void spWidget::SetVisibleForAnalysis(std::uint8_t value)
    { visible_=value;RefreshSelectionForAnalysis(); }
    void spWidget::SetSelectionStateForAnalysis(std::uint8_t enabled,std::uint8_t visible,
        std::uint8_t highlight,spNode* selected,
        std::array<spNode*,3> nodes,std::uint32_t index) noexcept
    { StoreEnabledByteForAnalysis(enabled);visible_=visible;highlight_=highlight;selected_=selected;nodes_=nodes;index_=index; }
    bool spWidget::HidePreviousSelectionForAnalysis()
    {
        if(selected_)
        {
            if(!GUIHostForAnalysis().IsGUINodeEnabledForAnalysis(*selected_))return false;
            GUIHostForAnalysis().SetGUINodeEnabledForAnalysis(*selected_,0,true);
        }
        return true;
    }
    void spWidget::SelectAndShowForAnalysis(std::uint32_t index,spNode* node)
    {
        index_=index;selected_=node?node:nodes_[0];
        if(selected_)GUIHostForAnalysis().SetGUINodeEnabledForAnalysis(*selected_,visible_,true);
    }
    void spWidget::RefreshSelectionForAnalysis()
    {
        if(!HidePreviousSelectionForAnalysis())return;
        const std::uint32_t index=!GetEnabledByteForAnalysis()?1:highlight_?2:0;
        SelectAndShowForAnalysis(index,nodes_[index]);
    }
    void spWidget::NotifyWidgetForAnalysis(const spGUINotificationForAnalysis& n)
    {
        if(n.code==0x1c)
        {
            if(auto* root=GetNodeForGUIForAnalysis())
            {
                nodes_[0]=root->FindNodeForAnalysis("NORMAL",true,true);
                nodes_[1]=GetNodeForGUIForAnalysis()->FindNodeForAnalysis("DISABLED",true,true);
                nodes_[2]=GetNodeForGUIForAnalysis()->FindNodeForAnalysis("HIGHLIGHTED",true,true);
                for(std::size_t i=0;nodes_[0]&&i<nodes_[0]->GetCollisionCountForAnalysis();++i)
                    nodes_[0]->GetCollisionForAnalysis(i)->SetGroupForAnalysis(4);
                if(nodes_[1])GUIHostForAnalysis().SetGUINodeEnabledForAnalysis(*nodes_[1],0,true);
                if(nodes_[2])
                {
                    GUIHostForAnalysis().SetGUINodeEnabledForAnalysis(*nodes_[2],0,true);
                    for(std::size_t i=0;i<nodes_[2]->GetCollisionCountForAnalysis();++i)
                        nodes_[2]->GetCollisionForAnalysis(i)->SetGroupForAnalysis(4);
                }
                selected_=nodes_[0];
            }
            NotifyBaseForAnalysis(n);
            return;
        }
        bool forward=false;
        if(n.code==0x11||n.code==0x12)
        {
            if(GetEnabledByteForAnalysis())
            {
                highlight_=n.code==0x11?1:0;
                RefreshSelectionForAnalysis();
                forward=true;
            }
        }
        else if(n.code==0x0e||n.code==0x0f||n.code==0x10)
            forward=GetEnabledByteForAnalysis()!=0;
        if(forward)
        {
            // Original reloads+24 after Refresh and does not recheck enabled.
            auto* target=GUIHostForAnalysis().GetGUINotificationTargetForAnalysis(*this);
            if(!target)throw std::logic_error("Widget direct owner Notify dereferences a null native target");
            GUIHostForAnalysis().DispatchNotificationForAnalysis(target,n);
        }
        // The owner may have changed the original message. Do not cache code.
        NotifyBaseForAnalysis(n);
    }
    void spWidget::vfunc_0C(const void* n) noexcept
    {
        if(!n){GUIHostForAnalysis().UnrecoveredGUINotificationForAnalysis(*this,n,"Null Widget notification");return;}
        try{NotifyWidgetForAnalysis(*static_cast<const spGUINotificationForAnalysis*>(n));}
        catch(...){GUIHostForAnalysis().UnrecoveredGUINotificationForAnalysis(*this,n,"Widget notification dependency failed");}
    }
}
