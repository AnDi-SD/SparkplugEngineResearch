#include "Code/wxGhoulScriptAIAction.h"
#include "Analysis/PC/wxGhoulScriptAIActionAbi.h"
#include "Code/Sparkplug/spNode.h"
#include <array>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

using namespace sparkplug::reconstruction;
using namespace winx::reconstruction;
namespace
{
    void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
    using Command = std::array<std::uint8_t, 0x74>;
    struct Entity { spNode* node = nullptr; };
    struct Actor { Command* command = nullptr; };
    struct Owner { Entity* entity = nullptr; Actor* actor = nullptr; std::uint8_t flag = 0; };
    std::string Hex(const Command& bytes)
    { constexpr char digits[]="0123456789abcdef";std::string out;for(auto b:bytes){out+=digits[b>>4];out+=digits[b&15];}return out; }
    std::string Array(const std::vector<std::string>& values)
    { std::ostringstream out;out<<'[';for(std::size_t i=0;i<values.size();++i){if(i)out<<',';out<<values[i];}out<<']';return out.str(); }
    struct Host final : wxGhoulScriptAIActionHost
    {
        std::array<spNode,2> nodes;
        std::array<Command,2> commands;
        std::array<Entity,2> entities;
        std::array<Actor,2> actors;
        std::array<Owner,2> owners;
        wxGhoulScriptAIAction* action = nullptr;
        bool admitted = true, mutated = false;
        unsigned mask = 0, ordinal = 0, nodeReads = 0, commandReads = 0, ownerWrites = 0;
        spNode* lastNodeRead = nullptr;
        void* lastReadOwner = nullptr;
        std::vector<std::string> mutations, logicalReads, logicalWrites;
        Host(unsigned flag, unsigned node, unsigned command)
        {
            for(unsigned i=0;i<2;++i)
            { commands[i].fill(static_cast<std::uint8_t>(i?0xE7:0xD7));entities[i].node=&nodes[i];actors[i].command=&commands[i];owners[i]={&entities[i],&actors[i],static_cast<std::uint8_t>(i?0xF1:flag)}; }
            entities[0].node=node?&nodes[node-1]:nullptr;actors[0].command=command?&commands[command-1]:nullptr;
        }
        ProfileForAnalysis EntryProfileForAnalysis() const noexcept override
        { return admitted?ProfileForAnalysis::PcCapturedBorrowedEntryData:ProfileForAnalysis::Unqualified; }
        template<class T,std::size_t N> static unsigned ID(const std::array<T,N>& values,const void* value)
        { if(!value)return 0;for(unsigned i=0;i<N;++i)if(value==&values[i])return i+1;throw std::runtime_error("Live supplied identity"); }
        std::string Bindings() const
        {
            std::ostringstream out;out<<"{\"actionOwner\":"<<ID(owners,action->GetOwnerForAnalysis())
                <<",\"ownerEntities\":["<<ID(entities,owners[0].entity)<<','<<ID(entities,owners[1].entity)
                <<"],\"ownerActors\":["<<ID(actors,owners[0].actor)<<','<<ID(actors,owners[1].actor)
                <<"],\"entityNodes\":["<<ID(nodes,entities[0].node)<<','<<ID(nodes,entities[1].node)
                <<"],\"actorCommands\":["<<ID(commands,actors[0].command)<<','<<ID(commands,actors[1].command)<<"]}";return out.str();
        }
        std::string Snapshot() const
        {
            const auto& s=action->CapturedStateForAnalysis();std::ostringstream out;
            out<<"{\"bindings\":"<<Bindings()<<",\"own\":{\"state3A8\":"<<s.state3A8<<",\"word3AC\":"<<s.word3AC
                <<",\"Node3B0\":"<<ID(nodes,s.borrowedNode3B0)<<",\"command3B8\":"<<ID(commands,s.borrowedCommand3B8)
                <<",\"duration370\":"<<action->GetDurationMillisecondsForAnalysis()<<",\"base374\":"<<action->GetClearableWordForAnalysis()
                <<"},\"ownerFlags\":["<<unsigned(owners[0].flag)<<','<<unsigned(owners[1].flag)
                <<"],\"commandBytes\":[\""<<Hex(commands[0])<<"\",\""<<Hex(commands[1])<<"\"]}";return out.str();
        }
        std::optional<spNode*> ReadOwnerNodeForAnalysis(void* value) noexcept override
        {
            ++nodeReads;const auto id=ID(owners,value);Check(id!=0,"Live Node-read owner");lastReadOwner=value;
            logicalReads.push_back("{\"kind\":\"Node\",\"owner\":"+std::to_string(id)+'}');
            auto* entity=owners[id-1].entity;if(!entity)return std::nullopt;lastNodeRead=entity->node;return lastNodeRead;
        }
        std::optional<void*> ReadCommandForAnalysis(void* value) noexcept override
        {
            ++commandReads;Check(value==lastReadOwner,"Entry retains the same owner between Node and command reads");
            Check(action->CapturedStateForAnalysis().borrowedNode3B0==lastNodeRead,"Node cache precedes ReadCommand start");
            if(mask&&!mutated)
            {
                Check(ordinal==0,"Mutation is declared in first Entry only");const auto before=Bindings();
                if(mask&1)action->SetOwnerForAnalysis(&owners[1]);
                if(mask&2)owners[0].actor=&actors[1];
                if(mask&4)actors[0].command=&commands[1];
                if(mask&8)owners[0].entity=&entities[1];
                mutated=true;std::ostringstream out;out<<"{\"call\":"<<ordinal<<",\"point\":\"ReadCommand-start-after-Node-cache\",\"mask\":"<<mask
                    <<",\"cachedOwner\":"<<ID(owners,value)<<",\"before\":"<<before<<",\"after\":"<<Bindings()<<'}';mutations.push_back(out.str());
            }
            const auto id=ID(owners,value);Check(id!=0,"Live command-read owner");
            logicalReads.push_back("{\"kind\":\"command\",\"owner\":"+std::to_string(id)+",\"NodeAlreadyCached\":"+std::to_string(ID(nodes,lastNodeRead))+'}');
            auto* actor=owners[id-1].actor;if(!actor)return std::nullopt;return static_cast<void*>(actor->command);
        }
        void WriteOwnerByteForAnalysis(void* value,std::uint32_t offset,std::uint8_t byte) noexcept override
        {
            Check(value==lastReadOwner&&offset==0x14D&&byte==0,"Action chooses cached-owner14D byte0");
            const auto id=ID(owners,value);Check(id!=0,"Live owner write");++ownerWrites;owners[id-1].flag=byte;
            logicalWrites.push_back("{\"owner\":"+std::to_string(id)+",\"offset\":333,\"size\":1,\"value\":0}");
        }
    };
    struct Fixture
    {
        Host host;
        std::unique_ptr<wxGhoulScriptAIAction> action;
        Fixture(std::uint32_t seed=0xFFFFFFFF,unsigned flag=165,unsigned node=1,unsigned command=1,std::uint32_t opaque=0)
            :host(flag,node,command)
        {
            const wxGhoulScriptCapturedStateForAnalysis state{seed,seed^0xA5A5A5A5u,&host.nodes[1],opaque,&host.commands[1]};
            action=std::make_unique<wxGhoulScriptAIAction>(host,&host.owners[0],state);host.action=action.get();
            action->SetDurationMillisecondsForAnalysis(seed);action->SetClearableWordForAnalysis(0xDEADBEEF);
        }
        ~Fixture(){action.reset();host.action=nullptr;}
    };
    void One(unsigned calls,std::uint32_t seed,unsigned flag,std::uint32_t argument,unsigned node,unsigned command,unsigned mutation,unsigned rebind,bool emit)
    {
        Check(calls>=1&&calls<=2&&flag<256&&node<=2&&command<=2&&mutation<16&&rebind<=2,"Bounded eight replay inputs");
        // Native ret4 consumes this unused argument; portable Entry is void.
        (void)argument;Fixture fixture(seed,flag,node,command);auto& host=fixture.host;auto& action=*fixture.action;host.mask=mutation;
        std::vector<std::string> steps;
        for(unsigned ordinal=0;ordinal<calls;++ordinal)
        {
            host.ordinal=ordinal;
            if(ordinal==1&&rebind)
            {
                const auto before=host.Bindings();action.SetOwnerForAnalysis(&host.owners[rebind-1]);
                host.mutations.push_back("{\"call\":1,\"point\":\"explicit-before-call2\",\"owner\":"+std::to_string(rebind)+",\"before\":"+before+",\"after\":"+host.Bindings()+'}');
            }
            const auto before=host.Snapshot();const auto cached=Host::ID(host.owners,action.GetOwnerForAnalysis());
            auto* owner=&host.owners[cached-1];auto* expectedNode=owner->entity->node;
            const auto firstRead=host.logicalReads.size(),firstWrite=host.logicalWrites.size();
            action.vfunc_30();Check(action.LastResultForAnalysis()==wxGhoulScriptResultForAnalysis::Completed,"Admitted whole Entry completes");
            const auto& s=action.CapturedStateForAnalysis();Check(s.borrowedNode3B0==expectedNode&&s.borrowedCommand3B8==owner->actor->command,"Cached owner/order determine Node and command");
            Check(s.state3A8==0&&s.word3AC==0&&s.opaqueNativeWord3B4==0&&owner->flag==0&&action.GetDurationMillisecondsForAnalysis()==15000&&action.GetClearableWordForAnalysis()==0xDEADBEEF,"Own fields, byte14D, real duration setter and untouched374");
            Check(host.logicalReads.size()-firstRead==2&&host.logicalWrites.size()-firstWrite==1,"Two storage reads and one owner-byte write, not native instruction counts");
            std::ostringstream out;out<<"{\"ordinal\":"<<ordinal<<",\"before\":"<<before<<",\"after\":"<<host.Snapshot()<<'}';steps.push_back(out.str());
        }
        Check(host.mutated==bool(mutation),"Exactly declared mutation executes");
        if(emit)std::cout<<"{\"steps\":"<<Array(steps)<<",\"logicalReads\":"<<Array(host.logicalReads)<<",\"logicalOwnerWrites\":"<<Array(host.logicalWrites)<<",\"inputMutations\":"<<Array(host.mutations)<<"}\n";
    }
    void Replay()
    {
        unsigned calls,flag,node,command,mutation,rebind;std::uint32_t seed,argument;
        while(std::cin>>calls>>seed>>flag>>argument>>node>>command>>mutation>>rebind)One(calls,seed,flag,argument,node,command,mutation,rebind,true);
        Check(std::cin.eof(),"Complete GhoulScript input record");
    }
    void Contracts()
    {
        static_assert(!std::is_copy_constructible_v<wxGhoulScriptAIAction> && !std::is_move_constructible_v<wxGhoulScriptAIAction>);
        static_assert(sizeof(winx::evidence::pc::wxGhoulScriptAIActionLayout)==0x3BC);
        One(2,0xFFFFFFFF,255,0xFFFFFFFF,1,1,15,1,false);One(1,0,0,0,0,0,0,0,false);
        {
            Fixture f;const auto before=f.host.Snapshot();f.host.admitted=false;
            Check(f.action->EnterForAnalysis()==wxGhoulScriptResultForAnalysis::UnqualifiedProfile&&f.host.nodeReads==0&&before==f.host.Snapshot(),"Default profile refuses before effects");
            f.host.admitted=true;f.action->SetOwnerForAnalysis(nullptr);
            Check(f.action->EnterForAnalysis()==wxGhoulScriptResultForAnalysis::MissingOwner&&f.host.nodeReads==0,"Missing owner preeffect refusal");
        }
        {
            Fixture f(7,128,1,1,1);const auto before=f.host.Snapshot();
            Check(f.action->EnterForAnalysis()==wxGhoulScriptResultForAnalysis::UnqualifiedProfile&&f.host.nodeReads==0&&before==f.host.Snapshot(),"Nonzero captured opaque3B4 refuses without inventing meaning");
        }
        {
            Fixture f;f.host.owners[0].entity=nullptr;const auto* old=f.action->CapturedStateForAnalysis().borrowedNode3B0;
            Check(f.action->EnterForAnalysis()==wxGhoulScriptResultForAnalysis::MissingEntity&&f.action->CapturedStateForAnalysis().borrowedNode3B0==old&&f.host.commandReads==0&&f.host.ownerWrites==0,"Missing entity is distinct from admitted null Node payload");
        }
        {
            Fixture f;f.host.owners[0].actor=nullptr;
            Check(f.action->EnterForAnalysis()==wxGhoulScriptResultForAnalysis::MissingActor,"Missing actor is distinct from admitted null command payload");
            const auto& s=f.action->CapturedStateForAnalysis();Check(s.borrowedNode3B0==&f.host.nodes[0]&&s.borrowedCommand3B8==&f.host.commands[1]&&s.state3A8==0xFFFFFFFF&&f.host.owners[0].flag==165&&f.action->GetDurationMillisecondsForAnalysis()==0xFFFFFFFF&&f.host.ownerWrites==0,"Analytical partial refusal retains first Node cache, no rollback/fault claim");
        }
        {
            struct DerivedNode final:spNode{};DerivedNode derived;Fixture f;f.host.entities[0].node=&derived;
            Check(f.action->EnterForAnalysis()==wxGhoulScriptResultForAnalysis::UnqualifiedNodeType&&f.action->CapturedStateForAnalysis().borrowedNode3B0==&f.host.nodes[1]&&f.host.commandReads==0,"Exact plain Node type guard before cache/command read");
        }
        {
            wxAIAction other;Fixture f;const auto before=f.host.Snapshot();f.action->SetCurrentActionForAnalysis(&other);
            Check(f.action->EnterForAnalysis()==wxGhoulScriptResultForAnalysis::UnqualifiedProfile&&f.host.nodeReads==0,"Current-action graph refusal");f.action->SetCurrentActionForAnalysis(nullptr);
            f.action->AddOwnedActionForAnalysis(std::make_unique<spBaseObject>());
            Check(f.action->EnterForAnalysis()==wxGhoulScriptResultForAnalysis::UnqualifiedProfile&&f.host.nodeReads==0&&before==f.host.Snapshot(),"Owned-action graph refuses without consuming");
        }
        {
            Fixture f;const auto before=f.host.Snapshot();f.action->vfunc_0C(nullptr);Check(f.action->LastResultForAnalysis()==wxGhoulScriptResultForAnalysis::UnqualifiedNotify,"Notify refusal");
            Check(!f.action->vfunc_24()&&!f.action->vfunc_28()&&!f.action->vfunc_2C()&&!f.action->vfunc_3C(nullptr)&&!f.action->vfunc_40(nullptr),"Other bool hooks have analytical refusal");
            f.action->vfunc_34_ClearForAnalysis();f.action->vfunc_38();Check(f.action->LastResultForAnalysis()==wxGhoulScriptResultForAnalysis::UnqualifiedHook,"Other void hooks report refusal");
            spCloneManager clones;Check(!f.action->vfunc_14(*f.action,clones),"Copy refusal");bool refused=false;
            try{(void)f.action->vfunc_10(clones);}catch(const std::logic_error&){refused=true;}Check(refused,"Clone refusal");
            refused=false;try{(void)spRTTIManager::Instance().Create(wxGhoulScriptAIAction::ClassID);}catch(const std::logic_error&){refused=true;}Check(refused,"Generic factory needs explicit postconstruction capture");
            Check(f.action->vfunc_18().IsExactly(wxGhoulScriptAIAction::ClassID)&&f.action->vfunc_18().IsKindOf(wxAIAction::ClassID)&&before==f.host.Snapshot()&&f.host.nodeReads==0,"Typed identity and unknown operations preserve selected game fields");
        }
        {
            Fixture f;f.action->vfunc_30();const auto commands=f.host.commands;const auto writes=f.host.ownerWrites;f.action.reset();f.host.action=nullptr;
            Check(commands==f.host.commands&&f.host.ownerWrites==writes&&f.host.nodes[0].vfunc_18().IsExactly(spNode::ClassID),"Borrowed controls stay live after action destruction; no dead action reads");
        }
    }
}
int main(int argc,char** argv)
{
    try{if(argc==2&&std::string(argv[1])=="--replay")Replay();else{Contracts();std::cout<<"GhoulScript bounded Entry contracts passed\n";}return 0;}
    catch(const std::exception& error){std::cerr<<"GhoulScript "<<error.what()<<'\n';return 1;}
}
