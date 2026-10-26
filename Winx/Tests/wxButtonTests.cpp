#include "Code/wxButton.h"
#include "Code/Sparkplug/spNode.h"
#include "Analysis/PC/wxButtonAbi.h"
#include "Analysis/PS2/wxButtonAbi.h"
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
    unsigned checks=0,nativeChecks=0,excludedAllocatorBytes=0;
    void Check(bool value,const char* reason) { ++checks;if(!value)throw std::runtime_error(reason); }
    void Match(std::uint32_t got,std::uint32_t expected,const char* reason)
    {
        ++nativeChecks;
        if(got!=expected)std::cerr<<reason<<": "<<got<<" != "<<expected<<'\n';
        Check(got==expected,reason);
    }
    void MatchText(const std::string& got,const std::string& expected,const char* reason)
    { ++nativeChecks;Check(got==expected,reason); }
    using Own=wxButton::OwnBytesForAnalysis;
    struct Event { std::string kind,phase,target;std::uint32_t value;Own own; };
    struct Host final:wxButtonHost
    {
        std::map<void*,std::uint16_t> references;
        std::map<const spNode*,unsigned> ids;
        std::map<const wxButton*,std::string> names;
        std::vector<std::array<unsigned,3>> enables;
        mutable std::vector<Event> events;
        std::string phase="basic",pendingName="basic";
        wxButton* lastRegistered=nullptr;
        wxButton* copyTarget=nullptr;
        unsigned unresolved=0;
        std::function<void()> beforeEnable;
        void Record(const char* kind,const wxButton& button,std::uint32_t value=0) const
        { events.push_back({kind,phase,names.at(&button),value,button.GetOwnBytesForAnalysis()}); }
        void AddToEngineManagerForAnalysis(spEntity&) override {}
        void DestroyEntityForAnalysis(spEntity& e) noexcept override
        { if(auto* p=e.GetReferenceForAnalysis())--references[p]; }
        std::uint16_t GetReferenceCountForAnalysis(void* p) override { return references[p]; }
        void SetReferenceCountForAnalysis(void* p,std::uint16_t n) override { references[p]=n; }
        void DeleteReferenceForAnalysis(void*) override { Check(false,"Unexpected fixture reference deletion"); }
        void SubscribeGUINodeForAnalysis(spNode&,spGUIObject&) override {}
        void UnsubscribeGUINodeForAnalysis(spNode&,spGUIObject&) noexcept override {}
        void DestroyGUIForAnalysis(spGUIObject&) noexcept override {}
        bool IsGUINodeEnabledForAnalysis(const spNode& n) override { return n.IsEnabledForAnalysis(); }
        void SetGUINodeEnabledForAnalysis(spNode& n,std::uint8_t value,bool recursive) override
        {
            enables.push_back({ids.at(&n),value,recursive?1u:0u});
            if(beforeEnable)beforeEnable();
            n.SetEnabledForAnalysis(value!=0,recursive);
        }
        void* GetGUINotificationTargetForAnalysis(const spGUIObject&) override { return nullptr; }
        std::uint32_t ReadGUIInputWord18ForAnalysis(std::uintptr_t) override
        { throw std::logic_error("No Button Notify fixture"); }
        void DispatchNotificationForAnalysis(void*,const spNotificationForAnalysis&) override
        { throw std::logic_error("No wxButton Notify fixture"); }
        void UnrecoveredGUINotificationForAnalysis(spGUIObject&,const void*,const char*) noexcept override { ++unresolved; }
        void RemoveWxButtonFromEngineManagerForAnalysis(wxButton& b) override
        { names[&b]=pendingName;Record("remove-engine",b); }
        void RegisterWxButtonForAnalysis(wxButton& b) override
        { lastRegistered=&b;Record("register",b); }
        void DestroyWxButtonForAnalysis(wxButton& b) noexcept override { Record("destroy",b); }
        void CompleteWxButtonCopyForAnalysis(std::uint32_t id) const override
        { Record("copy-complete",*(copyTarget?copyTarget:lastRegistered),id); }
    };
    struct Reader
    {
        std::ifstream input;
        explicit Reader(const char* path):input(path,std::ios::binary) { Check(bool(input),"Open wxButton fixture"); }
        std::uint32_t U32()
        { std::uint32_t v=0;input.read(reinterpret_cast<char*>(&v),4);Check(bool(input),"wxButton fixture uint32 extent");return v; }
        std::string Text()
        { auto n=U32();Check(n<=4096,"wxButton fixture text bound");std::string s(n,'\0');input.read(s.data(),n);Check(bool(input),"wxButton fixture text extent");return s; }
        Own Bytes()
        { Check(U32()==0x148,"wxButton own-byte extent");Own b;input.read(reinterpret_cast<char*>(b.data()),b.size());Check(bool(input),"wxButton own-byte data");return b; }
        unsigned Header(const char* magic)
        {
            char got[4];input.read(got,4);Check(std::memcmp(got,magic,4)==0,"wxButton fixture magic");
            Check(U32()==1,"wxButton fixture version");const auto n=U32();Check(n<=128,"wxButton case count");return n;
        }
        void End() { Check(input.peek()==std::char_traits<char>::eof(),"wxButton fixture fully consumed"); }
    };
    void MatchOwn(const Own& actual,const Own& expected,std::size_t count)
    {
        for(std::size_t i=0;i<count;++i)Match(actual[i],expected[i],"wxButton original own byte");
        excludedAllocatorBytes+=static_cast<unsigned>(expected.size()-count);
    }
    void Basics()
    {
        Host host;wxButton::SetFactoryHostForAnalysis(&host);
        auto object=spRTTIManager::Instance().Create(wxButton::ClassID);
        auto* button=dynamic_cast<wxButton*>(object.get());Check(button!=nullptr,"wxButton registered concrete factory");
        Check(button->vfunc_18().baseClassID==spButton::ClassID,"wxButton physical and registered Button base");
        Check(button->GetRefreshGateByteForAnalysis()==1,"wxButton constructor refresh gate");
        spCloneManager manager;host.pendingName="clone";
        auto clone=object->vfunc_10(manager);Check(dynamic_cast<wxButton*>(clone.get())!=nullptr,"wxButton concrete clone");
        spGUIObject* reply=nullptr;spNotificationForAnalysis n;n.code=0x18;n.payload=reinterpret_cast<std::uintptr_t>(&reply);
        object->vfunc_0C(&n);Check(!reply&&host.unresolved==1,"Own unknown Notify reports an explicit boundary");
        wxButton::SetFactoryHostForAnalysis(nullptr);
    }
    struct Mutation
    {
        unsigned at;
        std::vector<std::array<unsigned,2>> bytes,nodes,nodeEnabled;
    };
    std::vector<std::array<unsigned,2>> Pairs(Reader& r)
    {
        const auto count=r.U32();Check(count<=16,"wxButton mutation count bound");
        std::vector<std::array<unsigned,2>> result;
        for(unsigned i=0;i<count;++i){const auto a=r.U32(),b=r.U32();result.push_back({a,b});}
        return result;
    }
    void Selection(const char* path)
    {
        Reader r(path);const auto count=r.Header("WXB1");
        for(unsigned caseIndex=0;caseIndex<count;++caseIndex)
        {
            const auto label=r.Text();std::array<unsigned,14> c;for(auto& v:c)v=r.U32();
            const auto hidden=r.U32();Mutation m;m.at=r.U32();m.bytes=Pairs(r);m.nodes=Pairs(r);m.nodeEnabled=Pairs(r);
            Host host;std::array<std::shared_ptr<spNode>,7> nodes;
            for(unsigned i=1;i<7;++i){nodes[i]=std::make_shared<spNode>();host.ids[nodes[i].get()]=i;host.references[nodes[i].get()]=2;}
            Check(nodes[1]->AttachChildForAnalysis(nodes[6]),"wxButton stable child fixture");
            wxButton button(host);button.spEntity::SetReferenceForAnalysis(nodes.at(c[7]).get());
            button.SetSelectionStateForAnalysis(static_cast<std::uint8_t>(c[2]),static_cast<std::uint8_t>(c[3]),static_cast<std::uint8_t>(c[4]),
                nodes.at(c[8]).get(),{nodes.at(c[9]).get(),nodes.at(c[10]).get(),nodes.at(c[11]).get()},c[13]);
            button.SetButtonStateForAnalysis("",static_cast<std::uint8_t>(c[5]),nodes.at(c[12]).get());
            button.GetOwnBytesForAnalysis()[0x144]=static_cast<std::uint8_t>(c[6]);
            for(unsigned i=1;i<7;++i)if(hidden&(1u<<i))nodes[i]->SetEnabledForAnalysis(false,false);
            std::vector<std::array<unsigned,5>> snapshots;bool mutated=false;
            const auto id=[&](const spNode* node){return node?host.ids.at(node):0;};
            const auto rawState=[&](unsigned offset,unsigned value)
            {
                auto enabled=button.GetEnabledByteForAnalysis(),visible=button.GetVisibleByteForAnalysis(),highlight=button.GetHighlightByteForAnalysis();
                auto* selected=button.GetSelectedNodeForAnalysis();auto states=button.GetStateNodesForAnalysis();
                if(offset==0x28)enabled=static_cast<std::uint8_t>(value);
                else if(offset==0x3c)visible=static_cast<std::uint8_t>(value);
                else if(offset==0x3d)highlight=static_cast<std::uint8_t>(value);
                else if(offset==0x40)selected=nodes.at(value).get();
                else if(offset>=0x44&&offset<=0x4c)states[(offset-0x44)/4]=nodes.at(value).get();
                else if(offset==0x58){button.SetButtonStateForAnalysis("",static_cast<std::uint8_t>(value),button.GetPressedNodeForAnalysis());return;}
                else if(offset==0x5c){button.SetButtonStateForAnalysis("",button.GetPressedByteForAnalysis(),nodes.at(value).get());return;}
                else if(offset==0x1a4){button.GetOwnBytesForAnalysis()[0x144]=static_cast<std::uint8_t>(value);return;}
                else throw std::logic_error("Unexpected wxButton mutation offset");
                button.SetSelectionStateForAnalysis(enabled,visible,highlight,selected,states,button.GetSelectionIndexForAnalysis());
            };
            host.beforeEnable=[&]
            {
                snapshots.push_back({button.GetEnabledByteForAnalysis(),button.GetHighlightByteForAnalysis(),button.GetPressedByteForAnalysis(),
                    id(button.GetSelectedNodeForAnalysis()),button.GetSelectionIndexForAnalysis()});
                if(m.at&&!mutated&&host.enables.size()==m.at)
                {
                    for(auto pair:m.bytes)rawState(pair[0],pair[1]);
                    for(auto pair:m.nodes)rawState(pair[0],pair[1]);
                    for(auto pair:m.nodeEnabled)nodes.at(pair[0])->SetEnabledForAnalysis(pair[1]!=0,false);
                    mutated=true;
                }
            };
            if(c[0]==0)button.RefreshSelectionForAnalysis();
            else if(c[0]==1)button.SetStateForAnalysis(c[1]);
            else if(c[0]==2)button.SetEnabledForAnalysis(static_cast<std::uint8_t>(c[1]));
            else if(c[0]==3)button.SetVisibleForAnalysis(static_cast<std::uint8_t>(c[1]));
            else throw std::logic_error("Unknown wxButton fixture operation");
            for(auto value:{unsigned(button.GetEnabledByteForAnalysis()),unsigned(button.GetVisibleByteForAnalysis()),unsigned(button.GetHighlightByteForAnalysis()),
                unsigned(button.GetPressedByteForAnalysis()),unsigned(button.GetRefreshGateByteForAnalysis()),id(button.GetSelectedNodeForAnalysis()),
                button.GetSelectionIndexForAnalysis(),id(button.GetNodeForGUIForAnalysis())})Match(value,r.U32(),label.c_str());
            for(auto* node:button.GetStateNodesForAnalysis())Match(id(node),r.U32(),"wxButton state node");
            Match(id(button.GetPressedNodeForAnalysis()),r.U32(),"wxButton PUSHED node");
            for(unsigned i=1;i<7;++i)Match(nodes[i]->GetFlagsForAnalysis(),r.U32(),"wxButton Node flags");
            auto n=r.U32();Match(static_cast<unsigned>(host.enables.size()),n,"wxButton enable count");
            for(unsigned i=0;i<n;++i)for(unsigned j=0;j<3;++j)Match(host.enables.at(i)[j],r.U32(),"wxButton original enable call");
            n=r.U32();Match(static_cast<unsigned>(snapshots.size()),n,"wxButton callback snapshot count");
            for(unsigned i=0;i<n;++i)for(unsigned j=0;j<5;++j)Match(snapshots.at(i)[j],r.U32(),"wxButton callback live state");
            Check(host.unresolved==0,"Selection has no unresolved Notify path");
        }
        r.End();
    }
    void Lifecycle(const char* path)
    {
        Reader r(path);const auto count=r.Header("WXL1");
        for(unsigned caseIndex=0;caseIndex<count;++caseIndex)
        {
            (void)r.U32();const auto clone=r.U32()!=0;
            const auto ctor=r.Bytes(),populated=r.Bytes(),targetBefore=r.Bytes(),targetAfter=r.Bytes();
            const auto returnValue=r.U32(),sourcePreserved=r.U32(),sourceFreed=r.U32(),destinationFreed=r.U32();
            Host host;Own seed;seed.fill(0xcc);host.phase="constructor";host.pendingName="source";
            auto source=std::make_unique<wxButton>(host,seed);MatchOwn(source->GetOwnBytesForAnalysis(),ctor,0x148);
            source->GetOwnBytesForAnalysis()=populated;const auto sourceBefore=source->GetOwnBytesForAnalysis();
            std::unique_ptr<wxButton> destination;spCloneManager manager;unsigned actualReturn=1;
            if(clone)
            {
                host.phase="clone";host.pendingName="clone";
                auto result=source->vfunc_10(manager);auto* button=dynamic_cast<wxButton*>(result.get());
                Check(button!=nullptr,"Original clone retains wxButton dynamic type");result.release();destination.reset(button);
            }
            else
            {
                host.phase="destination-constructor";host.pendingName="destination";
                destination=std::make_unique<wxButton>(host,seed);destination->GetOwnBytesForAnalysis()=targetBefore;
                host.copyTarget=destination.get();host.phase="copy";actualReturn=source->vfunc_14(*destination,manager)?1:0;
            }
            Match(actualReturn,returnValue,"wxButton copy success");
            Match(source->GetOwnBytesForAnalysis()==sourceBefore?1:0,sourcePreserved,"wxButton source preserved");
            MatchOwn(destination->GetOwnBytesForAnalysis(),targetAfter,clone?0x145:0x148);
            std::memset(source->GetOwnBytesForAnalysis().data(),0,4);std::memset(destination->GetOwnBytesForAnalysis().data(),0,4);
            host.phase="delete-destination";destination.reset();Match(1,destinationFreed,"wxButton destination destructor completed");
            host.phase="delete-source";source.reset();Match(1,sourceFreed,"wxButton source destructor completed");
            const auto n=r.U32();Match(static_cast<unsigned>(host.events.size()),n,"wxButton foreign lifecycle boundary count");
            for(unsigned i=0;i<n;++i)
            {
                const auto kind=r.Text(),phase=r.Text(),target=r.Text();const auto value=r.U32();const auto own=r.Bytes();
                const auto& event=host.events.at(i);MatchText(event.kind,kind,"wxButton foreign call kind");
                MatchText(event.phase,phase,"wxButton foreign call phase");MatchText(event.target,target,"wxButton foreign call target");
                Match(event.value,value,"wxButton foreign call argument");
                if(clone&&target=="clone"&&(kind=="remove-engine"||kind=="register"))
                { Match(event.own[0x144],own[0x144],"wxButton clone pre-registration gate");excludedAllocatorBytes+=0x147; }
                else MatchOwn(event.own,own,clone&&target=="clone"?0x145:0x148);
            }
        }
        r.End();
    }
}
int main(int argc,char** argv)
{
    try
    {
        Basics();
        if(argc==3&&std::string(argv[1])=="--selection")Selection(argv[2]);
        else if(argc==3&&std::string(argv[1])=="--lifecycle")Lifecycle(argv[2]);
        else Check(argc==1,"Usage: --selection or --lifecycle fixture.bin");
        std::cout<<"PASS "<<checks<<" wxButton checks, "<<nativeChecks<<" current original scalar comparisons; "
            <<excludedAllocatorBytes<<" original allocator bytes excluded\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<"FAIL wxButton: "<<e.what()<<'\n';return 1;}
}
