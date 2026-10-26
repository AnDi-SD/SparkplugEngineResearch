#include "wxButton.h"
#include "Code/Sparkplug/spNode.h"
#include "Code/SparkBase/spBroadcastNotification.h"
#include <cstring>
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        wxButtonHost* factoryHost=nullptr;
        std::unique_ptr<spBaseObject> CreateButton()
        {
            if(!factoryHost)throw std::logic_error("wxButton requires an explicit GUI host");
            return std::make_unique<wxButton>(*factoryHost);
        }
        const spRTTIRecord record{wxButton::ClassID,spButton::ClassID,
            "wxButton",&spButton::StaticRTTI(),&CreateButton,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);

        void CopyString64(std::uint8_t* destination,const std::uint8_t* source)
        {
            bool ended=false;
            for(std::size_t i=0;i<64;++i)
            {
                const auto value=ended?std::uint8_t{0}:source[i];
                destination[i]=value;
                if(!value)ended=true;
            }
        }
    }
    wxButton::wxButton(wxButtonHost& host,OwnBytesForAnalysis allocationBytes)
        :spButton(host),host_(host),own_(allocationBytes)
    {
        own_[0x144]=1;
        host_.RemoveWxButtonFromEngineManagerForAnalysis(*this);
        host_.RegisterWxButtonForAnalysis(*this);
        std::memset(own_.data(),0,4);
        own_[4]=0;
        for(auto offset:{0x44u,0x84u,0xc4u,0x104u})
            std::memcpy(own_.data()+offset,"empty",6);
    }
    wxButton::~wxButton() { host_.DestroyWxButtonForAnalysis(*this); }
    void wxButton::SetFactoryHostForAnalysis(wxButtonHost* host) noexcept { factoryHost=host; }
    const spRTTIRecord& wxButton::StaticRTTI() noexcept { (void)registered;return record; }
    const spRTTIRecord& wxButton::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> wxButton::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<wxButton>(host_);
        manager.RegisterCloneForAnalysis(*this,*clone);
        if(!vfunc_14(*clone,manager))return nullptr;
        return clone;
    }
    bool wxButton::vfunc_14(spBaseObject& destination,spCloneManager& manager) const
    {
        auto* button=dynamic_cast<wxButton*>(&destination);
        if(!button||!spButton::vfunc_14(*button,manager))return false;
        std::memcpy(button->own_.data(),own_.data(),4);
        // Original calls strncpy in this order, each with count64. No original
        // callback lies between these five bounded CRT operations.
        for(auto offset:{4u,0xc4u,0x104u,0x44u,0x84u})
            CopyString64(button->own_.data()+offset,own_.data()+offset);
        // Native copy omits refresh gate1A4 and its following padding.
        host_.CompleteWxButtonCopyForAnalysis(ClassID);
        return true;
    }
    void wxButton::StoreHighlightForAnalysis(std::uint8_t value) noexcept
    {
        SetSelectionStateForAnalysis(GetEnabledByteForAnalysis(),GetVisibleByteForAnalysis(),value,
            GetSelectedNodeForAnalysis(),GetStateNodesForAnalysis(),GetSelectionIndexForAnalysis());
    }
    void wxButton::StoreSelectedForAnalysis(spNode* node) noexcept
    { StoreSelectionForAnalysis(GetSelectionIndexForAnalysis(),node); }
    void wxButton::StoreSelectionForAnalysis(std::uint32_t index,spNode* node) noexcept
    {
        SetSelectionStateForAnalysis(GetEnabledByteForAnalysis(),GetVisibleByteForAnalysis(),GetHighlightByteForAnalysis(),
            node,GetStateNodesForAnalysis(),index);
    }
    void wxButton::SetStateForAnalysis(std::uint32_t value)
    {
        if(value==0){StoreEnabledByteForAnalysis(1);StoreHighlightForAnalysis(0);}
        else if(value==1)StoreEnabledByteForAnalysis(0);
        else if(value==2){StoreEnabledByteForAnalysis(1);StoreHighlightForAnalysis(1);}
        RefreshSelectionForAnalysis();
    }
    void wxButton::RefreshSelectionForAnalysis()
    {
        if(!GetRefreshGateByteForAnalysis())return;
        if(auto* previous=GetSelectedNodeForAnalysis())
            GUIHostForAnalysis().SetGUINodeEnabledForAnalysis(*previous,0,true);
        if(GetPressedByteForAnalysis()&&GetPressedNodeForAnalysis())
            StoreSelectedForAnalysis(GetPressedNodeForAnalysis());
        else
        {
            const std::uint32_t index=!GetEnabledByteForAnalysis()?1:GetHighlightByteForAnalysis()?2:0;
            StoreSelectionForAnalysis(index,GetStateNodesForAnalysis()[index]);
        }
        if(!GetSelectedNodeForAnalysis())StoreSelectedForAnalysis(GetStateNodesForAnalysis()[0]);
        for(std::size_t i=0;i<3;++i)
        {
            auto* node=GetStateNodesForAnalysis()[i];
            if(node&&node!=GetSelectedNodeForAnalysis())
                GUIHostForAnalysis().SetGUINodeEnabledForAnalysis(*node,0,true);
        }
        if(!GetPressedByteForAnalysis())
            if(auto* pushed=GetPressedNodeForAnalysis())
                GUIHostForAnalysis().SetGUINodeEnabledForAnalysis(*pushed,0,true);
        auto* selected=GetSelectedNodeForAnalysis();
        if(!selected)return;
        auto* root=GetNodeForGUIForAnalysis();
        if(!root)throw std::logic_error("wxButton selection dereferences a null native root");
        if(root->IsEnabledForAnalysis())
            GUIHostForAnalysis().SetGUINodeEnabledForAnalysis(*selected,GetVisibleByteForAnalysis(),true);
    }
    void wxButton::vfunc_0C(const void* notification) noexcept
    {
        if(notification)
        {
            const auto& incoming=*static_cast<const spGUINotificationForAnalysis*>(notification);
            if(incoming.code==0x1c&&incoming.payload==1)
            {
                try
                {
                    const void* sender=this;
                    const std::uint32_t code=0x2715,key=3;
                    const std::uintptr_t payload=0,tail=0;
                    const auto result=SendBroadcastNotificationForAnalysis(sender,code,key,payload,tail);
                    if(result!=spBroadcastNotificationResultForAnalysis::Completed)
                    {
                        GUIHostForAnalysis().UnrecoveredGUINotificationForAnalysis(*this,notification,
                            "wxButton1C/mode1 requires a qualified existing SubscriptionManager");
                        return;
                    }
                    // The actual original frame is forwarded after the broadcast.
                    // An application callback may have changed its code/payload.
                    // Never recheck the own1C/mode1 admission or copy the frame.
                    spButton::vfunc_0C(notification);
                }
                catch(...)
                {
                    GUIHostForAnalysis().UnrecoveredGUINotificationForAnalysis(*this,notification,
                        "wxButton1C/mode1 broadcast dependency failed");
                }
                return;
            }
        }
        GUIHostForAnalysis().UnrecoveredGUINotificationForAnalysis(*this,notification,
            "wxButton own Notify code/mode is outside the qualified subset");
    }
}
