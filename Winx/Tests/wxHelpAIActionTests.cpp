#include "Code/wxHelpAIAction.h"
#include "Analysis/PC/wxHelpAIActionAbi.h"
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
    struct Actor { Command* command = nullptr; };
    struct Owner { Actor* actor = nullptr; };
    template<class T> std::string Hex(const T& bytes)
    {
        constexpr char digits[] = "0123456789abcdef"; std::string out;
        for (auto b : bytes) { out += digits[b >> 4]; out += digits[b & 15]; }
        return out;
    }
    std::string Array(const std::vector<std::string>& values)
    {
        std::ostringstream out; out << '[';
        for (std::size_t i=0; i<values.size(); ++i) { if(i) out << ','; out << values[i]; }
        out << ']'; return out.str();
    }
    wxHelpAIActionCapturedStateForAnalysis Decode(const std::string& text)
    {
        wxHelpAIActionCapturedStateForAnalysis state;
        Check(text.size()==state.ownBytes.size()*2,"Exact captured own-tail bytes");
        for(std::size_t i=0;i<state.ownBytes.size();++i)
            state.ownBytes[i]=static_cast<std::uint8_t>(std::stoul(text.substr(i*2,2),nullptr,16));
        return state;
    }
    struct Host final : wxHelpAIActionHost
    {
        std::array<Command,2> commands;
        std::array<Actor,2> actors;
        std::array<Owner,2> owners;
        wxHelpAIAction* action = nullptr;
        bool admitted = true, mutated = false;
        unsigned mask=0, ordinal=0, commandReads=0, commandWrites=0;
        std::array<std::uint8_t,0xC0> beforeCommandWrite{};
        std::vector<std::string> reads,writes,mutations;
        Host(unsigned fill,unsigned command)
        {
            commands[0].fill(static_cast<std::uint8_t>(fill));commands[1].fill(0xE7);
            for(unsigned i=0;i<2;++i){actors[i].command=&commands[i];owners[i].actor=&actors[i];}
            actors[0].command=&commands[command-1];
        }
        ProfileForAnalysis EntryProfileForAnalysis() const noexcept override
        { return admitted?ProfileForAnalysis::PcCapturedBorrowedEntryData:ProfileForAnalysis::Unqualified; }
        template<class T,std::size_t N> static unsigned ID(const std::array<T,N>& values,const void* value)
        {if(!value)return 0;for(unsigned i=0;i<N;++i)if(value==&values[i])return i+1;throw std::runtime_error("Live supplied identity");}
        std::string Bindings() const
        {
            std::ostringstream out;out<<"{\"actionOwner\":"<<ID(owners,action->GetOwnerForAnalysis())
                <<",\"ownerActors\":["<<ID(actors,owners[0].actor)<<','<<ID(actors,owners[1].actor)
                <<"],\"actorCommands\":["<<ID(commands,actors[0].command)<<','<<ID(commands,actors[1].command)<<"]}";
            return out.str();
        }
        std::string Snapshot() const
        {
            std::ostringstream out;out<<"{\"bindings\":"<<Bindings()<<",\"ownTail\":\""<<Hex(action->CapturedStateForAnalysis().ownBytes)
                <<"\",\"duration370\":"<<action->GetDurationMillisecondsForAnalysis()<<",\"base374\":"<<action->GetClearableWordForAnalysis()
                <<",\"commandBytes\":[\""<<Hex(commands[0])<<"\",\""<<Hex(commands[1])<<"\"]}";
            return out.str();
        }
        std::optional<void*> ReadCommandForAnalysis(void* owner) noexcept override
        {
            ++commandReads;const auto id=ID(owners,owner);Check(id!=0,"Live command-read owner");
            auto* actor=owners[id-1].actor;
            if(!actor)return std::nullopt;
            auto* command=actor->command;
            reads.push_back("{\"owner\":"+std::to_string(id)+",\"command\":"+std::to_string(ID(commands,command))+'}');
            return static_cast<void*>(command);
        }
        void WriteCommandWordForAnalysis(void* command,std::uint32_t offset,std::uint32_t value) noexcept override
        {
            Check(offset==4&&value==0,"Action chooses command+4 word0");
            Check(action->CapturedStateForAnalysis().ownBytes==beforeCommandWrite,"Command write precedes own byte/word stores");
            const auto id=ID(commands,command);Check(id!=0,"Live captured command destination");
            if(mask&&!mutated)
            {
                Check(ordinal==0,"Only declared first-call mutation");const auto before=Bindings();
                if(mask&1)action->SetOwnerForAnalysis(&owners[1]);
                if(mask&2)owners[0].actor=&actors[1];
                if(mask&4)actors[0].command=&commands[1];
                mutated=true;std::ostringstream out;
                out<<"{\"call\":"<<ordinal<<",\"point\":\"before-command-store-after-cached-load\",\"mask\":"<<mask
                    <<",\"cachedCommand\":"<<id<<",\"before\":"<<before<<",\"after\":"<<Bindings()<<'}';
                mutations.push_back(out.str());
            }
            ++commandWrites;
            for(unsigned i=0;i<4;++i)commands[id-1][offset+i]=0;
            writes.push_back("{\"command\":"+std::to_string(id)+",\"offset\":4,\"size\":4,\"value\":0}");
        }
    };
    struct Fixture
    {
        Host host;
        std::unique_ptr<wxHelpAIAction> action;
        Fixture(const wxHelpAIActionCapturedStateForAnalysis& state,unsigned fill=215,unsigned command=1):host(fill,command)
        {
            action=std::make_unique<wxHelpAIAction>(host,&host.owners[0],state);host.action=action.get();
            action->SetDurationMillisecondsForAnalysis(0x12345678);action->SetClearableWordForAnalysis(0xDEADBEEF);
        }
        ~Fixture(){action.reset();host.action=nullptr;}
    };
    void One(unsigned calls,unsigned fill,unsigned command,unsigned mutation,unsigned rebind,std::uint32_t argument,
        const wxHelpAIActionCapturedStateForAnalysis& state,bool emit)
    {
        Check(calls>=1&&calls<=2&&fill<256&&command>=1&&command<=2&&mutation<8&&rebind<=2,"Bounded replay inputs");
        (void)argument;Fixture f(state,fill,command);auto& host=f.host;auto& action=*f.action;host.mask=mutation;
        std::vector<std::string> steps;
        for(unsigned ordinal=0;ordinal<calls;++ordinal)
        {
            host.ordinal=ordinal;
            if(ordinal==1&&rebind)
            {
                const auto before=host.Bindings();action.SetOwnerForAnalysis(&host.owners[rebind-1]);
                host.mutations.push_back("{\"call\":1,\"point\":\"explicit-before-call2\",\"owner\":"+std::to_string(rebind)+",\"before\":"+before+",\"after\":"+host.Bindings()+'}');
            }
            const auto before=host.Snapshot();auto expected=action.CapturedStateForAnalysis().ownBytes;host.beforeCommandWrite=expected;
            const auto readCount=host.commandReads,writeCount=host.commandWrites;
            action.vfunc_30();Check(action.LastResultForAnalysis()==wxHelpAIActionResultForAnalysis::Completed,"Admitted whole Entry completes");
            expected[4]=0;for(unsigned i=0xB8;i<0xBC;++i)expected[i]=0;
            Check(action.CapturedStateForAnalysis().ownBytes==expected,"Exactly byte3AC and word460 change in captured tail");
            Check(action.GetDurationMillisecondsForAnalysis()==0x12345678&&action.GetClearableWordForAnalysis()==0xDEADBEEF,"Base duration/word374 unchanged");
            Check(host.commandReads==readCount+1&&host.commandWrites==writeCount+1,"One logical command read/write, not native instruction count");
            std::ostringstream out;out<<"{\"ordinal\":"<<ordinal<<",\"before\":"<<before<<",\"after\":"<<host.Snapshot()<<'}';steps.push_back(out.str());
        }
        Check(host.mutated==bool(mutation),"Declared mutation executed exactly once");
        if(emit)std::cout<<"{\"steps\":"<<Array(steps)<<",\"logicalCommandReads\":"<<Array(host.reads)
            <<",\"logicalCommandWrites\":"<<Array(host.writes)<<",\"inputMutations\":"<<Array(host.mutations)<<"}\n";
    }
    void Replay()
    {
        unsigned calls,fill,command,mutation,rebind;std::uint32_t argument;std::string tail;
        while(std::cin>>calls>>fill>>command>>mutation>>rebind>>argument>>tail)
            One(calls,fill,command,mutation,rebind,argument,Decode(tail),true);
        Check(std::cin.eof(),"Complete Help replay input record");
    }
    void Contracts()
    {
        static_assert(!std::is_copy_constructible_v<wxHelpAIAction>&&!std::is_move_constructible_v<wxHelpAIAction>);
        static_assert(sizeof(winx::evidence::pc::wxHelpAIActionLayout)==0x468);
        wxHelpAIActionCapturedStateForAnalysis state;state.ownBytes.fill(0xCC);
        One(2,215,1,7,1,0xFFFFFFFF,state,false);One(1,0,2,0,0,0,state,false);
        {
            Fixture f(state);const auto before=f.host.Snapshot();f.host.admitted=false;
            Check(f.action->EnterForAnalysis()==wxHelpAIActionResultForAnalysis::UnqualifiedProfile&&f.host.commandReads==0&&f.host.Snapshot()==before,"Unqualified profile refuses before effects");
            f.host.admitted=true;f.action->SetOwnerForAnalysis(nullptr);
            Check(f.action->EnterForAnalysis()==wxHelpAIActionResultForAnalysis::MissingOwner&&f.host.commandReads==0,"Missing owner refusal before storage read");
        }
        for(bool actorMissing:{false,true})
        {
            Fixture f(state);
            if(actorMissing)f.host.owners[0].actor=nullptr;else f.host.actors[0].command=nullptr;
            const auto before=f.host.Snapshot();
            Check(f.action->EnterForAnalysis()==wxHelpAIActionResultForAnalysis::MissingActorOrCommand&&f.host.commandWrites==0&&f.host.Snapshot()==before,"Missing actor/null command refuses without writes, not native fault proof");
        }
        {
            wxAIAction other;Fixture f(state);f.action->SetCurrentActionForAnalysis(&other);
            Check(f.action->EnterForAnalysis()==wxHelpAIActionResultForAnalysis::UnqualifiedProfile&&f.host.commandReads==0,"Current child graph refuses");
            f.action->SetCurrentActionForAnalysis(nullptr);f.action->AddOwnedActionForAnalysis(std::make_unique<spBaseObject>());
            Check(f.action->EnterForAnalysis()==wxHelpAIActionResultForAnalysis::UnqualifiedProfile&&f.host.commandReads==0,"Owned child graph refuses without consuming");
        }
        {
            Fixture f(state);const auto before=f.host.Snapshot();f.action->vfunc_0C(nullptr);
            Check(f.action->LastResultForAnalysis()==wxHelpAIActionResultForAnalysis::UnqualifiedNotify,"Notification refuses");
            Check(!f.action->vfunc_24()&&!f.action->vfunc_28()&&!f.action->vfunc_2C()&&!f.action->vfunc_3C(nullptr)&&!f.action->vfunc_40(nullptr),"Unknown bool hooks refuse");
            f.action->vfunc_34_ClearForAnalysis();f.action->vfunc_38();
            Check(f.action->LastResultForAnalysis()==wxHelpAIActionResultForAnalysis::UnqualifiedHook,"Unknown void hooks refuse");
            spCloneManager clones;Check(!f.action->vfunc_14(*f.action,clones),"Copy refuses");bool refused=false;
            try{(void)f.action->vfunc_10(clones);}catch(const std::logic_error&){refused=true;}Check(refused,"Clone refuses");
            refused=false;try{(void)spRTTIManager::Instance().Create(wxHelpAIAction::ClassID);}catch(const std::logic_error&){refused=true;}Check(refused,"Generic factory needs explicit capture");
            Check(f.action->vfunc_18().IsExactly(wxHelpAIAction::ClassID)&&f.action->vfunc_18().IsKindOf(wxAIAction::ClassID)&&f.host.Snapshot()==before&&f.host.commandReads==0,"RTTI and unknown operations preserve selected fields");
        }
        {
            Fixture f(state);f.host.beforeCommandWrite=state.ownBytes;f.action->vfunc_30();const auto commands=f.host.commands;const auto writes=f.host.commandWrites;
            f.action.reset();f.host.action=nullptr;Check(f.host.commands==commands&&f.host.commandWrites==writes,"Borrowed commands survive captured action destruction");
        }
    }
}
int main(int argc,char** argv)
{
    try{if(argc==2&&std::string(argv[1])=="--replay")Replay();else{Contracts();std::cout<<"Help bounded Entry contracts passed\n";}return 0;}
    catch(const std::exception& error){std::cerr<<"Help "<<error.what()<<'\n';return 1;}
}
