#include "Code/wxHUDPart.h"
#include "Code/Sparkplug/spNode.h"
#include "Code/SparkBase/spSubscriptionManager.h"
#include "Code/SparkBase/spBroadcastNotification.h"
#include "Analysis/PC/wxHUDPartAbi.h"
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
    unsigned checks=0,nativeChecks=0,originalOnlyLayoutScalars=0,originalOnlyReferenceScalars=0,originalOnlyNamedCompletions=0;
    void Check(bool value,const char* reason) { ++checks;if(!value)throw std::runtime_error(reason); }
    void Match(unsigned actual,unsigned expected,const char* reason)
    { ++nativeChecks;if(actual!=expected)std::cerr<<reason<<": "<<actual<<" != "<<expected<<'\n';Check(actual==expected,reason); }
    struct State
    {
        std::vector<unsigned> members;
        unsigned initial,selected,key,layer;
        std::array<unsigned,3> flags;
        unsigned field20;
    };
    struct Host final:wxHUDPartHost,wxButtonHost
    {
        std::map<const wxButton*,unsigned> buttonIDs;
        std::map<const spNode*,unsigned> nodeIDs;
        std::map<void*,std::uint16_t> refs;
        std::vector<std::array<unsigned,3>> calls,enables;
        std::vector<std::array<unsigned,8>> messages;
        mutable std::vector<unsigned> copyIDs;
        bool active=false,storageReleased=false;
        unsigned foreignToken=0;
        unsigned unresolved=0,buttonDeletes=0,foreignHUDRemoves=0;
        wxHUDPart* tearingPart=nullptr;
        State prefixState{};
        std::function<void()> beforeEnable;
        unsigned ID(const wxButton* b) const { return b?buttonIDs.at(b):0; }
        unsigned NodeID(const spNode* n) const { return n?nodeIDs.at(n):0; }
        State Snapshot(const wxHUDPart& part) const
        {
            State result;for(auto* button:part.GetButtonsForAnalysis())result.members.push_back(ID(button));
            result.initial=ID(part.GetInitialButtonForAnalysis());result.selected=ID(part.GetSelectedButtonForAnalysis());
            result.key=part.GetPartKeyForAnalysis();result.layer=part.GetLayerWordForAnalysis();
            for(unsigned i=0;i<3;++i)result.flags[i]=part.GetFlagsForAnalysis()[i];
            result.field20=part.GetField20ForAnalysis();return result;
        }
        void AddToEngineManagerForAnalysis(spEntity&) override {}
        void DestroyEntityForAnalysis(spEntity&) noexcept override {}
        std::uint16_t GetReferenceCountForAnalysis(void* p) override { return refs[p]; }
        void SetReferenceCountForAnalysis(void* p,std::uint16_t count) override { refs[p]=count; }
        void DeleteReferenceForAnalysis(void*) override { throw std::logic_error("Unexpected fixture reference release"); }
        void MoveFromEngineToGameForAnalysis(wxEntity&) override {}
        void DestroyWxEntityForAnalysis(wxEntity&) noexcept override {}
        void CompleteCopyForAnalysis(const wxEntity&,wxEntity&,spCloneManager&,std::uint32_t id) const override { copyIDs.push_back(id); }
        std::size_t CachedPredicateCountForAnalysis(const wxEntity&) const override { return 0; }
        bool CachedPredicateForAnalysis(const wxEntity&,std::size_t) const override { return false; }
        bool TimerByteForAnalysis(const wxEntity&) const override { return false; }
        float SquaredDistanceForAnalysis(const wxEntity&) const override { return 0; }
        void WriteComputedFlagsForAnalysis(wxEntity&,std::uint32_t) override {}
        void SubscribeGUINodeForAnalysis(spNode&,spGUIObject&) override {}
        void UnsubscribeGUINodeForAnalysis(spNode&,spGUIObject&) noexcept override {}
        void DestroyGUIForAnalysis(spGUIObject&) noexcept override {}
        bool IsGUINodeEnabledForAnalysis(const spNode& n) override { return n.IsEnabledForAnalysis(); }
        void SetGUINodeEnabledForAnalysis(spNode& n,std::uint8_t value,bool recursive) override
        {
            if(active){enables.push_back({NodeID(&n),value,recursive?1u:0u});if(beforeEnable)beforeEnable();}
            n.SetEnabledForAnalysis(value!=0,recursive);
        }
        void* GetGUINotificationTargetForAnalysis(const spGUIObject&) override { return nullptr; }
        std::uint32_t ReadGUIInputWord18ForAnalysis(std::uintptr_t) override { throw std::logic_error("No GUI input fixture"); }
        void DispatchNotificationForAnalysis(void*,const spNotificationForAnalysis&) override { throw std::logic_error("No directed GUI notification fixture"); }
        void UnrecoveredGUINotificationForAnalysis(spGUIObject&,const void*,const char*) noexcept override { ++unresolved; }
        void RemoveWxButtonFromEngineManagerForAnalysis(wxButton&) override {}
        void RegisterWxButtonForAnalysis(wxButton&) override {}
        void DestroyWxButtonForAnalysis(wxButton&) noexcept override { ++buttonDeletes; }
        void CompleteWxButtonCopyForAnalysis(std::uint32_t) const override {}
        void* ResolveHUDManagerForPartForAnalysis() noexcept override
        {
            if(tearingPart)
            {
                prefixState=Snapshot(*tearingPart);storageReleased=tearingPart->GetButtonsForAnalysis().capacity()==0;
                return nullptr; // The original prefix stopped before this foreign body.
            }
            return &foreignToken;
        }
        void RemoveHUDPartFromManagerForAnalysis(void*,std::uint32_t) noexcept override { ++foreignHUDRemoves; }
        void CompleteHUDPartCopyForAnalysis(std::uint32_t id) const override { copyIDs.push_back(id); }
        void UnrecoveredHUDPartDependencyForAnalysis(wxHUDPart&,const void*,const char*) noexcept override { ++unresolved; }
    };
    struct ObservedButton final:wxButton
    {
        Host& host;unsigned id;
        ObservedButton(Host& h,unsigned value):wxButton(static_cast<wxButtonHost&>(h)),host(h),id(value) { host.buttonIDs[this]=id; }
        void SetStateForAnalysis(std::uint32_t value) override
        { if(host.active)host.calls.push_back({0,id,value});wxButton::SetStateForAnalysis(value); }
        void RefreshSelectionForAnalysis() override
        { if(host.active)host.calls.push_back({1,id,0});wxButton::RefreshSelectionForAnalysis(); }
    };
    struct ObservedPart final:wxHUDPart
    {
        Host& host;
        ObservedPart(Host& h,spSubscriptionManager& m):wxHUDPart(h,m),host(h) {}
        void vfunc_0C(const void* notification) noexcept override
        {
            const auto& n=*static_cast<const spNotificationForAnalysis*>(notification);
            host.messages.push_back({n.code,n.word04,n.word08,n.word0C,host.ID(static_cast<const wxButton*>(n.sender)),n.word14,
                static_cast<unsigned>(n.payload),static_cast<unsigned>(n.tail)});
            wxHUDPart::vfunc_0C(notification);
        }
    };
    struct Reader
    {
        std::ifstream input;
        explicit Reader(const char* path):input(path,std::ios::binary) { Check(bool(input),"Open HUDPart original fixture"); }
        unsigned U32() { unsigned value=0;input.read(reinterpret_cast<char*>(&value),4);Check(bool(input),"HUDPart fixture scalar extent");return value; }
        std::string Text() { auto n=U32();Check(n<=4096,"HUDPart fixture text bound");std::string s(n,'\0');input.read(s.data(),n);Check(bool(input),"HUDPart fixture text extent");return s; }
        std::vector<unsigned> Vector()
        { const auto count=U32();Check(count<=128,"HUDPart fixture vector bound");std::vector<unsigned> values;for(unsigned i=0;i<count;++i)values.push_back(U32());return values; }
        State OwnState()
        {
            State s;s.members=Vector();s.initial=U32();s.selected=U32();s.key=U32();s.layer=U32();
            for(auto& value:s.flags)value=U32();s.field20=U32();(void)U32();(void)U32();originalOnlyLayoutScalars+=2;return s;
        }
        void End() { Check(input.peek()==std::char_traits<char>::eof(),"HUDPart fixture consumed"); }
    };
    void MatchVector(const std::vector<unsigned>& actual,const std::vector<unsigned>& expected,const char* reason)
    { Match(static_cast<unsigned>(actual.size()),static_cast<unsigned>(expected.size()),reason);for(unsigned i=0;i<expected.size();++i)Match(actual.at(i),expected[i],reason); }
    void MatchState(const State& actual,const State& expected)
    {
        MatchVector(actual.members,expected.members,"HUDPart borrowed member order");
        Match(actual.initial,expected.initial,"HUDPart initial pointer");Match(actual.selected,expected.selected,"HUDPart selected pointer");
        Match(actual.key,expected.key,"HUDPart key");Match(actual.layer,expected.layer,"HUDPart layer");
        for(unsigned i=0;i<3;++i)Match(actual.flags[i],expected.flags[i],"HUDPart raw flag");
        Match(actual.field20,expected.field20,"HUDPart Entity field20");
    }
    std::array<std::unique_ptr<ObservedButton>,4> Buttons(Host& host)
    {
        std::array<std::unique_ptr<ObservedButton>,4> result;
        for(unsigned i=1;i<4;++i)result[i]=std::make_unique<ObservedButton>(host,i);return result;
    }
    void Apply(wxHUDPart& part,const State& s,const std::array<std::unique_ptr<ObservedButton>,4>& buttons)
    {
        std::vector<wxButton*> members;for(auto id:s.members)members.push_back(buttons.at(id).get());
        part.SetHUDStateForAnalysis(std::move(members),buttons.at(s.initial).get(),buttons.at(s.selected).get(),s.key,s.layer,
            {static_cast<std::uint8_t>(s.flags[0]),static_cast<std::uint8_t>(s.flags[1]),static_cast<std::uint8_t>(s.flags[2])});
    }
    void OriginalReferences(Reader& r)
    { for(unsigned i=0;i<6;++i)(void)r.U32();originalOnlyReferenceScalars+=6; }
    void Basics()
    {
        Host host;spSubscriptionManager manager;wxHUDPart::SetFactoryContextForAnalysis(&host,&manager);
        auto object=spRTTIManager::Instance().Create(wxHUDPart::ClassID);auto* part=dynamic_cast<wxHUDPart*>(object.get());
        Check(part&&part->GetField20ForAnalysis()==1,"HUDPart concrete factory and Entity type1");
        Check(part->GetButtonsForAnalysis().empty()&&part->GetFlagsForAnalysis()==std::array<std::uint8_t,3>{0,0,0},"HUDPart own constructor state");
        Check(!part->GetInitialButtonForAnalysis()&&!part->GetSelectedButtonForAnalysis()
            &&part->GetPartKeyForAnalysis()==0&&part->GetLayerWordForAnalysis()==0,"HUDPart own pointer and word defaults");
        spNotificationForAnalysis n;n.code=0x1c;object->vfunc_0C(&n);
        Check(host.unresolved==1&&part->GetFlagsForAnalysis()[2]==0,"Unknown cold init reports boundary without success flag");
        spCloneManager clones;bool refused=false;try{(void)part->vfunc_14(*part,clones);}catch(const std::logic_error&){refused=true;}
        Check(refused&&part->GetButtonsForAnalysis().empty(),"Self-copy has an explicit unqualified-domain refusal");
        wxHUDPart::SetFactoryContextForAnalysis(nullptr,nullptr);
    }
    void Core(Reader& r,unsigned count)
    {
        for(unsigned caseIndex=0;caseIndex<count;++caseIndex)
        {
            const auto label=r.Text();std::array<unsigned,7> c;for(auto& value:c)value=r.U32();
            std::array<std::array<unsigned,3>,4> data;for(auto& values:data)for(auto& value:values)value=r.U32();
            const auto senders=r.Vector();std::array<std::string,3> actions;for(auto& action:actions)action=r.Text();
            const auto queryCount=r.U32();Check(queryCount<=32,"HUDPart query count bound");std::vector<std::string> queries;for(unsigned i=0;i<queryCount;++i)queries.push_back(r.Text());
            std::array<unsigned,5> mutation;for(auto& value:mutation)value=r.U32();
            const auto expectedPart20=r.U32(),expectedBase20=r.U32();
            Host host;spSubscriptionManager subscriptions;std::array<std::unique_ptr<spNode>,7> nodes;
            for(unsigned i=1;i<7;++i){nodes[i]=std::make_unique<spNode>();host.nodeIDs[nodes[i].get()]=i;host.refs[nodes[i].get()]=2;}
            auto buttons=Buttons(host);ObservedPart part(host,subscriptions);wxEntity base(host);
            part.SetHUDStateForAnalysis({},nullptr,nullptr,c[1],0,{0,0,static_cast<std::uint8_t>(c[6])});
            for(unsigned i=1;i<4;++i)
            {
                auto& button=*buttons[i];std::memcpy(button.GetOwnBytesForAnalysis().data(),&data[0][i-1],4);
                button.SetButtonStateForAnalysis(actions[i-1],0,nodes[5].get());button.spEntity::SetReferenceForAnalysis(nodes[1].get());
                button.GetOwnBytesForAnalysis()[0x144]=static_cast<std::uint8_t>(data[3][i-1]);
                button.SetSelectionStateForAnalysis(static_cast<std::uint8_t>(data[1][i-1]),1,0,nodes[2].get(),{nodes[2].get(),nodes[3].get(),nodes[4].get()},data[2][i-1]);
            }
            Check(subscriptions.SubscribeForAnalysis(3,part),"Real common manager subscribes actual Part");
            std::vector<std::vector<unsigned>> history;
            for(auto sender:senders)
            {
                if(c[4])
                {
                    spNotificationForAnalysis incoming;incoming.code=0x1c;incoming.payload=1;
                    buttons.at(sender)->vfunc_0C(&incoming);
                    Check(host.unresolved==0,"Current wxButton own producer and base fallback complete");
                }
                else Check(SendBroadcastNotificationForAnalysis(buttons.at(sender).get(),c[5],3,0,0)==spBroadcastNotificationResultForAnalysis::Completed,
                    "Current shared broadcast helper dispatches actual Part");
                history.push_back(host.Snapshot(part).members);
            }
            std::vector<unsigned> finds;for(const auto& query:queries)finds.push_back(host.ID(part.FindButtonForAnalysis(query.c_str())));
            // Same explicit post-producer populated input as the original probe.
            for(unsigned i=1;i<4;++i)
            {
                buttons[i]->SetSelectionStateForAnalysis(static_cast<std::uint8_t>(data[1][i-1]),1,0,nodes[2].get(),
                    {nodes[2].get(),nodes[3].get(),nodes[4].get()},data[2][i-1]);
                buttons[i]->SetButtonStateForAnalysis(actions[i-1],buttons[i]->GetPressedByteForAnalysis(),nodes[5].get());
            }
            part.SetHUDStateForAnalysis(part.GetButtonsForAnalysis(),buttons.at(c[2]).get(),nullptr,c[1],0,{0,0,static_cast<std::uint8_t>(c[6])});
            bool mutated=false;host.beforeEnable=[&]
            {
                if(mutation[0]&&!mutated&&host.enables.size()==mutation[0])
                {
                    if(mutation[1])
                    {
                        auto& button=*buttons.at(mutation[1]);button.SetSelectionStateForAnalysis(button.GetEnabledByteForAnalysis(),button.GetVisibleByteForAnalysis(),
                            button.GetHighlightByteForAnalysis(),button.GetSelectedNodeForAnalysis(),button.GetStateNodesForAnalysis(),mutation[2]);
                    }
                    if(mutation[4])part.GetButtonsForAnalysis().at(mutation[3])=buttons.at(mutation[4]).get();
                    mutated=true;
                }
            };
            if(c[0]==2){host.active=true;part.SelectButtonForAnalysis(buttons.at(c[3]).get());host.active=false;}
            Match(part.GetField20ForAnalysis(),expectedPart20,"HUDPart fresh original Entity type");Match(base.GetField20ForAnalysis(),expectedBase20,"wxEntity registered default3");
            auto n=r.U32();Match(static_cast<unsigned>(host.messages.size()),n,"HUDPart actual broadcast receiver frame count");
            for(unsigned i=0;i<n;++i)for(unsigned k=0;k<8;++k)Match(host.messages.at(i)[k],r.U32(),"HUDPart shared broadcast8word frame");
            n=r.U32();Match(static_cast<unsigned>(history.size()),n,"HUDPart notification history count");
            for(unsigned i=0;i<n;++i)MatchVector(history.at(i),r.Vector(),"HUDPart matching/duplicate/encounter history");
            MatchVector(host.Snapshot(part).members,r.Vector(),"HUDPart final members");MatchVector(finds,r.Vector(),"HUDPart first exact lookup");
            const auto state=host.Snapshot(part);for(auto value:{state.initial,state.selected,state.key,state.layer,state.flags[0],state.flags[1],state.flags[2]})Match(value,r.U32(),label.c_str());
            for(unsigned i=1;i<4;++i)
            {
                const auto& button=*buttons[i];for(auto value:{unsigned(button.GetEnabledByteForAnalysis()),unsigned(button.GetHighlightByteForAnalysis()),button.GetSelectionIndexForAnalysis(),host.NodeID(button.GetSelectedNodeForAnalysis())})
                    Match(value,r.U32(),"HUDPart actual wxButton state");
            }
            n=r.U32();Match(static_cast<unsigned>(host.calls.size()),n,"HUDPart captured state/refresh call count");
            for(unsigned i=0;i<n;++i)for(unsigned k=0;k<3;++k)Match(host.calls.at(i)[k],r.U32(),"HUDPart captured virtual recipient");
            n=r.U32();Match(static_cast<unsigned>(host.enables.size()),n,"HUDPart actual Node enable count");
            for(unsigned i=0;i<n;++i)for(unsigned k=0;k<3;++k)Match(host.enables.at(i)[k],r.U32(),"HUDPart actual Node enable call");
            for(unsigned i=1;i<7;++i)Match(nodes[i]->GetFlagsForAnalysis(),r.U32(),"HUDPart composed Node flags");
            OriginalReferences(r);Check(host.unresolved==0,"Qualified HUDPart path has no unknown init or GUI Notify");
        }
    }
    void Copy(Reader& r,unsigned count)
    {
        for(unsigned caseIndex=0;caseIndex<count;++caseIndex)
        {
            (void)r.U32();const auto clone=r.U32()!=0;const auto sourceInput=r.OwnState();const auto hasBefore=r.U32()!=0;
            State before{};if(hasBefore)before=r.OwnState();const auto expected=r.OwnState();const auto returnValue=r.U32(),preserved=r.U32();const auto ids=r.Vector();
            Host host;spSubscriptionManager subscriptions;auto buttons=Buttons(host);auto source=std::make_unique<wxHUDPart>(host,subscriptions);Apply(*source,sourceInput,buttons);
            const auto snapshot=host.Snapshot(*source);std::unique_ptr<wxHUDPart> target;spCloneManager manager;unsigned result=1;
            if(clone)
            {
                auto object=source->vfunc_10(manager);auto* part=dynamic_cast<wxHUDPart*>(object.get());Check(part!=nullptr,"HUDPart clone retains concrete type");object.release();target.reset(part);
            }
            else { target=std::make_unique<wxHUDPart>(host,subscriptions);Apply(*target,before,buttons);result=source->vfunc_14(*target,manager)?1:0; }
            Match(result,returnValue,"HUDPart original copy success");MatchState(host.Snapshot(*target),expected);
            const auto after=host.Snapshot(*source);MatchState(after,sourceInput);
            Match(after.members==snapshot.members&&after.initial==snapshot.initial&&after.selected==snapshot.selected&&after.flags==snapshot.flags?1:0,preserved,"HUDPart source preserved");
            Check(ids==std::vector<unsigned>{spNamedObject::ClassID,wxEntity::ClassID,wxHUDPart::ClassID},
                "Retain exact original full ancestor completion trace");
            // NamedObject's external completion is not represented by its
            // current common bounded copy. Never synthesize a source callback
            // to claim parity for that unrelated ancestor operation.
            ++originalOnlyNamedCompletions;
            MatchVector(host.copyIDs,{ids[1],ids[2]},"HUDPart own wxEntity-to-Part completion order");
            OriginalReferences(r);Check(host.buttonDeletes==0,"Borrowed copy does not delete buttons");
        }
    }
    void Prefix(Reader& r,unsigned count)
    {
        for(unsigned caseIndex=0;caseIndex<count;++caseIndex)
        {
            (void)r.U32();const auto before=r.OwnState(),after=r.OwnState();const auto unsubCount=r.U32();Check(unsubCount==1,"Original prefix one unsubscribe");
            Check(r.U32()==3&&r.U32()==1,"Original prefix unsubscribe key3/current Part");
            const auto freed=r.U32(),borrowed=r.U32(),partNotDeleted=r.U32(),noHUD=r.U32();Check(partNotDeleted&&noHUD,"Original prefix stops before lazy HUD startup");
            Host host;spSubscriptionManager subscriptions;auto buttons=Buttons(host);auto part=std::make_unique<wxHUDPart>(host,subscriptions);Apply(*part,before,buttons);
            Check(subscriptions.SubscribeForAnalysis(3,*part),"Prefix real manager subscription");host.tearingPart=part.get();part.reset();host.tearingPart=nullptr;
            MatchState(host.prefixState,after);Match(host.storageReleased?1:0,freed,"HUDPart vector storage released before foreign boundary");
            Match(host.buttonDeletes==0?1:0,borrowed,"HUDPart borrowed buttons remain alive");
            Check(subscriptions.GetSubscriberCountForAnalysis(3)==0,"Part destructor invokes actual common unsubscribe");
            Check(host.foreignHUDRemoves==0&&host.unresolved==1,"Prefix resolve explicitly refuses unqualified foreign startup");
        }
    }
}
int main(int argc,char** argv)
{
    try
    {
        Basics();
        if(argc==3&&std::string(argv[1])=="--fixture")
        {
            Reader r(argv[2]);char magic[4];r.input.read(magic,4);Check(std::memcmp(magic,"HUP1",4)==0&&r.U32()==1,"HUDPart fixture header");
            const auto mode=r.U32(),count=r.U32();Check(count<=128,"HUDPart case bound");
            if(mode==0)Core(r,count);else if(mode==1)Copy(r,count);else if(mode==2)Prefix(r,count);else throw std::logic_error("Unknown HUDPart fixture mode");r.End();
        }
        else Check(argc==1,"Usage: --fixture original.bin");
        std::cout<<"PASS "<<checks<<" HUDPart checks, "<<nativeChecks<<" current original scalar comparisons; "
            <<originalOnlyLayoutScalars<<" native layout scalars and "<<originalOnlyReferenceScalars<<" native refcount scalars original-only; "
            <<originalOnlyNamedCompletions<<" ancestor NamedCompletion events original-only\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<"FAIL HUDPart: "<<e.what()<<'\n';return 1;}
}
