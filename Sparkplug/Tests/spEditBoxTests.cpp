#include "Code/Sparkplug/spEditBox.h"
#include "Code/Sparkplug/spTextNode.h"
#include "Code/Sparkplug/spTextRenderable.h"
#include "Code/Sparkplug/spFont.h"
#include "Code/Sparkplug/spFontManager.h"
#include "Code/Sparkplug/spFontSerializer.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/Sparkplug/spCollisionInfo.h"
#include "Code/Sparkplug/spOBBBV.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>

using namespace sparkplug::reconstruction;
namespace
{
    unsigned checks=0,comparisons=0,nativeCases=0;
    void Check(bool value,const char* reason)
    {++checks;if(!value)throw std::runtime_error(reason);}
    void Match(std::uint32_t got,std::uint32_t expected,const char* reason)
    {
        ++comparisons;
        if(got!=expected)std::cerr<<reason<<" actual="<<std::hex<<got<<" expected="<<expected<<std::dec<<'\n';
        Check(got==expected,reason);
    }
    std::uint32_t Bits(float value)
    {std::uint32_t word;std::memcpy(&word,&value,4);return word;}
    float Float(std::uint32_t word)
    {float value;std::memcpy(&value,&word,4);return value;}
    struct Host final : spGUIHost
    {
        std::map<void*,std::uint16_t> references;
        unsigned unresolved=0;
        void AddToEngineManagerForAnalysis(spEntity&) override {}
        void DestroyEntityForAnalysis(spEntity& entity) noexcept override
        {if(auto* node=entity.GetReferenceForAnalysis())--references[node];}
        std::uint16_t GetReferenceCountForAnalysis(void* p) override {return references[p];}
        void SetReferenceCountForAnalysis(void* p,std::uint16_t count) override {references[p]=count;}
        void DeleteReferenceForAnalysis(void*) override {throw std::logic_error("External fixture owner remains live");}
        void SubscribeGUINodeForAnalysis(spNode&,spGUIObject&) override {}
        void UnsubscribeGUINodeForAnalysis(spNode&,spGUIObject&) noexcept override {}
        void DestroyGUIForAnalysis(spGUIObject&) noexcept override {}
        bool IsGUINodeEnabledForAnalysis(const spNode& node) override {return node.IsEnabledForAnalysis();}
        void SetGUINodeEnabledForAnalysis(spNode& node,std::uint8_t value,bool recursive) override
        {node.SetEnabledForAnalysis(value!=0,recursive);}
        void* GetGUINotificationTargetForAnalysis(const spGUIObject&) override {return nullptr;}
        std::uint32_t ReadGUIInputWord18ForAnalysis(std::uintptr_t) override {throw std::logic_error("No input-event binding");}
        void DispatchNotificationForAnalysis(void*,const spNotificationForAnalysis&) override {throw std::logic_error("No owner binding");}
        void UnrecoveredGUINotificationForAnalysis(spGUIObject&,const void*,const char*) noexcept override {++unresolved;}
    };
    struct Reader
    {
        std::ifstream input;
        explicit Reader(const char* path):input(path,std::ios::binary) {Check(bool(input),"Open EditBox fixture");}
        std::uint32_t U32()
        {std::uint32_t value=0;input.read(reinterpret_cast<char*>(&value),4);Check(bool(input),"Fixture word");return value;}
        std::string Blob()
        {auto size=U32();Check(size<=65536,"Fixture blob bound");std::string value(size,0);if(size)input.read(value.data(),size);Check(bool(input),"Fixture blob extent");return value;}
        std::optional<std::string> Optional()
        {auto present=U32();auto value=Blob();Check(present<=1,"Optional flag");return present?std::optional<std::string>(std::move(value)):std::nullopt;}
    };
    void MatchText(const char* actual,const std::optional<std::string>& expected,const char* reason)
    {
        ++comparisons;Check(bool(actual)==expected.has_value(),reason);
        if(expected){++comparisons;Check(*expected==actual,reason);}
    }
    std::shared_ptr<spFont> ReadFont(const std::string& bytes,spFontManager& manager)
    {
        spSerializerManager serializers;spResourceManager resources;
        spSerializerReadContextForAnalysis context(serializers,resources);context.fontManager=&manager;
        auto font=std::make_shared<spFont>();spMemoryStream stream;std::string error;
        Check(stream.ResizeAndSetSize(static_cast<unsigned>(bytes.size())),"Font stream allocation");
        std::memcpy(stream.GetBuffer(),bytes.data(),bytes.size());Check(stream.Seek(spStream::SeekSource::essStart,0),"Font stream rewind");
        Check(spFontSerializer{}.ReadPayloadForAnalysis(context,stream,static_cast<unsigned>(bytes.size()),*font,&error),error.c_str());
        manager.SetLayoutDefaultFontForAnalysis(font.get());return font;
    }
    void MatchLayout(const spTextRenderable& text,Reader& in)
    {
        Match(text.GetWrapWidthForAnalysis(),in.U32(),"Text wrap");Match(text.GetAlignmentForAnalysis(),in.U32(),"Text alignment");
        Match(text.GetMeasuredWidthForAnalysis(),in.U32(),"Text measured width");
        for(auto value:text.GetBoundingSphereForAnalysis())Match(Bits(value),in.U32(),"Text sphere");
        Check(text.GetMinimumForAnalysis().has_value()&&text.GetMaximumForAnalysis().has_value(),"Established Text bounds");
        for(auto value:*text.GetMinimumForAnalysis())Match(Bits(value),in.U32(),"Text minimum");
        for(auto value:*text.GetMaximumForAnalysis())Match(Bits(value),in.U32(),"Text maximum");
    }
    void MatchState(const spEditBox& object,Reader& in,const std::array<std::shared_ptr<spTextNode>,4>& nodes={})
    {
        MatchText(object.GetInitialTextForAnalysis(),in.Optional(),"Initial text");MatchText(object.GetTextForAnalysis(),in.Optional(),"Current text");
        Match(object.GetTextCapacityForAnalysis(),in.U32(),"Current allocated capacity");Match(object.GetTextLengthForAnalysis(),in.U32(),"Current logical length");
        Match(Bits(object.GetMaximumTextExtentForAnalysis()),in.U32(),"Maximum extent bits");
        Match(object.GetOverrideByteForAnalysis(),in.U32(),"Override raw byte");
        Match(static_cast<std::uint8_t>(object.GetPasswordCharacterForAnalysis()[0]),in.U32(),"Password byte");
        Match(static_cast<std::uint8_t>(object.GetPasswordCharacterForAnalysis()[1]),in.U32(),"Password terminator");
        auto role=[&](spTextNode* pointer)
        {for(unsigned i=0;i<nodes.size();++i)if(nodes[i].get()==pointer)return i;throw std::logic_error("Unexpected borrowed TextNode");};
        for(auto* node:object.GetTextStateNodesForAnalysis())Match(role(node),in.U32(),"Borrowed text state");
        Match(role(object.GetCurrentTextNodeForAnalysis()),in.U32(),"Borrowed current TextNode");
        Match(object.GetCursorNodeForAnalysis()!=nullptr,in.U32(),"Borrowed cursor presence");
        Match(object.GetSelectionIndexForAnalysis(),in.U32(),"Selection index");
        Match(object.GetEnabledByteForAnalysis(),in.U32(),"Enabled raw byte");Match(object.GetHighlightByteForAnalysis(),in.U32(),"Highlight raw byte");
    }
    void Basic()
    {
        Host host;auto root=std::make_shared<spNode>();spGUIObject::SetFactoryHostForAnalysis(&host);(void)spEditBox::StaticRTTI();
        auto created=spRTTIManager::Instance().Create(spEditBox::ClassID);auto* object=dynamic_cast<spEditBox*>(created.get());
        Check(object&&object->IsKindOf(spWidget::ClassID),"EditBox actual portable factory/type chain");
        Check(!object->GetTextForAnalysis()&&!object->GetInitialTextForAnalysis()&&!object->GetTextCapacityForAnalysis(),"Factory strings/capacity");
        Check(object->SetTextForAnalysis(nullptr)&&object->GetTextCapacityForAnalysis()==2&&std::string(object->GetTextForAnalysis()).empty(),"Null/null creates empty2-byte buffer and updates");
        Check(object->SetTextForAnalysis("")&&object->GetTextCapacityForAnalysis()==2,"Fresh empty current buffer minimum2");
        object->SetInitialTextForAnalysis("");Check(!object->GetInitialTextForAnalysis(),"Empty initial is null");
        object->SetInitialTextForAnalysis("AB");bool refused=false;
        try{object->SetInitialTextForAnalysis(object->GetInitialTextForAnalysis());}catch(const std::logic_error&){refused=true;}
        Check(refused,"Initial setter released self-alias explicitly refuses");
        spGUINotificationForAnalysis notification{};notification.code=0x1c;object->vfunc_0C(&notification);
        Check(host.unresolved==0,"Own1C null-root path returns without inherited GUI side effects");
        host.references[root.get()]=4;object->SetReferenceForAnalysis(root.get());
        object->vfunc_0C(&notification);
        Check(host.unresolved==1&&!object->GetCurrentTextNodeForAnalysis(),"Missing typed NORMAL_TEXT refuses before unqualified cold creation");
        std::string error;Check(!object->AppendCharacterForAnalysis('A',&error)&&std::string(object->GetTextForAnalysis())=="A","Append retains completed prefix before missing current-node refusal");
        spGUIObject::SetFactoryHostForAnalysis(nullptr);
    }
    void ReplayBare(Reader& in,const std::string& fontBytes)
    {
        const auto count=in.U32();Check(count<=64,"Bare case limit");
        for(unsigned index=0;index<count;++index)
        {
            const auto label=in.Blob();const auto operation=in.U32();
            try
            {
                Host host;
                if(operation==0)
                {
                    const auto nativeFlag=in.U32();Check(nativeFlag<=1,"Native setter flag profile");
                    spFontManager manager;auto font=ReadFont(fontBytes,manager);spTextRenderable text;char input[]{'A','B',0};std::string error;
                    Check(text.SetTextForAnalysis(input,&manager,&error),error.c_str());
                    MatchText(text.GetTextForAnalysis()->c_str(),in.Optional(),"Native flag-path owned text");input[0]='Z';
                    MatchText(text.GetTextForAnalysis()->c_str(),in.Optional(),"Native flag-path input mutation isolation");
                    Match(text.GetColorForAnalysis(),in.U32(),"Text default colour");MatchLayout(text,in);
                    Check(!text.GetFontForAnalysis(),"Default Font is borrowed by manager, not retained by Text");
                }
                else if(operation==1)
                {
                    spEditBox object(host);const auto stages=in.U32();Check(stages<=32,"Buffer stage bound");
                    for(unsigned stage=0;stage<stages;++stage)
                    {
                        const auto name=in.Blob();std::string error;
                        if(name=="initial-AB")object.SetInitialTextForAnalysis("AB");
                        else if(name=="initial-replace")object.SetInitialTextForAnalysis("Z");
                        else if(name=="current-AB")Check(object.SetTextForAnalysis("AB",&error),error.c_str());
                        else if(name=="current-long")Check(object.SetTextForAnalysis("ABABABAB",&error),error.c_str());
                        else if(name=="current-short")Check(object.SetTextForAnalysis("A",&error),error.c_str());
                        else if(name=="current-empty")Check(object.SetTextForAnalysis("",&error),error.c_str());
                        else if(name=="password")object.SetPasswordCharacterForAnalysis("*bb");
                        else if(name=="raw-properties"){object.SetOverrideByteForAnalysis(2);object.SetMaximumTextExtentForAnalysis(Float(0x42c80000));}
                        else Check(name=="factory","Known native buffer stage");
                        MatchState(object,in);
                    }
                }
                else
                {
                    Check(operation==2,"Known bare operation");spEditBox source(host),destination(host);std::string error;
                    source.SetInitialTextForAnalysis("AB");Check(source.SetTextForAnalysis("ABAB",&error),error.c_str());
                    destination.SetInitialTextForAnalysis("Z");Check(destination.SetTextForAnalysis("Y",&error),error.c_str());
                    source.SetMaximumTextExtentForAnalysis(Float(0x42c80000));source.SetOverrideByteForAnalysis(2);source.SetPasswordCharacterForAnalysis("*");
                    destination.SetMaximumTextExtentForAnalysis(Float(0x42000000));destination.SetOverrideByteForAnalysis(3);destination.SetPasswordCharacterForAnalysis("!");
                    MatchState(destination,in);spCloneManager copyManager;Check(source.vfunc_14(destination,copyManager),"Native distinct Copy return");MatchState(destination,in);
                    spCloneManager cloneManager;auto cloneBase=source.vfunc_10(cloneManager);auto* clone=dynamic_cast<spEditBox*>(cloneBase.get());Check(clone!=nullptr,"Native populated concrete Clone");MatchState(*clone,in);
                    Match(source.GetTextForAnalysis()!=destination.GetTextForAnalysis()&&source.GetTextForAnalysis()!=clone->GetTextForAnalysis()&&destination.GetTextForAnalysis()!=clone->GetTextForAnalysis(),in.U32(),"Independent current buffers");
                    Match(source.GetInitialTextForAnalysis()!=destination.GetInitialTextForAnalysis()&&source.GetInitialTextForAnalysis()!=clone->GetInitialTextForAnalysis()&&destination.GetInitialTextForAnalysis()!=clone->GetInitialTextForAnalysis(),in.U32(),"Independent initial strings");
                    Match(cloneManager.FindClone(source)==clone,in.U32(),"Clone mapping pair");
                }
                Check(host.unresolved==0,"Bare operation had no suppressed analytical refusal");++nativeCases;
            }
            catch(const std::exception& error){throw std::runtime_error(label+": "+error.what());}
        }
    }
    void ReplayText(Reader& in,const std::string& fontBytes)
    {
        const auto count=in.U32();Check(count<=128,"Typed case limit");
        for(unsigned index=0;index<count;++index)
        {
            const auto label=in.Blob();
            try
            {
                const auto operation=in.U32();const auto initial=in.Blob(),value=in.Blob();
                std::array<unsigned,3> states{},cached{};for(auto& v:states)v=in.U32();for(auto& v:cached)v=in.U32();
                const auto alignment=in.U32(),maximum=in.U32(),enabled=in.U32(),highlight=in.U32(),cursorPresent=in.U32(),password=in.U32();
                const auto preGrow=in.Blob();const auto code=in.U32(),payload=in.U32();
                Host host;spFontManager manager;auto font=ReadFont(fontBytes,manager);
                std::array<std::shared_ptr<spNode>,4> gui{};std::array<std::shared_ptr<spTextNode>,4> nodes{};
                std::array<std::shared_ptr<spTextRenderable>,4> texts{};std::string error;
                for(unsigned i=1;i<=3;++i)
                {
                    gui[i]=std::make_shared<spNode>();nodes[i]=std::make_shared<spTextNode>();
                    nodes[i]->SetPositionForAnalysis({float(10*i),float(2*i),float(3*i)});
                    if(!cached[i-1])continue;
                    texts[i]=std::make_shared<spTextRenderable>();Check(texts[i]->SetFontForAnalysis(font,&manager,&error),error.c_str());
                    Check(texts[i]->SetAlignmentForAnalysis(alignment,&manager,&error),error.c_str());
                    Check(texts[i]->SetTextForAnalysis("AB",&manager,&error),error.c_str());
                    Check(nodes[i]->AttachRenderableForAnalysis(texts[i]),"Actual shared cached Text attachment");
                }
                auto cursor=cursorPresent?std::make_shared<spNode>():nullptr;if(cursor)cursor->SetPositionForAnalysis({99,8,77});
                spEditBox object(host);object.SetSelectionStateForAnalysis(1,1,0,nullptr,{gui[1].get(),gui[2].get(),gui[3].get()},0);
                object.SetTextGraphForFixture({nodes.at(states[0]).get(),nodes.at(states[1]).get(),nodes.at(states[2]).get()},nodes.at(states[0]).get(),cursor.get());
                object.SetInitialTextForAnalysis("Q");if(!preGrow.empty())Check(object.SetTextForAnalysis(preGrow.c_str(),&error),error.c_str());
                Check(object.SetTextForAnalysis(initial.c_str(),&error),error.c_str());object.SetMaximumTextExtentForAnalysis(Float(maximum));
                object.SetSelectionStateForAnalysis(static_cast<std::uint8_t>(enabled),1,static_cast<std::uint8_t>(highlight),nullptr,{gui[1].get(),gui[2].get(),gui[3].get()},0);
                const char passwordBytes[]{static_cast<char>(password),0};object.SetPasswordCharacterForAnalysis(passwordBytes);
                if(operation==0)Check(object.SetTextForAnalysis(value.c_str(),&error),error.c_str());
                else if(operation==1){Check(!value.empty(),"Append input byte");Check(object.AppendCharacterForAnalysis(static_cast<std::uint8_t>(value[0]),&error),error.c_str());}
                else if(operation==2)Check(object.DeleteCharacterForAnalysis(&error),error.c_str());
                else {Check(operation==3,"Typed operation");spGUINotificationForAnalysis n{};n.code=code;n.payload=payload;object.NotifyEditBoxForAnalysis(n);}
                MatchState(object,in,nodes);
                auto* selected=object.GetSelectedNodeForAnalysis();unsigned selectedRole=0;for(unsigned i=1;i<=3;++i)if(selected==gui[i].get())selectedRole=i;
                Check(selectedRole||!selected,"Known selected GUI node");Match(selectedRole,in.U32(),"Selected GUI node role");
                for(unsigned i=1;i<=3;++i)Match(gui[i]->IsEnabledForAnalysis(),in.U32(),"GUI node enable bit");
                for(unsigned i=1;i<=3;++i)
                {
                    Match(texts[i]!=nullptr,in.U32(),"Cached Text presence");if(!texts[i])continue;
                    MatchText(texts[i]->GetTextForAnalysis()?texts[i]->GetTextForAnalysis()->c_str():nullptr,in.Optional(),"Actual target text");MatchLayout(*texts[i],in);
                }
                for(unsigned i=1;i<=3;++i)
                {
                    for(auto f:nodes[i]->GetPositionForAnalysis())Match(Bits(f),in.U32(),"TextNode position");
                    Match(nodes[i]->GetFlagsForAnalysis(),in.U32(),"TextNode flags");
                    for(auto f:nodes[i]->GetLocalBoundingSphereForAnalysis())Match(Bits(f),in.U32(),"TextNode local sphere");
                    for(auto f:nodes[i]->GetWorldBoundingSphereForAnalysis())Match(Bits(f),in.U32(),"TextNode world sphere");
                }
                Match(cursor!=nullptr,in.U32(),"Cursor fixture presence");
                if(cursor){for(auto f:cursor->GetPositionForAnalysis())Match(Bits(f),in.U32(),"Cursor position");Match(cursor->GetFlagsForAnalysis(),in.U32(),"Cursor flags");}
                Check(host.unresolved==0,"No suppressed typed analytical refusal");++nativeCases;
            }
            catch(const std::exception& error){throw std::runtime_error(label+": "+error.what());}
        }
    }
    void ReplayEdges(Reader& in,const std::string& fontBytes)
    {
        const auto count=in.U32();Check(count<=32,"Edge case limit");
        for(unsigned caseIndex=0;caseIndex<count;++caseIndex)
        {
            const auto label=in.Blob();
            try
            {
                const auto operation=in.U32();const auto initial=in.Blob(),value=in.Blob();
                std::array<unsigned,3> states{},cached{};for(auto& v:states)v=in.U32();for(auto& v:cached)v=in.U32();
                const auto alignment=in.U32(),maximum=in.U32(),enabled=in.U32(),highlight=in.U32(),cursorPresent=in.U32(),password=in.U32();
                const auto preGrow=in.Blob();const auto initialNull=in.U32(),nullText=in.U32(),positionX=in.U32(),scaleX=in.U32(),skipSeed=in.U32(),corruptCache=in.U32();
                Host host;spFontManager manager;auto font=ReadFont(fontBytes,manager);
                std::array<std::shared_ptr<spNode>,4> gui{};std::array<std::shared_ptr<spTextNode>,4> nodes{};
                std::array<std::shared_ptr<spTextRenderable>,4> texts{};std::string error;
                for(unsigned i=1;i<=3;++i)
                {
                    gui[i]=std::make_shared<spNode>();nodes[i]=std::make_shared<spTextNode>();
                    nodes[i]->SetPositionForAnalysis({Float(positionX),float(2*i),float(3*i)});
                    nodes[i]->SetScaleForAnalysis({Float(scaleX),1,1});
                    if(!cached[i-1])continue;
                    texts[i]=std::make_shared<spTextRenderable>();Check(texts[i]->SetFontForAnalysis(font,&manager,&error),error.c_str());
                    Check(texts[i]->SetAlignmentForAnalysis(alignment,&manager,&error),error.c_str());
                    if(!nullText)Check(texts[i]->SetTextForAnalysis("AB",&manager,&error),error.c_str());
                    Check(nodes[i]->AttachRenderableForAnalysis(texts[i]),"Edge actual cached Text attachment");
                }
                auto cursor=cursorPresent?std::make_shared<spNode>():nullptr;if(cursor)cursor->SetPositionForAnalysis({99,8,77});
                spEditBox object(host);object.SetSelectionStateForAnalysis(1,1,0,nullptr,{gui[1].get(),gui[2].get(),gui[3].get()},0);
                object.SetTextGraphForFixture({nodes.at(states[0]).get(),nodes.at(states[1]).get(),nodes.at(states[2]).get()},nodes.at(states[0]).get(),cursor.get());
                object.SetInitialTextForAnalysis("Q");if(!preGrow.empty())Check(object.SetTextForAnalysis(preGrow.c_str(),&error),error.c_str());
                if(!skipSeed)Check(object.SetTextForAnalysis(initialNull?nullptr:initial.c_str(),&error),error.c_str());
                object.SetMaximumTextExtentForAnalysis(Float(maximum));
                object.SetSelectionStateForAnalysis(static_cast<std::uint8_t>(enabled),1,static_cast<std::uint8_t>(highlight),nullptr,{gui[1].get(),gui[2].get(),gui[3].get()},0);
                const char passwordBytes[]{static_cast<char>(password),0};object.SetPasswordCharacterForAnalysis(passwordBytes);
                if(corruptCache){Check(texts[1]!=nullptr,"Known stale cache target");Check(texts[1]->SetTextForAnalysis("Z",&manager,&error),error.c_str());}
                if(operation==0)Check(object.UpdateCursorForAnalysis(&error),error.c_str());
                else if(operation==1){Check(!value.empty(),"Edge append byte");Check(object.AppendCharacterForAnalysis(static_cast<std::uint8_t>(value[0]),&error),error.c_str());}
                else if(operation==2)Check(object.SetTextForAnalysis(nullptr,&error),error.c_str());
                else {Check(operation==3,"Known equality operation");Check(object.SetTextForAnalysis(object.GetTextForAnalysis(),&error),error.c_str());}
                MatchState(object,in,nodes);Match(object.GetSelectedNodeForAnalysis()!=nullptr,in.U32(),"Edge no selected GUI node");
                for(unsigned i=1;i<=3;++i)Match(gui[i]->IsEnabledForAnalysis(),in.U32(),"Edge GUI node enable bit");
                for(unsigned i=1;i<=3;++i)
                {
                    Match(texts[i]!=nullptr,in.U32(),"Edge cached Text presence");if(!texts[i])continue;
                    MatchText(texts[i]->GetTextForAnalysis()?texts[i]->GetTextForAnalysis()->c_str():nullptr,in.Optional(),"Edge actual cached string presence/bytes");
                    Match(texts[i]->GetWrapWidthForAnalysis(),in.U32(),"Edge Text wrap");Match(texts[i]->GetAlignmentForAnalysis(),in.U32(),"Edge Text alignment");
                    Match(texts[i]->GetMeasuredWidthForAnalysis(),in.U32(),"Edge Text measured width");
                    for(auto f:texts[i]->GetBoundingSphereForAnalysis())Match(Bits(f),in.U32(),"Edge Text sphere");
                    // Native null-string factory AABB is unassigned; omitted
                    // rather than normalized or compared with host sentinels.
                }
                for(unsigned i=1;i<=3;++i)
                {
                    for(auto f:nodes[i]->GetPositionForAnalysis())Match(Bits(f),in.U32(),"Edge TextNode position");
                    Match(nodes[i]->GetFlagsForAnalysis(),in.U32(),"Edge TextNode flags");
                    for(auto f:nodes[i]->GetLocalBoundingSphereForAnalysis())Match(Bits(f),in.U32(),"Edge TextNode local sphere");
                    for(auto f:nodes[i]->GetWorldBoundingSphereForAnalysis())Match(Bits(f),in.U32(),"Edge TextNode world sphere");
                }
                Match(cursor!=nullptr,in.U32(),"Edge cursor presence");if(cursor)
                {for(auto f:cursor->GetPositionForAnalysis())Match(Bits(f),in.U32(),"Edge cursor position bits");Match(cursor->GetFlagsForAnalysis(),in.U32(),"Edge cursor flags");}
                Check(host.unresolved==0,"No suppressed edge analytical refusal");++nativeCases;
            }
            catch(const std::exception& error){throw std::runtime_error(label+": "+error.what());}
        }
    }
    void MatchBindingLayout(const spTextRenderable& text,Reader& in,bool maximumAssigned)
    {
        Match(text.GetWrapWidthForAnalysis(),in.U32(),"Binding Text wrap");Match(text.GetAlignmentForAnalysis(),in.U32(),"Binding Text alignment");
        Match(text.GetMeasuredWidthForAnalysis(),in.U32(),"Binding Text measured width");
        for(auto value:text.GetBoundingSphereForAnalysis())Match(Bits(value),in.U32(),"Binding Text sphere");
        Check(text.GetMinimumForAnalysis().has_value(),"Binding Text minimum assigned");
        for(auto value:*text.GetMinimumForAnalysis())Match(Bits(value),in.U32(),"Binding Text minimum");
        Match(text.GetMaximumForAnalysis().has_value(),maximumAssigned,"Binding Text maximum assignment state");
        std::array<std::uint32_t,3> expected{};for(auto& value:expected)value=in.U32();
        if(maximumAssigned)
        {for(unsigned i=0;i<3;++i)Match(Bits((*text.GetMaximumForAnalysis())[i]),expected[i],"Binding Text known maximum");}
        else
        {
            // Original fresh empty Text writes minimum twice and leaves
            // maximum unassigned. CC is allocator input, never a float value
            // to synthesize into the portable optional bound.
            for(auto value:expected)Check(value==0xCCCCCCCC,"Native empty Text maximum remains allocator data");
        }
    }
    void ReplayBinding(Reader& in,const std::string& fontBytes)
    {
        const auto count=in.U32();Check(count<=64,"Binding case bound");
        for(unsigned row=0;row<count;++row)
        {
            const auto label=in.Blob();
            try
            {
                const auto alignment=in.U32();spNode::Vector3 scale{};for(auto& value:scale)value=Float(in.U32());
                const auto enabled=in.U32(),highlight=in.U32(),visible=in.U32();const auto initial=in.Optional();
                const auto overrideByte=in.U32();const auto normalText=in.Blob();std::array<unsigned,3> cached{};for(auto& value:cached)value=in.U32();
                const auto caretPresent=in.U32(),repeats=in.U32(),rootPresent=in.U32(),steps=in.U32();
                Check(repeats>=1&&repeats<=2&&steps==repeats&&rootPresent<=1,"Binding sequence input");
                Host host;spFontManager manager;auto font=ReadFont(fontBytes,manager);std::string error;
                std::array<std::shared_ptr<spNode>,9> all{};std::array<std::shared_ptr<spTextNode>,4> nodes{};
                std::array<std::shared_ptr<spTextRenderable>,4> texts{};
                constexpr std::array<const char*,9> names{nullptr,"ui","NORMAL","DISABLED","HIGHLIGHTED","NORMAL_TEXT","DISABLED_TEXT","HIGHLIGHTED_TEXT","CARET"};
                for(unsigned i=1;i<=8;++i)
                {
                    if(i==8&&!caretPresent)continue;
                    if(i>=5&&i<=7){nodes[i-4]=std::make_shared<spTextNode>();all[i]=nodes[i-4];}
                    else all[i]=std::make_shared<spNode>();
                    all[i]->SetName(names[i]);
                    if(i>=5&&i<=7)
                    {
                        const unsigned role=i-4;all[i]->SetPositionForAnalysis({float(10*role),float(2*role),float(3*role)});
                        if(role==1)all[i]->SetScaleForAnalysis(scale);
                        if(!cached[role-1])continue;
                        texts[role]=std::make_shared<spTextRenderable>();Check(texts[role]->SetFontForAnalysis(font,&manager,&error),error.c_str());
                        Check(texts[role]->SetAlignmentForAnalysis(role==1?alignment:2,&manager,&error),error.c_str());
                        Check(texts[role]->SetTextForAnalysis(role==1?normalText.c_str():role==2?"A":"B",&manager,&error),error.c_str());
                        Check(nodes[role]->AttachRenderableForAnalysis(texts[role]),"Binding actual cached Text attachment");
                    }
                    else if(i==8)all[i]->SetPositionForAnalysis({99,8,77});
                }
                constexpr std::array<unsigned,9> parents{0,0,1,1,1,2,3,4,1};
                for(unsigned i=2;i<=8;++i)if(all[i])Check(all[parents[i]]->AttachChildForAnalysis(all[i]),"Binding existing stable child graph");
                std::array<std::vector<std::shared_ptr<spCollisionInfo>>,3> collisions;
                std::array<std::vector<std::shared_ptr<spOBBBV>>,3> primitives;
                for(unsigned role=2;role<=4;++role)for(unsigned index=0;index<2;++index)
                {
                    auto info=std::make_shared<spCollisionInfo>();auto primitive=std::make_shared<spOBBBV>();info->SetPrimitiveForAnalysis(primitive);
                    info->SetGroupForAnalysis(0x12340000+role*16+index);Check(all[role]->AttachCollisionForAnalysis(info),"Binding actual CollisionInfo attachment");
                    collisions[role-2].push_back(info);primitives[role-2].push_back(primitive);
                }
                spEditBox object(host);
                if(rootPresent){host.references[all[1].get()]=4;object.SetReferenceForAnalysis(all[1].get());}
                object.SetSelectionStateForAnalysis(static_cast<std::uint8_t>(enabled),static_cast<std::uint8_t>(visible),static_cast<std::uint8_t>(highlight),nullptr,{},0);
                object.SetOverrideByteForAnalysis(static_cast<std::uint8_t>(overrideByte));if(initial)object.SetInitialTextForAnalysis(initial->c_str());
                auto role=[&](const spNode* value){for(unsigned i=0;i<all.size();++i)if(all[i].get()==value)return i;throw std::logic_error("Known binding Node role");};
                auto ownership=[&]()
                {
                    std::vector<long> result;for(const auto& node:all)result.push_back(node.use_count());for(const auto& text:texts)result.push_back(text.use_count());
                    for(const auto& list:collisions)for(const auto& value:list)result.push_back(value.use_count());for(const auto& list:primitives)for(const auto& value:list)result.push_back(value.use_count());return result;
                };
                auto links=[&]()
                {
                    std::vector<std::uintptr_t> result;for(const auto& node:all)if(node)
                    {
                        result.push_back(reinterpret_cast<std::uintptr_t>(node->GetParentForAnalysis()));result.push_back(node->GetChildCountForAnalysis());
                        for(unsigned i=0;i<node->GetChildCountForAnalysis();++i)result.push_back(reinterpret_cast<std::uintptr_t>(node->GetChildForAnalysis(i)));
                        for(const auto byte:std::string(node->GetName()))result.push_back(static_cast<unsigned char>(byte));result.push_back(0);
                    }
                    return result;
                };
                const auto references=ownership();const auto beforeLinks=links();const auto hostReferences=host.references;
                spGUINotificationForAnalysis notification{};notification.code=0x1c;
                for(unsigned step=0;step<steps;++step)
                {
                    object.NotifyEditBoxForAnalysis(notification);Check(!host.unresolved,"Binding whole typed source notification");MatchState(object,in,nodes);
                    Match(role(object.GetNodeForGUIForAnalysis()),in.U32(),"Binding root role");Match(role(object.GetSelectedNodeForAnalysis()),in.U32(),"Binding selected GUI role");
                    for(auto* node:object.GetStateNodesForAnalysis())Match(role(node),in.U32(),"Binding GUI state role");
                    Match(role(object.GetCurrentTextNodeForAnalysis()),in.U32(),"Binding current TextNode role");Match(role(object.GetCursorNodeForAnalysis()),in.U32(),"Binding CARET role");
                    const auto nodeCount=in.U32();Check(nodeCount<=8,"Binding Node count");unsigned actualCount=0;for(const auto& node:all)if(node)++actualCount;Match(actualCount,nodeCount,"Binding whole Node set");
                    for(unsigned i=0;i<nodeCount;++i)
                    {
                        const auto id=in.U32();Check(id>=1&&id<=8&&all[id],"Binding known Node output");
                        for(auto value:all[id]->GetPositionForAnalysis())Match(Bits(value),in.U32(),"Binding Node position bits");Match(all[id]->GetFlagsForAnalysis(),in.U32(),"Binding Node flags");
                    }
                    for(const auto& list:collisions)
                    {const auto groupCount=in.U32();Match(static_cast<unsigned>(list.size()),groupCount,"Binding collision count");for(const auto& info:list)Match(info->GetGroupForAnalysis(),in.U32(),"Binding actual collision group");}
                    for(unsigned i=1;i<=3;++i)
                    {
                        Match(texts[i]!=nullptr,in.U32(),"Binding cached Text presence");if(!texts[i])continue;
                        MatchText(texts[i]->GetTextForAnalysis()?texts[i]->GetTextForAnalysis()->c_str():nullptr,in.Optional(),"Binding own current text propagation");
                        const bool maximumAssigned=i!=1||!normalText.empty()||(rootPresent&&initial&&!initial->empty());
                        MatchBindingLayout(*texts[i],in,maximumAssigned);
                    }
                }
                Match(ownership()==references&&host.references==hostReferences,in.U32(),"Binding input ownership retained");Match(links()==beforeLinks,in.U32(),"Binding stable names and links retained");
                Match(notification.code==0x1c&&notification.word04==0&&notification.word08==0&&notification.word0C==0&&!notification.sender&&notification.word14==0&&notification.payload==0&&notification.tail==0,in.U32(),"Binding original logical input frame retained");
                ++nativeCases;
            }
            catch(const std::exception& error){throw std::runtime_error(label+": "+error.what());}
        }
    }
    void Replay(const char* path)
    {
        Reader in(path);const auto magic=in.U32();const auto fontBytes=in.Blob();
        if(magic==0x31504445)ReplayBare(in,fontBytes);
        else if(magic==0x314e4445)ReplayEdges(in,fontBytes);
        else if(magic==0x31494245)ReplayBinding(in,fontBytes);
        else {Check(magic==0x31424445,"EDB1/EDP1 fixture");ReplayText(in,fontBytes);}
        Check(in.input.peek()==std::char_traits<char>::eof(),"Complete EditBox fixture read");
    }
}
int main(int argc,char** argv)
{
    try
    {
        Basic();for(int i=1;i<argc;++i)Replay(argv[i]);
        std::cout<<"PASS EditBox bounded strings/Copy/Clone/text/cursor/selection/append/Notify; nativeCases="<<nativeCases<<" scalarComparisons="<<comparisons<<" checks="<<checks<<'\n';return 0;
    }
    catch(const std::exception& error){std::cerr<<"FAIL EditBox "<<error.what()<<'\n';return 1;}
}
