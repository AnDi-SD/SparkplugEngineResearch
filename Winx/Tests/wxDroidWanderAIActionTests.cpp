#include "Code/wxDroidWanderAIAction.h"
#include "Analysis/PC/wxDroidWanderAIActionAbi.h"
#include <array>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace sparkplug::reconstruction;
using namespace winx::reconstruction;
namespace
{
    void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
    using Command=std::array<std::uint8_t,0x74>;
    struct Actor{Command* command=nullptr;};
    struct Owner{Actor* actor=nullptr;};
    std::string Hex(const Command& bytes)
    {constexpr char digits[]="0123456789abcdef";std::string out;out.reserve(bytes.size()*2);for(auto b:bytes){out+=digits[b>>4];out+=digits[b&15];}return out;}
    std::string Strings(const std::vector<std::string>& values,bool raw)
    {std::ostringstream out;out<<'[';for(std::size_t i=0;i<values.size();++i){if(i)out<<',';if(!raw)out<<'"';out<<values[i];if(!raw)out<<'"';}out<<']';return out.str();}
    std::vector<std::string> From(const std::vector<std::string>& values,std::size_t first)
    {return {values.begin()+first,values.end()};}
    struct Host final:wxDroidWanderAIActionHost
    {
        std::array<Command,3> commands{};std::array<Actor,2> actors{};std::array<Owner,2> owners{};
        wxDroidWanderAIAction* action=nullptr;
        bool admitted=true,mutated=false,missingCommand=false;
        unsigned middle=0,exitInput=0,readCalls=0;
        std::vector<std::string> events,writes,reads,inputMutations;
        Host(std::uint32_t word,unsigned flag)
        {
            const unsigned fills[]={0xA5,0x5A,0xC3};
            for(unsigned i=0;i<3;++i){commands[i].fill(static_cast<std::uint8_t>(fills[i]));PutWord(commands[i],4,i?0xAABBCCDDu+i:word);commands[i][0x1C]=static_cast<std::uint8_t>(i?0xF0+i:flag);}
            for(unsigned i=0;i<2;++i){actors[i].command=&commands[i];owners[i].actor=&actors[i];}
        }
        static void PutWord(Command& value,unsigned offset,std::uint32_t word)
        {for(unsigned i=0;i<4;++i)value[offset+i]=static_cast<std::uint8_t>(word>>(8*i));}
        ProfileForAnalysis CommandProfileForAnalysis() const noexcept override
        {return admitted?ProfileForAnalysis::PcStableBorrowedCommandData:ProfileForAnalysis::Unqualified;}
        unsigned OwnerID(const void* value) const
        {if(!value)return 0;for(unsigned i=0;i<2;++i)if(value==&owners[i])return i+1;throw std::runtime_error("Live owner identity");}
        unsigned ActorID(const Actor* value) const
        {if(!value)return 0;for(unsigned i=0;i<2;++i)if(value==&actors[i])return i+1;throw std::runtime_error("Live actor identity");}
        unsigned CommandID(const void* value) const
        {if(!value)return 0;for(unsigned i=0;i<3;++i)if(value==&commands[i])return i+1;throw std::runtime_error("Live command identity");}
        std::string Bindings() const
        {
            std::ostringstream out;out<<"{\"owner\":"<<OwnerID(action->GetOwnerForAnalysis())<<",\"ownerActors\":["<<ActorID(owners[0].actor)<<','<<ActorID(owners[1].actor)
                <<"],\"actorCommands\":["<<CommandID(actors[0].command)<<','<<CommandID(actors[1].command)<<"]}";return out.str();
        }
        std::string Snapshot() const
        {
            std::ostringstream out;out<<"{\"bindings\":"<<Bindings()<<",\"commands\":[";
            for(unsigned i=0;i<3;++i){if(i)out<<',';out<<'"'<<Hex(commands[i])<<'"';}
            out<<"],\"baseClearableWord\":"<<action->GetClearableWordForAnalysis()<<'}';return out.str();
        }
        void* ReadCommandForAnalysis(void* value) noexcept override
        {
            ++readCalls;const auto owner=OwnerID(value);Check(owner>0,"Live command owner");
            reads.push_back("{\"kind\":\"owner144\",\"receiver\":"+std::to_string(owner)+",\"bindings\":"+Bindings()+'}');
            auto* actor=owners[owner-1].actor;const auto id=ActorID(actor);Check(id>0,"Live actor for owner144");
            reads.push_back("{\"kind\":\"actor130\",\"receiver\":"+std::to_string(id)+",\"bindings\":"+Bindings()+'}');
            return missingCommand?nullptr:actor->command;
        }
        void RecordWrite(void* value,std::uint32_t offset,unsigned size,std::uint32_t word)
        {
            const auto id=CommandID(value);Check(id>0,"Live command write");
            const auto owner=OwnerID(action->GetOwnerForAnalysis());Check(owner>0&&owners[owner-1].actor->command==value,"Write uses current chain");
            std::ostringstream out;out<<"{\"command\":"<<id<<",\"offset\":"<<offset<<",\"size\":"<<size<<",\"value\":"<<word<<",\"before\":"<<Snapshot()<<'}';writes.push_back(out.str());
            events.push_back("W"+std::to_string(id)+':'+std::to_string(offset)+':'+std::to_string(size)+':'+std::to_string(word));
        }
        void WriteCommandWordForAnalysis(void* value,std::uint32_t offset,std::uint32_t word) noexcept override
        {
            Check(offset==4&&word==0,"Own source chooses literal DWORD04 zero");RecordWrite(value,offset,4,word);PutWord(*static_cast<Command*>(value),offset,word);
            events.push_back("A:word04-zero:"+std::to_string(CommandID(value)));
            if(middle&&!mutated)
            {
                const auto before=Bindings();
                if(middle&1)action->SetOwnerForAnalysis(&owners[1]);
                if(middle&2)owners[0].actor=&actors[1];
                if(middle&4)actors[0].command=&commands[2];
                mutated=true;events.push_back("M:between:"+std::to_string(middle));
                inputMutations.push_back("{\"point\":\"afterDWORD04-before-fresh-owner-load\",\"before\":"+before+",\"after\":"+Bindings()+'}');
            }
        }
        void WriteCommandByteForAnalysis(void* value,std::uint32_t offset,std::uint8_t byte) noexcept override
        {Check(offset==0x1C&&(byte==0||byte==1),"Own source chooses literal byte1C value");RecordWrite(value,offset,1,byte);(*static_cast<Command*>(value))[offset]=byte;}
        void BeforeExit()
        {
            if(!exitInput)return;const auto before=Bindings();
            if(exitInput&1)action->SetOwnerForAnalysis(&owners[0]);
            if(exitInput&2)actors[1].command=&commands[2];
            if(exitInput&4)actors[0].command=&commands[1];
            events.push_back("M:beforeExit:"+std::to_string(exitInput));
            inputMutations.push_back("{\"point\":\"explicit-before-Exit\",\"before\":"+before+",\"after\":"+Bindings()+'}');
        }
    };
    void One(const std::string& sequence,std::uint32_t word,unsigned flag,unsigned middle,unsigned exitInput,bool emit)
    {
        Host host(word,flag);wxDroidWanderAIAction action(host,&host.owners[0]);host.action=&action;host.middle=middle;host.exitInput=exitInput;
        action.SetClearableWordForAnalysis(0xDEADBEEF);std::vector<std::string> steps;
        for(std::size_t ordinal=0;ordinal<sequence.size();++ordinal)
        {
            const char operation=sequence[ordinal];Check(operation=='s'||operation=='e',"Selected command operation");if(operation=='e')host.BeforeExit();
            const auto before=host.Snapshot();const auto firstWrite=host.writes.size(),firstEvent=host.events.size(),firstRead=host.reads.size();
            bool returned=false;if(operation=='s'){returned=action.vfunc_24();Check(returned,"Native low-byte1 publication");}else action.vfunc_34_ClearForAnalysis();
            Check(action.LastResultForAnalysis()==wxDroidWanderResultForAnalysis::Completed&&action.GetClearableWordForAnalysis()==0xDEADBEEF,"Admitted source completed without base clear374");
            std::ostringstream row;row<<"{\"operation\":\""<<(operation=='s'?"start":"exit")<<"\",\"ordinal\":"<<ordinal<<",\"before\":"<<before<<",\"after\":"<<host.Snapshot()<<",\"returnedLowByte\":";
            if(operation=='s')row<<1;else row<<"null";
            row<<",\"writes\":"<<Strings(From(host.writes,firstWrite),true)<<",\"events\":"<<Strings(From(host.events,firstEvent),false)<<",\"reads\":"<<Strings(From(host.reads,firstRead),true)<<'}';steps.push_back(row.str());
        }
        if(emit)std::cout<<"{\"steps\":"<<Strings(steps,true)<<",\"events\":"<<Strings(host.events,false)<<",\"writeEvents\":"<<Strings(host.writes,true)
            <<",\"readObservations\":"<<Strings(host.reads,true)<<",\"inputMutations\":"<<Strings(host.inputMutations,true)<<"}\n";
    }
    void Replay()
    {
        std::string sequence;std::uint32_t word;unsigned flag,middle,exitInput;
        while(std::cin>>sequence>>word>>flag>>middle>>exitInput)
        {Check(flag<256&&middle<8&&exitInput<8,"Five bounded replay inputs");One(sequence,word,flag,middle,exitInput,true);}
        Check(std::cin.eof(),"Complete Droid replay input");
    }
    void Contracts()
    {
        One("se",0xFFFFFFFF,255,1,1,false);One("ssee",0,0,0,0,false);One("se",0x12345678,165,4,4,false);
        Host host(0x12345678,165);wxDroidWanderAIAction action(host,&host.owners[0]);host.action=&action;action.SetClearableWordForAnalysis(0xDEADBEEF);
        const auto before=host.Snapshot();host.admitted=false;
        Check(!action.vfunc_24()&&action.LastResultForAnalysis()==wxDroidWanderResultForAnalysis::UnqualifiedProfile&&host.readCalls==0&&host.writes.empty()&&before==host.Snapshot(),"Default/unqualified refuses before reads/stores");
        host.admitted=true;action.SetOwnerForAnalysis(nullptr);
        Check(action.PublishCommandForAnalysis()==wxDroidWanderResultForAnalysis::MissingOwner&&host.writes.empty(),"Missing owner refuses before store");action.SetOwnerForAnalysis(&host.owners[0]);
        host.missingCommand=true;Check(action.ClearCommandForAnalysis()==wxDroidWanderResultForAnalysis::MissingCommand&&host.writes.empty(),"Missing command refuses before clear");host.missingCommand=false;
        wxAIAction other;action.SetCurrentActionForAnalysis(&other);Check(!action.vfunc_24()&&host.writes.empty(),"Current-action graph is outside selected profile");action.SetCurrentActionForAnalysis(nullptr);
        action.AddOwnedActionForAnalysis(std::make_unique<spBaseObject>());Check(!action.vfunc_24()&&host.writes.empty()&&action.GetOwnedActionCountForAnalysis()==1,"Owned-action graph refuses without consuming");
        action.vfunc_0C(nullptr);Check(action.LastResultForAnalysis()==wxDroidWanderResultForAnalysis::UnqualifiedNotify,"Notify refusal");
        Check(!action.vfunc_28()&&action.LastResultForAnalysis()==wxDroidWanderResultForAnalysis::UnqualifiedTick,"Complex Tick refusal");
        action.vfunc_30();Check(action.LastResultForAnalysis()==wxDroidWanderResultForAnalysis::UnqualifiedEntry,"Actual Entry refusal");
        action.vfunc_38();Check(action.LastResultForAnalysis()==wxDroidWanderResultForAnalysis::UnqualifiedQuery,"Query refusal");
        spCloneManager clones;Check(!action.vfunc_14(action,clones),"Copy admission refusal");bool refused=false;
        try{(void)action.vfunc_10(clones);}catch(const std::logic_error&){refused=true;}Check(refused,"Clone admission refusal");
        refused=false;try{(void)spRTTIManager::Instance().Create(wxDroidWanderAIAction::ClassID);}catch(const std::logic_error&){refused=true;}Check(refused,"Generic factory requires postconstruction binding");
        Check(action.vfunc_18().IsExactly(wxDroidWanderAIAction::ClassID)&&action.vfunc_18().IsKindOf(wxAIAction::ClassID),"Typed portable identity");
        Check(host.writes.empty()&&before==host.Snapshot(),"Unsupported operations preserve selected game fields");
    }
}
int main(int argc,char** argv)
{
    try{if(argc==2&&std::string(argv[1])=="--replay")Replay();else{Contracts();std::cout<<"DroidWander bounded command contracts passed\n";}return 0;}
    catch(const std::exception& error){std::cerr<<"DroidWander "<<error.what()<<'\n';return 1;}
}
