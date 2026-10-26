#include "Code/wxButton.h"
#include "Code/wxHUDPart.h"
#include "Code/SparkBase/spSubscriptionManager.h"
#include "Code/Sparkplug/spNode.h"
#include "Code/Sparkplug/spCollisionInfo.h"
#include "Code/Sparkplug/spOBBBV.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

using namespace sparkplug::reconstruction;
using namespace winx::reconstruction;
namespace
{
    unsigned checks=0,nativeChecks=0;
    void Check(bool value,const char* reason) { ++checks;if(!value)throw std::runtime_error(reason); }
    void Match(unsigned actual,unsigned expected,const char* reason)
    { ++nativeChecks;if(actual!=expected)std::cerr<<reason<<": "<<actual<<" != "<<expected<<'\n';Check(actual==expected,reason); }
    struct Query { unsigned node;std::string name;unsigned recursive,substring; };
    struct Host final:wxButtonHost,wxHUDPartHost
    {
        std::map<const wxButton*,unsigned> buttonIDs;
        std::map<const spNode*,unsigned> nodeIDs;
        std::map<void*,std::uint16_t> refs;
        std::vector<std::array<unsigned,8>> packets;
        std::vector<Query> queries;
        std::vector<std::array<unsigned,3>> enables;
        std::function<void()> beforePart;
        unsigned guiUnqualified=0,partUnqualified=0;
        bool active=false;
        unsigned ID(const wxButton* b) const { return b?buttonIDs.at(b):0; }
        unsigned NodeID(const spNode* n) const { return n?nodeIDs.at(n):0; }
        void AddToEngineManagerForAnalysis(spEntity&) override {}
        void DestroyEntityForAnalysis(spEntity&) noexcept override {}
        std::uint16_t GetReferenceCountForAnalysis(void* p) override { return refs[p]; }
        void SetReferenceCountForAnalysis(void* p,std::uint16_t n) override { refs[p]=n; }
        void DeleteReferenceForAnalysis(void*) override { throw std::logic_error("Unexpected fixture reference delete"); }
        void SubscribeGUINodeForAnalysis(spNode&,spGUIObject&) override {}
        void UnsubscribeGUINodeForAnalysis(spNode&,spGUIObject&) noexcept override {}
        void DestroyGUIForAnalysis(spGUIObject&) noexcept override {}
        bool IsGUINodeEnabledForAnalysis(const spNode& node) override { return node.IsEnabledForAnalysis(); }
        void SetGUINodeEnabledForAnalysis(spNode& node,std::uint8_t value,bool recursive) override
        { if(active)enables.push_back({NodeID(&node),value,recursive?1u:0u});node.SetEnabledForAnalysis(value!=0,recursive); }
        void* GetGUINotificationTargetForAnalysis(const spGUIObject&) override { return nullptr; }
        std::uint32_t ReadGUIInputWord18ForAnalysis(std::uintptr_t) override { throw std::logic_error("Unqualified event read"); }
        void DispatchNotificationForAnalysis(void*,const spNotificationForAnalysis&) override { throw std::logic_error("Unqualified directed notification"); }
        void UnrecoveredGUINotificationForAnalysis(spGUIObject&,const void*,const char*) noexcept override { ++guiUnqualified; }
        void RemoveWxButtonFromEngineManagerForAnalysis(wxButton&) override {}
        void RegisterWxButtonForAnalysis(wxButton&) override {}
        void DestroyWxButtonForAnalysis(wxButton&) noexcept override {}
        void CompleteWxButtonCopyForAnalysis(std::uint32_t) const override {}
        void MoveFromEngineToGameForAnalysis(wxEntity&) override {}
        void DestroyWxEntityForAnalysis(wxEntity&) noexcept override {}
        void CompleteCopyForAnalysis(const wxEntity&,wxEntity&,spCloneManager&,std::uint32_t) const override {}
        std::size_t CachedPredicateCountForAnalysis(const wxEntity&) const override { return 0; }
        bool CachedPredicateForAnalysis(const wxEntity&,std::size_t) const override { return false; }
        bool TimerByteForAnalysis(const wxEntity&) const override { return false; }
        float SquaredDistanceForAnalysis(const wxEntity&) const override { return 0; }
        void WriteComputedFlagsForAnalysis(wxEntity&,std::uint32_t) override {}
        void* ResolveHUDManagerForPartForAnalysis() noexcept override { return nullptr; }
        void RemoveHUDPartFromManagerForAnalysis(void*,std::uint32_t) noexcept override {}
        void CompleteHUDPartCopyForAnalysis(std::uint32_t) const override {}
        void UnrecoveredHUDPartDependencyForAnalysis(wxHUDPart&,const void*,const char*) noexcept override { ++partUnqualified; }
    };
    struct ObservedNode final:spNode
    {
        Host& host;unsigned id;
        ObservedNode(Host& h,unsigned value):host(h),id(value) { host.nodeIDs[this]=id; }
        spNode* FindNodeForAnalysis(const char* name,bool recursive,bool substring) override
        {
            if(host.active)host.queries.push_back({id,name,recursive?1u:0u,substring?1u:0u});
            return spNode::FindNodeForAnalysis(name,recursive,substring);
        }
    };
    struct ObservedPart final:wxHUDPart
    {
        Host& host;
        ObservedPart(Host& h,spSubscriptionManager& manager):wxHUDPart(h,manager),host(h) {}
        void vfunc_0C(const void* frame) noexcept override
        {
            const auto& n=*static_cast<const spNotificationForAnalysis*>(frame);
            host.packets.push_back({n.code,n.word04,n.word08,n.word0C,host.ID(static_cast<const wxButton*>(n.sender)),n.word14,
                static_cast<unsigned>(n.payload),static_cast<unsigned>(n.tail)});
            if(host.beforePart)host.beforePart();
            wxHUDPart::vfunc_0C(frame);
        }
    };
    struct Reader
    {
        std::ifstream input;
        explicit Reader(const char* path):input(path,std::ios::binary) { Check(bool(input),"Open wxButton Notify original fixture"); }
        unsigned U32() { unsigned value=0;input.read(reinterpret_cast<char*>(&value),4);Check(bool(input),"Notify fixture scalar extent");return value; }
        std::string Text() { const auto n=U32();Check(n<=4096,"Notify fixture text bound");std::string value(n,'\0');input.read(value.data(),n);Check(bool(input),"Notify fixture text extent");return value; }
        std::vector<unsigned> Vector()
        { const auto n=U32();Check(n<=128,"Notify fixture vector bound");std::vector<unsigned> values;for(unsigned i=0;i<n;++i)values.push_back(U32());return values; }
    };
    void MatchVector(const std::vector<unsigned>& actual,const std::vector<unsigned>& expected,const char* reason)
    { Match(static_cast<unsigned>(actual.size()),static_cast<unsigned>(expected.size()),reason);for(unsigned i=0;i<expected.size();++i)Match(actual.at(i),expected[i],reason); }
    std::vector<unsigned> Groups(const spNode& node)
    { std::vector<unsigned> result;for(std::size_t i=0;i<node.GetCollisionCountForAnalysis();++i)result.push_back(node.GetCollisionForAnalysis(i)->GetGroupForAnalysis());return result; }
    void Basics()
    {
        Check(spSubscriptionManager::GetInstance()==nullptr,"Notify cold admission starts without a manager");
        Host host;wxButton button(static_cast<wxButtonHost&>(host));spNotificationForAnalysis message;message.code=0x1c;message.payload=1;
        button.vfunc_0C(&message);
        Check(host.guiUnqualified==1&&message.code==0x1c&&message.payload==1,"Missing manager reports own boundary before base fallback");
        for(auto code:{0x0eu,0x0fu,0x10u,0x11u,0x12u,0x18u})
        { message.code=code;button.vfunc_0C(&message); }
        message.code=0x1c;message.payload=0;button.vfunc_0C(&message);message.payload=2;button.vfunc_0C(&message);
        Check(host.guiUnqualified==9,"Other own Notify branches stay explicit refusals");
    }
    void Fixture(const char* path)
    {
        Reader r(path);char magic[4];r.input.read(magic,4);Check(std::memcmp(magic,"WBN1",4)==0&&r.U32()==1,"Notify fixture header");const auto count=r.U32();Check(count<=128,"Notify fixture case bound");
        for(unsigned caseIndex=0;caseIndex<count;++caseIndex)
        {
            const auto label=r.Text();const auto graph=r.U32()!=0;const auto missing=r.Vector();
            std::array<unsigned,7> c;for(auto& value:c)value=r.U32();std::array<unsigned,7> mutation;for(auto& value:mutation)value=r.U32();
            const auto byteCount=r.U32();Check(byteCount<=16,"Notify byte mutation bound");std::vector<std::array<unsigned,2>> bytes;
            for(unsigned i=0;i<byteCount;++i){const auto offset=r.U32(),value=r.U32();bytes.push_back({offset,value});}
            const auto captionChanges=r.U32()!=0;const auto changedCaption=r.Text();
            Host host;spSubscriptionManager subscriptions;
            // Dependencies precede GUI objects so every teardown reference is live.
            std::array<std::shared_ptr<ObservedNode>,8> nodes;
            const std::array<const char*,7> names{"root","NORMAL","DISABLED","HIGHLIGHTED","PUSHED","GUICollision","old"};
            for(unsigned i=1;i<8;++i)
            {
                nodes[i]=std::make_shared<ObservedNode>(host,i);
                const bool absent=std::find(missing.begin(),missing.end(),i)!=missing.end();
                nodes[i]->SetName(absent?"unmatched-"+std::to_string(i):"prefix-"+std::string(names[i-1])+"-suffix");host.refs[nodes[i].get()]=2;
                if(i>=2&&i<=6)
                    for(auto group:{0x12000000u+i,0x34000000u+i})
                    {
                        auto info=std::make_shared<spCollisionInfo>();info->SetGroupForAnalysis(group);info->SetPrimitiveForAnalysis(std::make_shared<spOBBBV>());
                        Check(nodes[i]->AttachCollisionForAnalysis(std::move(info)),"Typed common collision attachment");
                    }
            }
            if(graph)for(unsigned i=2;i<=6;++i)Check(nodes[1]->AttachChildForAnalysis(nodes[i]),"Stable typed child graph");
            std::array<std::unique_ptr<wxButton>,4> buttons;
            for(unsigned i=1;i<4;++i){buttons[i]=std::make_unique<wxButton>(static_cast<wxButtonHost&>(host));host.buttonIDs[buttons[i].get()]=i;}
            ObservedPart part(host,subscriptions);part.SetHUDStateForAnalysis({},nullptr,nullptr,c[1],0,{0,0,0});Check(subscriptions.SubscribeForAnalysis(3,part),"Warm actual common Part subscription");
            auto& button=*buttons[1];button.spEntity::SetReferenceForAnalysis(nodes[1].get());std::memcpy(button.GetOwnBytesForAnalysis().data(),&c[0],4);
            button.SetSelectionStateForAnalysis(static_cast<std::uint8_t>(c[2]),static_cast<std::uint8_t>(c[3]),static_cast<std::uint8_t>(c[4]),nodes[7].get(),
                {nodes[2].get(),nodes[3].get(),nodes[4].get()},7);button.SetButtonStateForAnalysis("initial action",static_cast<std::uint8_t>(c[5]),nodes[5].get());button.GetOwnBytesForAnalysis()[0x144]=static_cast<std::uint8_t>(c[6]);
            struct Output { unsigned before=0x69696969;spGUIObject* object=reinterpret_cast<spGUIObject*>(std::uintptr_t{0x69696969});unsigned after=0x69696969; } output;
            spNotificationForAnalysis incoming;incoming.code=0x1c;incoming.word04=0x12345678;incoming.word08=0x9abcdef0;incoming.word0C=0x55aa55aa;
            incoming.sender=buttons[2].get();incoming.word14=0x33330000;incoming.payload=1;incoming.tail=0xdeadbeef;
            bool mutated=false;host.beforePart=[&]
            {
                if(mutated)return;
                for(auto pair:bytes)
                {
                    auto enabled=button.GetEnabledByteForAnalysis(),visible=button.GetVisibleByteForAnalysis(),highlight=button.GetHighlightByteForAnalysis();
                    if(pair[0]==0x28)enabled=static_cast<std::uint8_t>(pair[1]);
                    else if(pair[0]==0x3c)visible=static_cast<std::uint8_t>(pair[1]);
                    else if(pair[0]==0x3d)highlight=static_cast<std::uint8_t>(pair[1]);
                    else if(pair[0]==0x58){button.SetButtonStateForAnalysis(button.GetStringForAnalysis(),static_cast<std::uint8_t>(pair[1]),button.GetPressedNodeForAnalysis());continue;}
                    else if(pair[0]==0x1a4){button.GetOwnBytesForAnalysis()[0x144]=static_cast<std::uint8_t>(pair[1]);continue;}
                    else throw std::logic_error("Unknown Notify application byte mutation");
                    button.SetSelectionStateForAnalysis(enabled,visible,highlight,button.GetSelectedNodeForAnalysis(),button.GetStateNodesForAnalysis(),button.GetSelectionIndexForAnalysis());
                }
                if(mutation[5])std::memcpy(button.GetOwnBytesForAnalysis().data(),&mutation[6],4);
                if(captionChanges)button.SetButtonStateForAnalysis(changedCaption,button.GetPressedByteForAnalysis(),button.GetPressedNodeForAnalysis());
                if(mutation[0])incoming.code=mutation[1];if(mutation[3])incoming.payload=mutation[4];
                if(mutation[2])incoming.payload=reinterpret_cast<std::uintptr_t>(&output.object);
                mutated=true;
            };
            host.active=true;button.vfunc_0C(&incoming);host.active=false;host.beforePart={};
            unsigned key;std::memcpy(&key,button.GetOwnBytesForAnalysis().data(),4);
            for(auto value:{key,unsigned(button.GetEnabledByteForAnalysis()),unsigned(button.GetVisibleByteForAnalysis()),unsigned(button.GetHighlightByteForAnalysis()),
                unsigned(button.GetPressedByteForAnalysis()),unsigned(button.GetRefreshGateByteForAnalysis()),host.NodeID(button.GetSelectedNodeForAnalysis())})Match(value,r.U32(),label.c_str());
            for(auto* node:button.GetStateNodesForAnalysis())Match(host.NodeID(node),r.U32(),"Notify resolved state node");
            Match(host.NodeID(button.GetPressedNodeForAnalysis()),r.U32(),"Notify resolved PUSHED");Match(button.GetSelectionIndexForAnalysis(),r.U32(),"Notify does not refresh selection index");
            ++nativeChecks;Check(button.GetStringForAnalysis()==r.Text(),"Notify live caption");std::vector<unsigned> members;for(auto* member:part.GetButtonsForAnalysis())members.push_back(host.ID(member));MatchVector(members,r.Vector(),"Notify actual Part registration");
            const auto mode=incoming.payload==reinterpret_cast<std::uintptr_t>(&output.object)?0x100u:static_cast<unsigned>(incoming.payload);
            for(auto value:{incoming.code,incoming.word04,incoming.word08,incoming.word0C,host.ID(static_cast<const wxButton*>(incoming.sender)),incoming.word14,mode,static_cast<unsigned>(incoming.tail)})Match(value,r.U32(),"Original incoming frame remains live and distinct");
            const auto outputID=output.object==static_cast<spGUIObject*>(&button)?1u:0u;Match(outputID,r.U32(),"Notify code18 uses actual original output cell");
            Check(output.before==0x69696969&&output.after==0x69696969,"Logical pointer output cell guards");
            auto n=r.U32();Match(static_cast<unsigned>(host.packets.size()),n,"Notify actual broadcast packet count");for(unsigned i=0;i<n;++i)for(unsigned k=0;k<8;++k)Match(host.packets.at(i)[k],r.U32(),"Notify emitted8word frame");
            n=r.U32();Match(static_cast<unsigned>(host.queries.size()),n,"Notify actual virtual Find count");
            for(unsigned i=0;i<n;++i){const auto node=r.U32();const auto query=r.Text();const auto recursive=r.U32(),substring=r.U32();const auto& q=host.queries.at(i);Match(q.node,node,"Notify Find receiver");++nativeChecks;Check(q.name==query,"Notify Find query");Match(q.recursive,recursive,"Notify Find recursive");Match(q.substring,substring,"Notify Find substring");}
            n=r.U32();Match(static_cast<unsigned>(host.enables.size()),n,"Notify actual Node enable count");for(unsigned i=0;i<n;++i)for(unsigned k=0;k<3;++k)Match(host.enables.at(i)[k],r.U32(),"Notify actual Node enable call");
            for(unsigned i=1;i<8;++i)Match(nodes[i]->GetFlagsForAnalysis(),r.U32(),"Notify final typed Node flags");
            for(unsigned i=1;i<8;++i)MatchVector(Groups(*nodes[i]),r.Vector(),"Notify typed CollisionInfo groups");
            Check(host.guiUnqualified==0&&host.partUnqualified==0,"Qualified whole producer has no unresolved own dependency");
        }
        Check(r.input.peek()==std::char_traits<char>::eof(),"Notify fixture consumed");
    }
}
int main(int argc,char** argv)
{
    try
    {
        Basics();if(argc==3&&std::string(argv[1])=="--fixture")Fixture(argv[2]);else Check(argc==1,"Usage: --fixture original.bin");
        std::cout<<"PASS "<<checks<<" wxButton Notify checks, "<<nativeChecks<<" current original scalar comparisons\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<"FAIL wxButton Notify: "<<e.what()<<'\n';return 1;}
}
