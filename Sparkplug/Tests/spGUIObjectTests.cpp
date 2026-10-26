#include "Code/Sparkplug/spButton.h"
#include "Code/Sparkplug/spNode.h"
#include "Code/Sparkplug/spCollisionInfo.h"
#include "Code/Sparkplug/spOBBBV.h"
#include "Analysis/PC/spGUIAbi.h"
#include "Analysis/PS2/spGUIAbi.h"
#include <array>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace sparkplug::reconstruction;
namespace
{
    unsigned checks=0,nativeChecks=0,originalOnlyGroupWriteEvents=0,originalOnlyGroupWriteScalars=0;
    void Check(bool value,const char* reason)
    { ++checks;if(!value)throw std::runtime_error(reason); }
    std::vector<std::uint32_t> Groups(const spNode& node)
    {
        std::vector<std::uint32_t> result;
        for(std::size_t i=0;i<node.GetCollisionCountForAnalysis();++i)result.push_back(node.GetCollisionForAnalysis(i)->GetGroupForAnalysis());
        return result;
    }
    void SetGroups(spNode& node,const std::vector<std::uint32_t>& groups)
    {
        while(node.GetCollisionCountForAnalysis())(void)node.DetachCollisionForAnalysis(*node.GetCollisionForAnalysis(0));
        for(auto group:groups)
        {
            auto info=std::make_shared<spCollisionInfo>();info->SetGroupForAnalysis(group);
            info->SetPrimitiveForAnalysis(std::make_shared<spOBBBV>());
            Check(node.AttachCollisionForAnalysis(std::move(info)),"Typed Node CollisionInfo fixture attachment");
        }
    }
    struct Host final:spGUIHost
    {
        std::map<void*,std::uint16_t> references;
        std::map<const spNode*,unsigned> ids;
        std::vector<std::array<unsigned,3>> enables;
        std::vector<std::string> events;
        unsigned unresolved=0,deleted=0;
        std::map<const spGUIObject*,void*> targets;
        std::function<void()> beforeNodeEnable;
        std::function<void(void*,const spNotificationForAnalysis&)> beforeNotification;
        std::function<void(spNode&,spGUIObject&,bool)> beforeSubscription;
        void AddToEngineManagerForAnalysis(spEntity&) override { events.emplace_back("add"); }
        void DestroyEntityForAnalysis(spEntity& entity) noexcept override
        {
            events.emplace_back("entity-destroy");
            if(auto* reference=entity.GetReferenceForAnalysis())--references[reference];
        }
        std::uint16_t GetReferenceCountForAnalysis(void* p) override { return references[p]; }
        void SetReferenceCountForAnalysis(void* p,std::uint16_t n) override
        { references[p]=n;events.emplace_back("reference:"+std::to_string(n)); }
        void DeleteReferenceForAnalysis(void*) override { ++deleted;events.emplace_back("delete"); }
        void SubscribeGUINodeForAnalysis(spNode& n,spGUIObject& object) override
        { if(beforeSubscription)beforeSubscription(n,object,true);events.emplace_back("subscribe:"+std::to_string(ids[&n])); }
        void UnsubscribeGUINodeForAnalysis(spNode& n,spGUIObject& object) noexcept override
        { if(beforeSubscription)beforeSubscription(n,object,false);events.emplace_back("unsubscribe:"+std::to_string(ids[&n])); }
        void DestroyGUIForAnalysis(spGUIObject&) noexcept override { events.emplace_back("gui-destroy"); }
        bool IsGUINodeEnabledForAnalysis(const spNode& n) override { return n.IsEnabledForAnalysis(); }
        void SetGUINodeEnabledForAnalysis(spNode& n,std::uint8_t v,bool recursive) override
        { enables.push_back({ids[&n],v,recursive?1u:0u});if(beforeNodeEnable)beforeNodeEnable();n.SetEnabledForAnalysis(v!=0,recursive); }
        void* GetGUINotificationTargetForAnalysis(const spGUIObject& object) override { return targets[&object]; }
        std::uint32_t ReadGUIInputWord18ForAnalysis(std::uintptr_t event) override { return *reinterpret_cast<const std::uint32_t*>(event); }
        void DispatchNotificationForAnalysis(void* target,const spNotificationForAnalysis& n) override
        { if(beforeNotification)beforeNotification(target,n);static_cast<spBaseObject*>(target)->vfunc_0C(&n); }
        void UnrecoveredGUINotificationForAnalysis(spGUIObject&,const void*,const char*) noexcept override { ++unresolved; }
    };
    struct Reader
    {
        std::ifstream input;
        explicit Reader(const char* path):input(path,std::ios::binary) { Check(bool(input),"Open original GUI fixture"); }
        std::uint32_t U32()
        { std::uint32_t n=0;input.read(reinterpret_cast<char*>(&n),4);Check(bool(input),"GUI fixture uint32 extent");return n; }
        std::string Text()
        { auto n=U32();Check(n<=4096,"GUI fixture text bound");std::string s(n,'\0');if(n)input.read(s.data(),n);Check(bool(input),"GUI fixture text extent");return s; }
    };
    void Match(unsigned a,unsigned b,const char* reason)
    {
        if(a!=b)std::cerr<<reason<<": actual "<<a<<" expected "<<b<<'\n';
        ++nativeChecks;Check(a==b,reason);
    }
    void Basics()
    {
        Host host;spGUIObject::SetFactoryHostForAnalysis(&host);
        for(auto id:{spGUIObject::ClassID,spWidget::ClassID,spButton::ClassID})
        {
            auto object=spRTTIManager::Instance().Create(id);
            auto* gui=dynamic_cast<spGUIObject*>(object.get());
            Check(gui&&gui->GetEnabledByteForAnalysis()==1&&gui->GetField20ForAnalysis()==1,
                "GUI factory passes Entity type1 and preserves enabled byte");
            Check(gui->GetNodeForGUIForAnalysis()==nullptr&&gui->GetSavedCollisionGroupsForAnalysis().empty(),"GUI own defaults");
            Check(gui->IsKindOf(spEntity::ClassID)&&gui->IsKindOf(spNamedObject::ClassID),"GUI physical and registered ancestry");
            auto clone=object->Clone();Check(clone&&clone->IsExactly(id),"GUI concrete clone type");
            spGUIObject* result=nullptr;spGUINotificationForAnalysis n;n.code=0x18;n.payload=reinterpret_cast<std::uintptr_t>(&result);
            object->vfunc_0C(&n);Check(result==gui,"GUI notification18 virtual output");
        }
        spEntity base(host);Check(base.GetField20ForAnalysis()==3,"Existing Entity factory constructor domain remains3");
        spWidget widget(host);spGUINotificationForAnalysis unresolved;unresolved.code=0x1c;
        widget.vfunc_0C(&unresolved);Check(host.unresolved==1,"Null Node dependency is an observed host boundary");
        spGUIObject cold(host);bool guarded=false;
        try{cold.SetEnabledForAnalysis(255);}catch(const std::logic_error&){guarded=true;}
        Check(guarded&&cold.GetEnabledByteForAnalysis()==255,"Null native dependency guard preserves preceding raw store");
        spGUIObject::SetFactoryHostForAnalysis(nullptr);
    }
    void HistoryAndTeardown()
    {
        Host host;spNode a,b;host.ids[&a]=1;host.ids[&b]=2;
        host.references[&a]=host.references[&b]=2;SetGroups(a,{1,2,3});SetGroups(b,{16,32});
        {
            spGUIObject object(host);object.SetReferenceForAnalysis(&a);
            Check(Groups(a)==std::vector<std::uint32_t>({4,4,4}),"Bind writes collision group4");
            Check(host.references[&a]==3,"Bind uses Entity 16-bit retention");
            object.SetReferenceForAnalysis(&b);
            Check(Groups(a)==std::vector<std::uint32_t>({1,2,3})&&Groups(b)==std::vector<std::uint32_t>({4,4}),"Rebind restores old before changing reference");
            object.SetReferenceForAnalysis(nullptr);
            Check(Groups(b)==std::vector<std::uint32_t>({1,2}),"Native stale-history prefix quirk remains visible");
            Check(object.GetSavedCollisionGroupsForAnalysis()==std::vector<std::uint32_t>({1,2,3,16,32}),"Saved history appends across bindings");
            Check(host.references[&a]==2&&host.references[&b]==2&&host.deleted==0,"Both old references released once");
            object.SetReferenceForAnalysis(&a);const auto count=host.references[&a];
            object.SetReferenceForAnalysis(&a);
            Check(host.references[&a]==count&&object.GetSavedCollisionGroupsForAnalysis().size()==11,"Same-node bind retains count and appends another history");
            host.events.clear();
        }
        Check(host.events.size()>=3&&host.events[0]=="gui-destroy"&&host.events[1]=="unsubscribe:1"&&host.events.back()=="entity-destroy",
            "GUI cleanup precedes unsubscribe and existing Entity teardown");
        Check(Groups(a)==std::vector<std::uint32_t>({4,4,4}),"Destructor does not restore collision groups");
        Check(host.references[&a]==2,"Entity teardown owns final reference release");
        Host enableHost;spNode collision;collision.SetName("prefixGUICollisionSuffix");SetGroups(collision,{1,2});enableHost.references[&collision]=2;
        spGUIObject enable(enableHost);enable.SetReferenceForAnalysis(&collision);
        for(auto raw:{0u,1u,255u})
        {
            SetGroups(collision,{0x12345678,0xffffffff});enable.SetEnabledForAnalysis(static_cast<std::uint8_t>(raw));
            Check(enable.GetEnabledByteForAnalysis()==raw&&Groups(collision)==std::vector<std::uint32_t>({4,4}),"Both GUI enabled branches write literal collision group4");
        }
    }
    void NativeSelections(Reader& r)
    {
        auto count=r.U32();Check(count>0&&count<=128,"Bounded original selection case count");
        for(unsigned caseIndex=0;caseIndex<count;++caseIndex)
        {
            Host host;std::array<spNode,4> nodes;std::array<spNode*,5> ptrs{nullptr,&nodes[0],&nodes[1],&nodes[2],&nodes[3]};
            for(unsigned i=1;i<=4;++i)host.ids[ptrs[i]]=i;
            std::array<unsigned,13> in{};for(auto& value:in)value=r.U32();
            const bool button=in[0]!=0;std::unique_ptr<spWidget> object;
            if(button)object=std::make_unique<spButton>(host);else object=std::make_unique<spWidget>(host);
            object->SetSelectionStateForAnalysis(static_cast<std::uint8_t>(in[1]),static_cast<std::uint8_t>(in[3]),static_cast<std::uint8_t>(in[2]),ptrs.at(in[4]),
                {ptrs.at(in[6]),ptrs.at(in[7]),ptrs.at(in[8])},7);
            if(button)static_cast<spButton*>(object.get())->SetButtonStateForAnalysis("",static_cast<std::uint8_t>(in[9]),ptrs.at(in[10]));
            if(in[4]&&!in[5])ptrs.at(in[4])->SetEnabledForAnalysis(false);
            if(in[11]==1)object->SetEnabledForAnalysis(static_cast<std::uint8_t>(in[12]));
            else if(in[11]==2)object->SetVisibleForAnalysis(static_cast<std::uint8_t>(in[12]));
            else object->RefreshSelectionForAnalysis();
            auto id=[&](spNode* node){for(unsigned i=0;i<ptrs.size();++i)if(ptrs[i]==node)return i;throw std::runtime_error("Unexpected selection pointer");};
            Match(id(object->GetSelectedNodeForAnalysis()),r.U32(),"Original selected Node");
            Match(object->GetSelectionIndexForAnalysis(),r.U32(),"Original selection index including retained pressed index");
            Match(object->GetEnabledByteForAnalysis(),r.U32(),"Original raw enabled byte");
            Match(object->GetVisibleByteForAnalysis(),r.U32(),"Original raw visible byte");
            Match(object->GetHighlightByteForAnalysis(),r.U32(),"Original raw highlight byte");
            Match(button?static_cast<spButton*>(object.get())->GetPressedByteForAnalysis():0,r.U32(),"Original raw pressed byte");
            for(unsigned i=0;i<4;++i)Match(nodes[i].GetFlagsForAnalysis(),r.U32(),"Whole original Node flag word");
            const auto calls=r.U32();Match(static_cast<unsigned>(host.enables.size()),calls,"Original ordered Node-enable call count");
            for(unsigned i=0;i<calls;++i)for(unsigned word=0;word<3;++word)Match(host.enables.at(i)[word],r.U32(),"Original Node-enable raw arguments/order");
        }
    }
    void NativeCopies(Reader& r)
    {
        auto count=r.U32();Check(count>0&&count<=128,"Bounded original copy case count");
        for(unsigned i=0;i<count;++i)
        {
            Host host;std::array<spNode,5> nodes;std::array<spNode*,6> ptrs{nullptr,&nodes[0],&nodes[1],&nodes[2],&nodes[3],&nodes[4]};
            for(unsigned j=1;j<ptrs.size();++j){host.ids[ptrs[j]]=j;host.references[ptrs[j]]=0;}
            host.references[ptrs[5]]=2; // native populated destination holds count3 after retention
            const auto kind=r.U32(),doClone=r.U32();std::array<unsigned,10> in{};for(auto& word:in)word=r.U32();
            const auto text=r.Text(),name=r.Text();
            auto create=[&]()->std::unique_ptr<spGUIObject>{if(kind==2)return std::make_unique<spButton>(host);if(kind==1)return std::make_unique<spWidget>(host);return std::make_unique<spGUIObject>(host);};
            auto source=create();source->SetName(name);source->SetEnabledByteForAnalysis(static_cast<std::uint8_t>(in[0]));
            source->SetSavedCollisionGroupsForAnalysis({17,18,19});
            if(auto* widget=dynamic_cast<spWidget*>(source.get()))widget->SetSelectionStateForAnalysis(static_cast<std::uint8_t>(in[0]),static_cast<std::uint8_t>(in[1]),static_cast<std::uint8_t>(in[2]),ptrs.at(in[3]),
                {ptrs.at(in[4]),ptrs.at(in[5]),ptrs.at(in[6])},in[7]);
            if(auto* button=dynamic_cast<spButton*>(source.get()))button->SetButtonStateForAnalysis(text,static_cast<std::uint8_t>(in[8]),ptrs.at(in[9]));
            auto destination=create();destination->SetReferenceForAnalysis(ptrs[5]);destination->SetSavedCollisionGroupsForAnalysis({91,92});
            if(auto* widget=dynamic_cast<spWidget*>(destination.get()))widget->SetSelectionStateForAnalysis(1,37,0,nullptr,{},0);
            spCloneManager manager;bool result=true;
            if(doClone)
            {
                destination.reset();auto cloned=manager.Clone(*source);
                destination.reset(dynamic_cast<spGUIObject*>(cloned.release()));result=destination!=nullptr;
            }
            else result=source->vfunc_14(*destination,manager);
            Match(result?1:0,r.U32(),"Original copy/clone completion result");
            Check(destination!=nullptr,"Original successful GUI clone has portable result");
            auto id=[&](const spNode* p){for(unsigned j=0;j<ptrs.size();++j)if(ptrs[j]==p)return j;throw std::runtime_error("Unexpected copied Node pointer");};
            auto* widget=dynamic_cast<spWidget*>(destination.get());auto* button=dynamic_cast<spButton*>(destination.get());
            Match(destination->GetEnabledByteForAnalysis(),r.U32(),"Original copied enabled byte");
            Match(widget?widget->GetVisibleByteForAnalysis():0,r.U32(),"Original visible omission in copy");
            Match(widget?widget->GetHighlightByteForAnalysis():0,r.U32(),"Original copied highlight byte");
            Match(widget?id(widget->GetSelectedNodeForAnalysis()):0,r.U32(),"Original shallow selected Node");
            for(unsigned j=0;j<3;++j)Match(widget?id(widget->GetStateNodesForAnalysis()[j]):0,r.U32(),"Original shallow state Node");
            Match(widget?widget->GetSelectionIndexForAnalysis():0,r.U32(),"Original copied index");
            Match(button?button->GetPressedByteForAnalysis():0,r.U32(),"Original copied pressed byte");
            Match(button?id(button->GetPressedNodeForAnalysis()):0,r.U32(),"Original shallow pressed Node");
            Check((button?button->GetStringForAnalysis():std::string{})==r.Text(),"Original deep copied button string");++nativeChecks;
            Check(std::string(destination->GetName()?destination->GetName():"")==r.Text(),"Original shared Named string value");++nativeChecks;
            Match(id(destination->GetNodeForGUIForAnalysis()),r.U32(),"Original Entity reference omission in GUI copy");
            const auto history=r.U32();Match(static_cast<unsigned>(destination->GetSavedCollisionGroupsForAnalysis().size()),history,"Original saved history omission in GUI copy");
            for(unsigned j=0;j<history;++j)Match(destination->GetSavedCollisionGroupsForAnalysis()[j],r.U32(),"Original retained destination history");
            if(button){source.reset();Check(button->GetStringForAnalysis()==text,"Copied string owns independent storage after source destruction");}
        }
    }
    std::vector<std::uint32_t> Words(Reader& r)
    {
        const auto count=r.U32();Check(count<=128,"Original GUI vector extent bound");
        std::vector<std::uint32_t> result(count);for(auto& value:result)value=r.U32();return result;
    }
    void NativeBindings(Reader& r)
    {
        const auto cases=r.U32();Check(cases>0&&cases<=64,"Original GUI binding case count");
        for(unsigned i=0;i<cases;++i)
        {
            Host host;spNode a,b;std::array<spNode*,3> nodes{nullptr,&a,&b};
            host.ids[&a]=1;host.ids[&b]=2;const auto initial=r.U32();
            const auto firstReferences=r.U32(),secondReferences=r.U32();
            auto saved=Words(r);const auto firstFlags=Words(r),secondFlags=Words(r);
            SetGroups(a,firstFlags);SetGroups(b,secondFlags);
            host.references[&a]=static_cast<std::uint16_t>(firstReferences);
            host.references[&b]=static_cast<std::uint16_t>(secondReferences);
            spGUIObject object(host);
            if(initial)
            {
                // Populate the literal initial Entity reference through its
                // real common bind, then restore the declared pre-call data.
                --host.references[nodes.at(initial)];object.SetReferenceForAnalysis(nodes.at(initial));
                SetGroups(a,firstFlags);SetGroups(b,secondFlags);
            }
            object.SetSavedCollisionGroupsForAnalysis(std::move(saved));
            const auto operations=r.U32();Check(operations<=16,"Original binding sequence bound");
            for(unsigned step=0;step<operations;++step)
            {
                const auto target=r.U32();host.events.clear();object.SetReferenceForAnalysis(nodes.at(target));
                auto id=[&](const spNode* node){for(unsigned n=0;n<nodes.size();++n)if(nodes[n]==node)return n;throw std::runtime_error("Unexpected bound Node");};
                Match(id(object.GetNodeForGUIForAnalysis()),r.U32(),"Original bound Node after complete call");
                Match(host.references[&a],r.U32(),"Original first retained Node reference count");
                Match(host.references[&b],r.U32(),"Original second retained Node reference count");
                const auto expectedSaved=Words(r);Check(object.GetSavedCollisionGroupsForAnalysis()==expectedSaved,"Original cumulative saved collision-group history");++nativeChecks;
                Check(Groups(a)==Words(r),"Original first Node collision groups");++nativeChecks;
                Check(Groups(b)==Words(r),"Original second Node collision groups");++nativeChecks;
                std::vector<std::array<unsigned,2>> subscriptions;
                for(const auto& event:host.events)
                {
                    if(event.rfind("unsubscribe:",0)==0)subscriptions.push_back({0,static_cast<unsigned>(std::stoul(event.substr(12)))});
                    else if(event.rfind("subscribe:",0)==0)subscriptions.push_back({1,static_cast<unsigned>(std::stoul(event.substr(10)))});
                }
                // Historical fixture contains native group-store observations.
                // Direct typed SetGroup has no callback between stores. Retain
                // these original-only fields without pretending to observe a
                // current write trace or incrementing nativeChecks for them.
                const auto count=r.U32();Check(count<=128,"Historical original group-store trace bound");
                originalOnlyGroupWriteEvents+=count;originalOnlyGroupWriteScalars+=1+count*3;
                for(unsigned j=0;j<count;++j)for(unsigned k=0;k<3;++k)(void)r.U32();
                const auto events=r.U32();Match(static_cast<unsigned>(subscriptions.size()),events,"Original ordered subscription call count");
                for(unsigned j=0;j<events;++j)for(unsigned k=0;k<2;++k)Match(subscriptions.at(j)[k],r.U32(),"Original unsubscribe/subscribe direction and Node order");
            }
        }
    }
    void NativeNotifications(Reader& r)
    {
        const auto cases=r.U32();Check(cases>0&&cases<=64,"Original Notify fixture case count");
        for(unsigned i=0;i<cases;++i)
        {
            std::array<unsigned,15> in{};for(auto& word:in)word=r.U32();
            Host host;std::array<spNode,4> nodeStorage;std::array<spNode*,5> nodes{nullptr,&nodeStorage[0],&nodeStorage[1],&nodeStorage[2],&nodeStorage[3]};
            for(unsigned j=1;j<nodes.size();++j)host.ids[nodes[j]]=j;
            spGUIObject firstOwner(host),secondOwner(host);std::array<void*,3> owners{nullptr,&firstOwner,&secondOwner};
            std::unique_ptr<spWidget> object=in[0]==1?std::unique_ptr<spWidget>(std::make_unique<spButton>(host)):std::make_unique<spWidget>(host);
            auto* button=dynamic_cast<spButton*>(object.get());object->SetReferenceForAnalysis(nodes[1]);
            object->SetSelectionStateForAnalysis(static_cast<std::uint8_t>(in[2]),static_cast<std::uint8_t>(in[3]),static_cast<std::uint8_t>(in[4]),nodes.at(in[11]),
                {nodes.at(in[7]),nodes.at(in[8]),nodes.at(in[9])},7);
            if(button)button->SetButtonStateForAnalysis("action-caption",static_cast<std::uint8_t>(in[5]),nodes.at(in[10]));
            if(in[11]&&!in[12])nodes.at(in[11])->SetEnabledForAnalysis(false);
            host.targets[object.get()]=owners.at(in[13]);
            std::uint32_t eventWord=in[6];spGUIObject* output=nullptr;
            spGUINotificationForAnalysis notification;notification.code=in[1];notification.word04=0x11111111;
            notification.word08=0x22222222;notification.word0C=0x33333333;notification.sender=object.get();notification.word14=0x44444444;
            notification.payload=button&&in[1]==0xf?reinterpret_cast<std::uintptr_t>(&eventWord):in[1]==0x18?reinterpret_cast<std::uintptr_t>(&output):0x12345678;
            notification.tail=0x76543210;
            bool applied=false;
            auto mutate=[&](bool atNode)
            {
                const auto mutation=in[14];if(applied||!mutation||((mutation==3||mutation==6||mutation==9)!=atNode))return;
                applied=true;
                if(mutation==1||mutation==3||mutation==9)object->SetEnabledByteForAnalysis(0);
                if(mutation==2||mutation==3||mutation==5||mutation==6||mutation==9)host.targets[object.get()]=owners[2];
                if(mutation==2||mutation==3){Check(button!=nullptr,"Button-only callback mutation");button->SetButtonStateForAnalysis("changed",0,nodes[2]);}
                if(mutation==4){notification.code=0x18;notification.payload=reinterpret_cast<std::uintptr_t>(&output);}
                if(mutation==5)
                {
                    const auto selected=object->GetSelectedNodeForAnalysis();const auto stateNodes=object->GetStateNodesForAnalysis();const auto index=object->GetSelectionIndexForAnalysis();
                    object->SetSelectionStateForAnalysis(0,object->GetVisibleByteForAnalysis(),255,selected,stateNodes,index);
                }
                if(mutation==6){Check(button!=nullptr,"Pressed callback mutation");button->SetButtonStateForAnalysis(button->GetStringForAnalysis(),1,button->GetPressedNodeForAnalysis());}
                if(mutation==8)notification.code=0x99;
            };
            auto normalize=[&](const spNotificationForAnalysis& n)
            {
                std::uint32_t payload=static_cast<std::uint32_t>(n.payload);
                if(n.payload==reinterpret_cast<std::uintptr_t>(&eventWord))payload=0xf0000001;
                else if(button&&n.payload==reinterpret_cast<std::uintptr_t>(button->GetStringForAnalysis().c_str()))payload=0xf0000002;
                else if(n.payload==reinterpret_cast<std::uintptr_t>(&output))payload=0xf0000003;
                return std::array<std::uint32_t,8>{n.code,n.word04,n.word08,n.word0C,
                    n.sender==object.get()?0xe0000001u:static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(n.sender)),n.word14,payload,static_cast<std::uint32_t>(n.tail)};
            };
            struct ObservedNotification{unsigned owner,identity;std::array<std::uint32_t,8> words;std::string text;};
            std::vector<ObservedNotification> observed;
            host.beforeNodeEnable=[&]{mutate(true);};
            host.beforeNotification=[&](void* target,const spNotificationForAnalysis& n)
            {
                const auto owner=target==owners[1]?1u:target==owners[2]?2u:0u;Check(owner!=0,"Expected actual owner notification target");
                const bool textPayload=button&&n.payload==reinterpret_cast<std::uintptr_t>(button->GetStringForAnalysis().c_str());
                observed.push_back({owner,&n==&notification?0u:1u,normalize(n),textPayload?button->GetStringForAnalysis():std::string{}});
                mutate(false);
            };
            if(in[0]==2)
            {
                auto* const target=host.targets[object.get()];
                SendDirectedNotificationForAnalysis(host,object.get(),target,in[1],0x10,0);
            }
            else object->vfunc_0C(&notification);
            Check(host.unresolved==0,"Supported whole Notify path has no unresolved host boundary");
            const auto nodeId=[&](spNode* p){for(unsigned j=0;j<nodes.size();++j)if(nodes[j]==p)return j;throw std::runtime_error("Unexpected Notify selected Node");};
            Match(object->GetEnabledByteForAnalysis(),r.U32(),"Whole Notify enabled byte");
            Match(object->GetVisibleByteForAnalysis(),r.U32(),"Whole Notify visible byte");
            Match(object->GetHighlightByteForAnalysis(),r.U32(),"Whole Notify highlight byte");
            Match(button?button->GetPressedByteForAnalysis():0,r.U32(),"Whole Notify pressed byte and callback ordering");
            Match(nodeId(object->GetSelectedNodeForAnalysis()),r.U32(),"Whole Notify selected Node");
            Match(object->GetSelectionIndexForAnalysis(),r.U32(),"Whole Notify retained selection index");
            Match(host.targets[object.get()]==owners[1]?1:host.targets[object.get()]==owners[2]?2:0,r.U32(),"Callback-visible owner reload");
            Match(output==object.get()?99:output==&firstOwner?1:output==&secondOwner?2:0,r.U32(),"Whole Notify base output after input mutation");
            Match(applied?1:0,r.U32(),"Controlled callback mutation reached actual consumer");
            Check((button?button->GetStringForAnalysis():std::string{})==r.Text(),"Callback-visible owned string value");++nativeChecks;
            for(auto& node:nodeStorage)Match(node.GetFlagsForAnalysis(),r.U32(),"Whole Notify original Node flag word");
            const auto calls=r.U32();Match(static_cast<unsigned>(host.enables.size()),calls,"Whole Notify Node call count including double Refresh");
            for(unsigned j=0;j<calls;++j)for(unsigned k=0;k<3;++k)Match(host.enables.at(j)[k],r.U32(),"Whole Notify ordered Node arguments");
            const auto messages=r.U32();Match(static_cast<unsigned>(observed.size()),messages,"Whole Notify owner-message count including19 thenF");
            for(unsigned j=0;j<messages;++j)
            {
                const auto& got=observed.at(j);Match(got.owner,r.U32(),"Whole Notify owner order");Match(got.identity,r.U32(),"Forwarded original message identity versus new helper frame");
                for(auto word:got.words)Match(word,r.U32(),"Original notification eight logical words");
                Check(got.text==r.Text(),"Original synchronously borrowed string payload");++nativeChecks;
            }
            for(auto word:normalize(notification))Match(word,r.U32(),"Original incoming message after callback return");
            host.beforeNotification={};host.beforeNodeEnable={};
        }
    }
    void NativeLookup(Reader& r)
    {
        struct Query{unsigned node;bool recursive,substring;std::string name;};
        struct ObservedNode final:spNode
        {
            unsigned id=0;std::vector<Query>* calls=nullptr;
            spNode* FindNodeForAnalysis(const char* name,bool recursive,bool substring) override
            {
                // Test-only virtual-entry observation. Selection remains in
                // the shared actual spNode body, not a fixture search provider.
                if(calls)calls->push_back({id,recursive,substring,name?name:""});
                return spNode::FindNodeForAnalysis(name,recursive,substring);
            }
        };
        const auto cases=r.U32();Check(cases>0&&cases<=64,"Typed original lookup case count");
        for(unsigned row=0;row<cases;++row)
        {
            std::array<unsigned,9> input{};for(auto& value:input)value=r.U32();const auto query=r.Text();const auto count=r.U32();
            Check(count>0&&count<=16,"Typed original Node graph extent");
            struct Spec{unsigned parent,enabled;std::vector<std::uint32_t> groups;};
            std::vector<Spec> specs(count+1);std::vector<std::shared_ptr<ObservedNode>> nodes(count+1);
            Host host;std::vector<Query> calls;
            for(unsigned i=1;i<=count;++i)
            {
                const auto parent=r.U32(),enabled=r.U32(),hasName=r.U32();const auto name=r.Text();auto groups=Words(r);
                nodes[i]=std::make_shared<ObservedNode>();nodes[i]->id=i;nodes[i]->calls=&calls;host.ids[nodes[i].get()]=i;
                if(hasName)nodes[i]->SetName(name);SetGroups(*nodes[i],groups);specs[i]={parent,enabled,std::move(groups)};
            }
            for(unsigned i=1;i<=count;++i)if(specs[i].parent)
                Check(nodes.at(specs[i].parent)->AttachChildForAnalysis(nodes[i]),"Stable typed child graph input");
            for(unsigned i=1;i<=count;++i)if(!specs[i].enabled)nodes[i]->SetEnabledForAnalysis(false,false);
            const auto id=[&](const spNode* p){if(!p)return 0u;for(unsigned i=1;i<=count;++i)if(nodes[i].get()==p)return i;throw std::runtime_error("Unexpected typed lookup Node");};
            std::unique_ptr<spWidget> object;spButton* button=nullptr;unsigned result=0;
            if(input[0]==2)
            { calls.clear();result=id(nodes[1]->FindNodeForAnalysis(query.c_str(),input[7]!=0,input[8]!=0)); }
            else
            {
                if(input[0]==1)object=std::make_unique<spButton>(host);else object=std::make_unique<spWidget>(host);
                object->SetReferenceForAnalysis(nodes[1].get());
                // Match literal pre-call groups/history after establishing the
                // real common typed Node association in the host facade.
                for(unsigned i=1;i<=count;++i)SetGroups(*nodes[i],specs[i].groups);
                object->SetSavedCollisionGroupsForAnalysis({});
                object->SetSelectionStateForAnalysis(static_cast<std::uint8_t>(input[1]),static_cast<std::uint8_t>(input[2]),static_cast<std::uint8_t>(input[3]),nodes.at(input[6]).get(),{},input[5]);
                button=dynamic_cast<spButton*>(object.get());if(button)button->SetButtonStateForAnalysis("",static_cast<std::uint8_t>(input[4]),nullptr);
                calls.clear();host.enables.clear();spNotificationForAnalysis notification;notification.code=0x1c;object->vfunc_0C(&notification);
                Check(host.unresolved==0,"Whole typed GUI1C completed without unknown dependency");
            }
            Match(result,r.U32(),"Original borrowed direct lookup result");
            Match(object?object->GetEnabledByteForAnalysis():0,r.U32(),"GUI1C enabled byte");
            Match(object?object->GetVisibleByteForAnalysis():0,r.U32(),"GUI1C preserves visible byte");
            Match(object?object->GetHighlightByteForAnalysis():0,r.U32(),"GUI1C preserves highlight byte");
            Match(button?button->GetPressedByteForAnalysis():0,r.U32(),"GUI1C preserves pressed byte");
            Match(object?id(object->GetSelectedNodeForAnalysis()):0,r.U32(),"GUI1C selected NORMAL borrowed Node");
            for(unsigned i=0;i<3;++i)Match(object?id(object->GetStateNodesForAnalysis()[i]):0,r.U32(),"GUI1C exact state name result");
            Match(button?id(button->GetPressedNodeForAnalysis()):0,r.U32(),"GUI1C PUSHED name result");
            Match(object?object->GetSelectionIndexForAnalysis():0,r.U32(),"GUI1C preserves previous selection index");
            for(unsigned i=1;i<=count;++i)Match(nodes[i]->GetFlagsForAnalysis(),r.U32(),"Whole typed Node flags after recursive GUI effects");
            for(unsigned i=1;i<=count;++i){Check(Groups(*nodes[i])==Words(r),"Actual typed CollisionInfo groups after lookup protocol");++nativeChecks;}
            const auto queries=r.U32();Match(static_cast<unsigned>(calls.size()),queries,"Original virtual Node Find call count");
            for(unsigned i=0;i<queries;++i)
            {
                const auto& call=calls.at(i);Match(call.node,r.U32(),"Original own-first child virtual traversal order");
                Match(call.recursive?1:0,r.U32(),"Original recursive lookup flag");Match(call.substring?1:0,r.U32(),"Original substring lookup flag");
                Check(call.name==r.Text(),"Original lookup query bytes/order");++nativeChecks;
            }
            const auto enables=r.U32();Match(static_cast<unsigned>(host.enables.size()),enables,"Original GUI-initiated Node Enable call count");
            for(unsigned i=0;i<enables;++i)for(unsigned j=0;j<3;++j)Match(host.enables.at(i)[j],r.U32(),"Original GUI direct Enable arguments/order");
            const auto writes=r.U32();Check(writes<=512,"Original-only typed group-store trace bound");
            originalOnlyGroupWriteEvents+=writes;originalOnlyGroupWriteScalars+=1+4*writes;
            for(unsigned i=0;i<writes;++i)for(unsigned j=0;j<4;++j)(void)r.U32();
        }
        spNode guard;Check(guard.FindNodeForAnalysis(nullptr,true,true)==nullptr,"Explicit host null-query refusal");
    }
    void NativeTypedBindings(Reader& r)
    {
        struct State{unsigned node;std::vector<std::uint32_t> references,saved;std::vector<std::vector<std::uint32_t>> groups;};
        struct Event{unsigned kind,receiver;State state;};
        const auto cases=r.U32();Check(cases>0&&cases<=32,"Typed original binding case count");
        for(unsigned row=0;row<cases;++row)
        {
            const auto count=r.U32();Check(count>=2&&count<=8,"Typed binding Node graph extent");
            Host host;std::vector<std::shared_ptr<spNode>> nodes(count+1);std::vector<unsigned> parents(count+1);
            for(unsigned i=1;i<=count;++i)
            {
                parents[i]=r.U32();const auto references=r.U32();auto groups=Words(r);nodes[i]=std::make_shared<spNode>();
                host.ids[nodes[i].get()]=i;host.references[nodes[i].get()]=static_cast<std::uint16_t>(references);SetGroups(*nodes[i],groups);
            }
            for(unsigned i=1;i<=count;++i)if(parents[i])Check(nodes.at(parents[i])->AttachChildForAnalysis(nodes[i]),"Stable typed binding child input");
            const auto id=[&](const spNode* p){if(!p)return 0u;for(unsigned i=1;i<=count;++i)if(nodes[i].get()==p)return i;throw std::runtime_error("Unexpected typed binding Node");};
            spGUIObject object(host);
            auto snapshot=[&]()
            {
                State s;s.node=id(object.GetNodeForGUIForAnalysis());s.saved=object.GetSavedCollisionGroupsForAnalysis();
                for(unsigned i=1;i<=count;++i){s.references.push_back(host.references[nodes[i].get()]);s.groups.push_back(Groups(*nodes[i]));}
                return s;
            };
            auto compare=[&](const State& s)
            {
                Match(s.node,r.U32(),"Typed binding current Node visible at boundary");
                for(auto value:s.references)Match(value,r.U32(),"Typed binding 16-bit reference count at boundary");
                Check(s.saved==Words(r),"Typed binding saved collision-group history at boundary");++nativeChecks;
                for(const auto& groups:s.groups){Check(groups==Words(r),"Typed CollisionInfo groups at actual subscription boundary");++nativeChecks;}
            };
            std::vector<Event> events;
            host.beforeSubscription=[&](spNode& receiver,spGUIObject& observed,bool subscribe)
            {
                if(&observed==&object)events.push_back({subscribe?1u:0u,id(&receiver),snapshot()});
            };
            const auto operations=r.U32();Check(operations<=16,"Typed binding sequence bound");
            for(unsigned step=0;step<operations;++step)
            {
                const auto target=r.U32();events.clear();object.SetReferenceForAnalysis(nodes.at(target).get());compare(snapshot());
                const auto expected=r.U32();Match(static_cast<unsigned>(events.size()),expected,"Original typed subscription boundary count");
                for(unsigned i=0;i<expected;++i)
                {
                    Match(events.at(i).kind,r.U32(),"Original unsubscribe/subscribe boundary direction");
                    Match(events.at(i).receiver,r.U32(),"Original Node/child subscription order");compare(events.at(i).state);
                }
            }
            host.beforeSubscription={};
        }
    }
}
int main(int argc,char** argv)
{
    try
    {
        (void)spButton::StaticRTTI();(void)spWidget::StaticRTTI();(void)spGUIObject::StaticRTTI();
        Basics();HistoryAndTeardown();
        if(argc==2)
        {
            Reader r(argv[1]);Check(r.U32()==0x31495547,"GUI1 original fixture identity");
            NativeSelections(r);NativeCopies(r);Check(r.input.peek()==std::char_traits<char>::eof(),"Original GUI fixture exact end");
        }
        else if(argc==3&&std::string(argv[1])=="--binding")
        {
            Reader r(argv[2]);Check(r.U32()==0x31424947,"GIB1 original binding fixture identity");
            NativeBindings(r);Check(r.input.peek()==std::char_traits<char>::eof(),"Original binding fixture exact end");
        }
        else if(argc==3&&std::string(argv[1])=="--notify")
        {
            Reader r(argv[2]);Check(r.U32()==0x314e5547,"GUN1 original Notify fixture identity");
            NativeNotifications(r);Check(r.input.peek()==std::char_traits<char>::eof(),"Original Notify fixture exact end");
        }
        else if(argc==3&&std::string(argv[1])=="--lookup")
        {
            Reader r(argv[2]);Check(r.U32()==0x324c5547,"GUL2 typed lookup fixture identity");
            NativeLookup(r);Check(r.input.peek()==std::char_traits<char>::eof(),"Typed original lookup fixture exact end");
        }
        else if(argc==3&&std::string(argv[1])=="--typed-binding")
        {
            Reader r(argv[2]);Check(r.U32()==0x32424947,"GIB2 typed binding fixture identity");
            NativeTypedBindings(r);Check(r.input.peek()==std::char_traits<char>::eof(),"Typed binding fixture exact end");
        }
        else Check(argc==1,"Optional original GUI fixture path only");
        std::cout<<"PASS "<<checks<<" GUI checks, "<<nativeChecks<<" original comparisons\n";
        if(originalOnlyGroupWriteScalars)std::cout<<"Original-only group store trace: "<<originalOnlyGroupWriteEvents<<" events, "
            <<originalOnlyGroupWriteScalars<<" scalars excluded from current parity\n";
        return 0;
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
