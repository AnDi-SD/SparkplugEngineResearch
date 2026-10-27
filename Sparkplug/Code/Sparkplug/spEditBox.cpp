#include "spEditBox.h"
#include "spGUIManager.h"
#include "spTextNode.h"
#include "spTextRenderable.h"
#include "spFont.h"
#include "spFontManager.h"
#include "spCollisionInfo.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateEditBox()
        {
            auto* host=spGUIObject::GetFactoryHostForAnalysis();
            if(!host)throw std::logic_error("EditBox requires an explicit GUI host");
            return std::make_unique<spEditBox>(*host);
        }
        const spRTTIRecord record{spEditBox::ClassID,spWidget::ClassID,
            "spEditBox",&spWidget::StaticRTTI(),&CreateEditBox,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        bool Fail(std::string* error,const char* reason)
        {if(error)*error=reason;return false;}
        std::size_t BoundedLength(const char* value)
        {
            // Stable NUL-terminated input is required. This portable extent
            // limit is an analytical refusal, not a native string-size rule.
            const auto length=std::strlen(value);
            if(length>65535)throw std::logic_error("EditBox analytical string limit");
            return length;
        }
        bool Measure(spTextNode& node,const char* value,std::uint32_t& width,std::string* error)
        {
            // Original Font462C00 returns0 before reading this for null/empty.
            if(!value||!*value){width=0;return true;}
            if(std::strlen(value)>65535)return Fail(error,"EditBox direct measurement analytical string limit");
            auto* text=node.GetTextRenderableForAnalysis();
            auto* font=text?text->GetFontForAnalysis().get():nullptr;
            if(!font)return Fail(error,"EditBox direct measure requires the cached Text's bound Font");
            width=font->MeasureTextForAnalysis(value,0);return true;
        }
    }
    const spRTTIRecord& spEditBox::StaticRTTI() noexcept {(void)registered;return record;}
    const spRTTIRecord& spEditBox::vfunc_18() const noexcept {return record;}
    std::unique_ptr<spBaseObject> spEditBox::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spEditBox>(GUIHostForAnalysis());
        manager.RegisterCloneForAnalysis(*this,*clone);
        return vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
    bool spEditBox::vfunc_14(spBaseObject& destination,spCloneManager& manager) const
    {
        auto* target=dynamic_cast<spEditBox*>(&destination);
        if(!target||!spWidget::vfunc_14(destination,manager))return false;
        target->textNodes_=textNodes_;target->currentNode_=currentNode_;target->cursor_=cursor_;
        target->SetInitialTextForAnalysis(GetInitialTextForAnalysis());
        if(!target->SetTextForAnalysis(GetTextForAnalysis()))return false;
        target->maximum_=maximum_;
        // Native Copy does not assign override7C or password7D/7E.
        return true;
    }
    const char* spEditBox::GetInitialTextForAnalysis() const noexcept
    {return initial_?initial_->c_str():nullptr;}
    void spEditBox::SetInitialTextForAnalysis(const char* value)
    {
        if(initial_&&value)
        {
            const auto address=reinterpret_cast<std::uintptr_t>(value);
            const auto begin=reinterpret_cast<std::uintptr_t>(initial_->c_str());
            if(address>=begin&&address<=begin+initial_->size())
                throw std::logic_error("EditBox initial setter reads an already released self-alias in the native path");
        }
        // Native frees78 before inspecting/replacing its input. In particular
        // an empty initial string is NULL, unlike the current empty buffer.
        initial_.reset();
        if(value&&*value){const auto length=BoundedLength(value);initial_.emplace(value,length);}
    }
    const char* spEditBox::GetTextForAnalysis() const noexcept
    {return buffer_.empty()?nullptr:buffer_.data();}
    std::uint32_t spEditBox::GetTextCapacityForAnalysis() const noexcept
    {return static_cast<std::uint32_t>(buffer_.size());}
    bool spEditBox::SetTextForAnalysis(const char* value,std::string* error)
    {
        if(error)error->clear();
        // PC468C9B ->468DEE skips copying for equal nonnull pointers, but
        // still updates TextNodes. Equal null pointers create the empty2-byte
        // buffer before that update (468DF2..468E25).
        if(value==GetTextForAnalysis())
        {
            if(buffer_.empty()){buffer_.resize(2);buffer_[0]=0;length_=0;}
            return UpdateTextForAnalysis(error);
        }
        const auto length=value&&*value?BoundedLength(value):0;
        const auto required=length?length+1:std::size_t(2);
        if(buffer_.size()<required)buffer_.resize(required);
        if(length)std::memmove(buffer_.data(),value,length+1);
        else buffer_[0]=0;
        length_=static_cast<std::uint32_t>(length);
        return UpdateTextForAnalysis(error);
    }
    void spEditBox::SetPasswordCharacterForAnalysis(const char* value) noexcept
    {password_[0]=value?*value:0;password_[1]=0;}
    void spEditBox::SetTextGraphForFixture(std::array<spTextNode*,3> nodes,
        spTextNode* current,spNode* cursor) noexcept
    {textNodes_=nodes;currentNode_=current;cursor_=cursor;}
    bool spEditBox::UpdateTextForAnalysis(std::string* error)
    {
        if(error)error->clear();
        // Native visits NORMAL, HIGHLIGHTED, DISABLED, retaining duplicates.
        for(const auto index:std::array<std::size_t,3>{0,2,1})
        {
            auto* node=textNodes_[index];
            auto* text=node?node->GetTextRenderableForAnalysis():nullptr;
            if(!text)continue;
            // Both original flag0 and wrapper flag1 paths create/reuse owning
            // copies and perform layout in the qualified domain.
            if(!text->SetTextForAnalysis(GetTextForAnalysis(),spFontManager::GetInstance(),error))return false;
            node->RebuildRenderableBoundsForAnalysis();
        }
        if(GetHighlightByteForAnalysis()&&GetEnabledByteForAnalysis())return UpdateCursorForAnalysis(error);
        return true;
    }
    bool spEditBox::UpdateCursorForAnalysis(std::string* error)
    {
        if(error)error->clear();
        if(!cursor_||!currentNode_||!GetHighlightByteForAnalysis()||!GetEnabledByteForAnalysis())return true;
        const auto& currentPosition=currentNode_->GetPositionForAnalysis();
        auto position=spNode::Vector3{currentPosition[0],cursor_->GetPositionForAnalysis()[1],currentPosition[2]};
        auto* text=currentNode_->GetTextRenderableForAnalysis();
        std::uint32_t width=0;
        const auto* value=text&&text->GetTextForAnalysis()?text->GetTextForAnalysis()->c_str():nullptr;
        float scaled=0;
        // At most65535 bytes * uint8 glyph advance255 gives width<2^24;
        // its product with a float's24 significant bits is exact in double.
        // Native spills scaled width to float before adding to the node X.
        // A missing cached Text/string skips the multiply and retains +0;
        // an existing empty string still multiplies its measured zero.
        if(text&&value)
        {
            if(!Measure(*currentNode_,value,width,error))return false;
            scaled=static_cast<float>(double(width)*double(currentNode_->GetScaleForAnalysis()[0]));
        }
        const auto alignment=text?text->GetAlignmentForAnalysis():0;
        if(alignment==0)position[0]=static_cast<float>(double(position[0])+double(scaled));
        else if(alignment==1)position[0]=static_cast<float>(double(position[0])+double(scaled)*0.5);
        cursor_->SetPositionForAnalysis(position);cursor_->MarkLocalTransformDirtyForAnalysis();return true;
    }
    bool spEditBox::DeleteCharacterForAnalysis(std::string* error)
    {
        if(error)error->clear();
        if(!length_)return true;
        if(buffer_.size()<=length_)return Fail(error,"EditBox length/capacity input graph is invalid");
        buffer_[--length_]=0;return UpdateTextForAnalysis(error);
    }
    bool spEditBox::AppendCharacterForAnalysis(std::uint8_t value,std::string* error)
    {
        if(error)error->clear();
        const char byte=password_[0]?password_[0]:static_cast<char>(value);
        if(std::uint64_t(length_)+1<buffer_.size())
        {
            buffer_[length_]=byte;buffer_[++length_]=0;
            if(!UpdateTextForAnalysis(error))return false;
        }
        else
        {
            if(length_>255){buffer_[255]=0;length_=255;}
            std::vector<char> temporary(std::size_t(length_)+2,0);
            if(GetTextForAnalysis())std::memcpy(temporary.data(),GetTextForAnalysis(),std::strlen(GetTextForAnalysis())+1);
            temporary[length_]=byte;temporary[std::size_t(length_)+1]=0;
            if(!SetTextForAnalysis(temporary.data(),error))return false;
        }
        // The original dereferences current60 here even with no cursor or
        // highlight. Keep completed buffer/text effects before this refusal.
        if(!currentNode_)return Fail(error,"EditBox append requires a live current TextNode");
        std::uint32_t width=0;
        if(!Measure(*currentNode_,GetTextForAnalysis(),width,error))return false;
        const double scaled=double(width)*double(currentNode_->GetScaleForAnalysis()[0]);
        if(maximum_>0&&scaled>double(maximum_)&&length_)
        {buffer_[--length_]=0;return UpdateTextForAnalysis(error);}
        return true;
    }
    void spEditBox::RefreshSelectionForAnalysis()
    {
        spWidget::RefreshSelectionForAnalysis();
        if(currentNode_)GUIHostForAnalysis().SetGUINodeEnabledForAnalysis(*currentNode_,0,true);
        const auto index=GetSelectionIndexForAnalysis();
        if(index>=textNodes_.size())throw std::logic_error("EditBox selection index is outside its established graph");
        currentNode_=textNodes_[index];
        if(currentNode_)GUIHostForAnalysis().SetGUINodeEnabledForAnalysis(*currentNode_,1,true);
        if(GetHighlightByteForAnalysis()&&GetEnabledByteForAnalysis())
        {
            if(cursor_)GUIHostForAnalysis().SetGUINodeEnabledForAnalysis(*cursor_,1,true);
            std::string error;if(!UpdateCursorForAnalysis(&error))throw std::logic_error(error);
        }
        else if(cursor_)GUIHostForAnalysis().SetGUINodeEnabledForAnalysis(*cursor_,0,true);
    }
    void spEditBox::BindTextNodesForAnalysis()
    {
        if(!GetNodeForGUIForAnalysis())return;
        auto lookup=[this](const char* name)
        {
            auto* root=GetNodeForGUIForAnalysis();
            if(!root)throw std::logic_error("EditBox binding requires its current live root");
            return root->FindNodeForAnalysis(name,true,true);
        };
        auto storeState=[this](std::size_t index,spNode* node)
        {
            auto states=GetStateNodesForAnalysis();states[index]=node;
            SetSelectionStateForAnalysis(GetEnabledByteForAnalysis(),GetVisibleByteForAnalysis(),
                GetHighlightByteForAnalysis(),GetSelectedNodeForAnalysis(),states,GetSelectionIndexForAnalysis());
        };
        for(std::size_t i=0;i<3;++i)
            storeState(i,lookup(std::array<const char*,3>{"NORMAL","DISABLED","HIGHLIGHTED"}[i]));
        auto groups=[this](std::size_t role)
        {
            for(std::size_t i=0;;++i)
            {
                auto* node=GetStateNodesForAnalysis()[role];
                if(!node||i>=node->GetCollisionCountForAnalysis())break;
                auto* collision=node->GetCollisionForAnalysis(i);
                if(!collision)throw std::logic_error("EditBox binding requires live collision entries");
                collision->SetGroupForAnalysis(4);
            }
        };
        groups(0);
        if(auto* node=GetStateNodesForAnalysis()[1])GUIHostForAnalysis().SetGUINodeEnabledForAnalysis(*node,0,true);
        if(auto* node=GetStateNodesForAnalysis()[2])GUIHostForAnalysis().SetGUINodeEnabledForAnalysis(*node,0,true);
        groups(2);
        SetSelectionStateForAnalysis(GetEnabledByteForAnalysis(),GetVisibleByteForAnalysis(),GetHighlightByteForAnalysis(),
            GetStateNodesForAnalysis()[0],GetStateNodesForAnalysis(),GetSelectionIndexForAnalysis());
        auto* normal=dynamic_cast<spTextNode*>(lookup("NORMAL_TEXT"));
        // Native now creates/names/attaches a new TextNode on a miss or wrong
        // kind. Preserve the completed GUI prefix before this open producer.
        if(!normal)throw std::logic_error("EditBox cold NORMAL_TEXT creation is not qualified");
        textNodes_[0]=normal;
        auto* cached=normal->GetTextRenderableForAnalysis();
        const char* value=cached&&cached->GetTextForAnalysis()?cached->GetTextForAnalysis()->c_str():nullptr;
        std::uint32_t width=0;std::string error;
        if(!Measure(*normal,value,width,&error))throw std::logic_error(error);
        const auto alignment=cached?cached->GetAlignmentForAnalysis():0;
        const double offset=alignment==1?-double(width)*0.5:alignment==2?-double(width):0.0;
        const auto scale=normal->GetScaleForAnalysis();
        const spNode::Vector3 shift{static_cast<float>(offset*double(scale[0])),
            static_cast<float>(double(scale[1])*0.0),static_cast<float>(double(scale[2])*0.0)};
        const auto position=normal->GetPositionForAnalysis();
        // PC4365E0 adds a supplied vector and marks Node local state dirty.
        // This is an observed primitive call, not an attributed Editor helper.
        normal->SetPositionForAnalysis({static_cast<float>(double(position[0])+double(shift[0])),
            static_cast<float>(double(position[1])+double(shift[1])),static_cast<float>(double(position[2])+double(shift[2]))});
        normal->MarkLocalTransformDirtyForAnalysis();
        auto resetAlignment=[&](spTextNode& node)
        {
            if(auto* text=node.GetTextRenderableForAnalysis())
                if(!text->SetAlignmentForAnalysis(0,spFontManager::GetInstance(),&error))throw std::logic_error(error);
        };
        resetAlignment(*textNodes_[0]);
        for(std::size_t i=1;i<3;++i)
        {
            auto* node=dynamic_cast<spTextNode*>(lookup(i==1?"DISABLED_TEXT":"HIGHLIGHTED_TEXT"));
            if(!node)continue;
            textNodes_[i]=node;resetAlignment(*node);
            if(!textNodes_[0])throw std::logic_error("EditBox binding requires a live normal TextNode");
            const auto origin=textNodes_[0]->GetPositionForAnalysis();
            node->SetPositionForAnalysis(origin);node->MarkLocalTransformDirtyForAnalysis();
        }
        if(textNodes_[1])GUIHostForAnalysis().SetGUINodeEnabledForAnalysis(*textNodes_[1],0,true);
        if(textNodes_[2])GUIHostForAnalysis().SetGUINodeEnabledForAnalysis(*textNodes_[2],0,true);
        currentNode_=textNodes_[0];cursor_=lookup("CARET");
        if(cursor_)GUIHostForAnalysis().SetGUINodeEnabledForAnalysis(*cursor_,0,true);
        value=GetInitialTextForAnalysis();
        if(!value&&!override_)
        {
            cached=textNodes_[0]?textNodes_[0]->GetTextRenderableForAnalysis():nullptr;
            value=cached&&cached->GetTextForAnalysis()?cached->GetTextForAnalysis()->c_str():nullptr;
        }
        if(!SetTextForAnalysis(value,&error))throw std::logic_error(error);
    }
    void spEditBox::NotifyEditBoxForAnalysis(const spGUINotificationForAnalysis& n)
    {
        if(n.code==0x1c){BindTextNodesForAnalysis();return;}
        if(n.code==0x0f)
        {
            if(!GetEnabledByteForAnalysis())return;
            auto* manager=spGUIManager::GetInstanceForAnalysis();
            if(!manager)throw std::logic_error("EditBox focus entry requires a live GUIManager singleton");
            if(manager->SetFocusForAnalysis(this)!=spGUIFocusResultForAnalysis::Completed)
                throw std::logic_error("EditBox focus entry reached the analytical nesting limit");
            // Focus callbacks may change enabled, owner24 or the original
            // frame. Reload the owner, forward that frame, and do not gate
            // again on enabled or route its mutated code through Widget.
            auto* owner=GUIHostForAnalysis().GetGUINotificationTargetForAnalysis(*this);
            if(!owner)throw std::logic_error("EditBox focus entry requires a live notification target");
            GUIHostForAnalysis().DispatchNotificationForAnalysis(owner,n);
            return;
        }
        if(n.code==0x0e||n.code==0x10||n.code==0x11||n.code==0x12)
        {
            if(GetEnabledByteForAnalysis())
            {
                auto* owner=GUIHostForAnalysis().GetGUINotificationTargetForAnalysis(*this);
                if(!owner)throw std::logic_error("EditBox owner Notify requires a live target");
                GUIHostForAnalysis().DispatchNotificationForAnalysis(owner,n);
            }
            return; // Own routes do not perform Widget highlight/base handling.
        }
        if(n.code==0x13)
        {
            if(GetEnabledByteForAnalysis())
            {std::string error;if(!AppendCharacterForAnalysis(static_cast<std::uint8_t>(n.payload),&error))throw std::logic_error(error);}
            return;
        }
        if(n.code==0x14)
        {
            if(!GetEnabledByteForAnalysis())return;
            if(n.payload==0x0d)
            {std::string error;if(!DeleteCharacterForAnalysis(&error))throw std::logic_error(error);}
            else if(n.payload==0x1b||n.payload==0x54)
            {
                auto* manager=spGUIManager::GetInstanceForAnalysis();
                if(!manager)throw std::logic_error("EditBox key requires a live GUIManager singleton");
                if(manager->GetFocusForAnalysis()==this&&
                    manager->SetFocusForAnalysis(nullptr)!=spGUIFocusResultForAnalysis::Completed)
                    throw std::logic_error("EditBox focus clearing reached the analytical nesting limit");
            }
        }
        else if(n.code==0x16||n.code==0x17)
        {
            if(!GetEnabledByteForAnalysis())return;
            SetSelectionStateForAnalysis(GetEnabledByteForAnalysis(),GetVisibleByteForAnalysis(),
                n.code==0x16?1:0,GetSelectedNodeForAnalysis(),GetStateNodesForAnalysis(),GetSelectionIndexForAnalysis());
            RefreshSelectionForAnalysis();
        }
        NotifyWidgetForAnalysis(n);
    }
    void spEditBox::vfunc_0C(const void* notification) noexcept
    {
        if(!notification){GUIHostForAnalysis().UnrecoveredGUINotificationForAnalysis(*this,notification,"Null EditBox notification");return;}
        try{NotifyEditBoxForAnalysis(*static_cast<const spGUINotificationForAnalysis*>(notification));}
        catch(...){GUIHostForAnalysis().UnrecoveredGUINotificationForAnalysis(*this,notification,"EditBox notification dependency is unqualified");}
    }
}
