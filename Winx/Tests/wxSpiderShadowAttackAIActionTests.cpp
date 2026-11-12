#include "Code/wxSpiderAttackAIAction.h"
#include "Code/wxShadowBeastAttackAIAction.h"
#include "Code/wxAICommandRecordForAnalysis.h"
#include "Analysis/PC/wxSpiderAttackAIActionAbi.h"
#include "Analysis/PC/wxShadowBeastAttackAIActionAbi.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

using namespace winx::reconstruction;
namespace
{
    using Command = wxAICommandRecordForAnalysis;
    using Profile = wxAIAttackStateProfileForAnalysis;
    using Result = wxAIAttackStateResultForAnalysis;
    using Access = wxAIAttackStateAccessForAnalysis;
    void Check(bool value, const char* text) { if (!value) throw std::runtime_error(text); }
    template<class Bytes> std::uint32_t Word(const Bytes& data, std::size_t offset)
    { std::uint32_t value = 0; for (unsigned b = 0; b != 4; ++b) value |= std::uint32_t{data[offset + b]} << (8 * b); return value; }
    template<class Bytes> void Put(Bytes& data, std::size_t offset, std::uint32_t value)
    { for (unsigned b = 0; b != 4; ++b) data[offset + b] = static_cast<std::uint8_t>(value >> (8 * b)); }
    template<class Bytes> std::string Hex(const Bytes& data)
    { constexpr char digits[] = "0123456789abcdef"; std::string out; for (auto byte : data) { out += digits[byte >> 4]; out += digits[byte & 15]; } return out; }
    unsigned Nibble(char c)
    { if (c >= '0' && c <= '9') return c - '0'; if (c >= 'a' && c <= 'f') return c - 'a' + 10; if (c >= 'A' && c <= 'F') return c - 'A' + 10; throw std::runtime_error("hex input"); }
    std::vector<std::uint8_t> Decode(const std::string& text)
    { Check(text.size() % 2 == 0, "even hex extent"); std::vector<std::uint8_t> out(text.size() / 2); for (std::size_t i = 0; i != out.size(); ++i) out[i] = static_cast<std::uint8_t>((Nibble(text[2*i]) << 4) | Nibble(text[2*i+1])); return out; }
    struct Input final
    { std::array<std::uint32_t, 9> native{}; std::array<std::vector<std::uint8_t>, 9> guarded; };
    template<class Action> constexpr bool Spider = std::is_same_v<Action, wxSpiderAttackAIAction>;
    template<class Action> constexpr std::size_t OwnerBytes = Spider<Action> ? 444 : 436;
    template<class Action> constexpr std::uint32_t Primary = Spider<Action> ? 0x7095F8U : 0x709B60U;
    template<class Action> constexpr std::size_t CommandByte = Spider<Action> ? 0x20 : 0x22;
    struct Record final
    { std::uint32_t native = 0; std::vector<std::uint8_t> guarded, initial; };
    struct GuardedCommand final
    {
        std::uint32_t native;
        std::array<std::uint8_t, 16> before{};
        Command value;
        std::array<std::uint8_t, 16> after{};
        GuardedCommand(std::uint32_t address, const Command::RawBytesForAnalysis& raw, Command::ProfileForAnalysis profile)
            : native(address), value(raw, profile) {}
    };
    template<class Action> struct GuardedAction final
    {
        std::array<std::uint8_t, 16> before{};
        Action value;
        std::array<std::uint8_t, 16> after{};
        GuardedAction(const typename Action::CapturedState& state, void* owner, wxAIAttackStateStorageHost* host, Profile profile)
            : value(state, owner, host, profile) {}
    };
    // Our raw supplied storage. No Record type below is attributed to a native
    // actor/state class; methods return only stored pointers or a raw DWORD.
    template<class Action> struct Fixture final : wxAIAttackStateStorageHost
    {
        Input input;
        std::array<Record, 6> records;
        std::array<std::unique_ptr<GuardedCommand>, 2> commands;
        std::unique_ptr<GuardedAction<Action>> action;
        std::array<char, 16> hostCodes{};
        std::size_t hostCount = 0;
        bool omitActor = false, omitState = false, omitValue = false, omitCommand = false;
        explicit Fixture(const Input& in, Profile profile = Profile::PcCapturedStableDisjointStorage,
            Command::ProfileForAnalysis cp = Command::ProfileForAnalysis::PcCaptured703090) : input(in)
        {
            const std::array<std::size_t,9> sizes{Action::CapturedBytesForAnalysis,OwnerBytes<Action>,OwnerBytes<Action>,308,308,308,308,116,116};
            for (unsigned i = 0; i != 9; ++i) Check(input.guarded[i].size() == sizes[i]+32, "complete guarded raw extent");
            for (unsigned i = 0; i != 6; ++i) { records[i].native = input.native[i+1]; records[i].guarded = records[i].initial = input.guarded[i+1]; }
            for (unsigned i = 0; i != 2; ++i)
            {
                Command::RawBytesForAnalysis raw; std::copy_n(input.guarded[i+7].begin()+16,116,raw.begin());
                commands[i] = std::make_unique<GuardedCommand>(input.native[i+7],raw,cp);
                std::copy_n(input.guarded[i+7].begin(),16,commands[i]->before.begin());
                std::copy_n(input.guarded[i+7].end()-16,16,commands[i]->after.begin());
            }
            typename Action::CapturedState state; std::copy_n(input.guarded[0].begin()+16,state.size(),state.begin());
            action = std::make_unique<GuardedAction<Action>>(state, FindRecord(Word(state,0x20),0), this, profile);
            std::copy_n(input.guarded[0].begin(),16,action->before.begin()); std::copy_n(input.guarded[0].end()-16,16,action->after.begin());
        }
        ~Fixture() { action.reset(); }
        Record* FindRecord(std::uint32_t address, unsigned first) noexcept
        { for (unsigned i = first; i != first+2; ++i) if (records[i].native == address) return &records[i]; return nullptr; }
        Record* KnownRecord(void* pointer, unsigned first) noexcept
        { for (unsigned i = first; i != first+2; ++i) if (pointer == &records[i]) return &records[i]; return nullptr; }
        Command* FindCommand(std::uint32_t address) noexcept
        { for (auto& entry : commands) if (entry->native == address) return &entry->value; return nullptr; }
        std::optional<void*> ReadActor144ForAnalysis(void* owner) noexcept override
        { hostCodes[hostCount++]='A'; auto* record=KnownRecord(owner,0); if (omitActor || !record) return std::nullopt; return static_cast<void*>(FindRecord(Word(record->guarded,16+0x144),2)); }
        std::optional<Command*> ReadCommand130ForAnalysis(void* actor) noexcept override
        { hostCodes[hostCount++]='C'; auto* record=KnownRecord(actor,2); if (omitCommand || !record) return std::nullopt; return FindCommand(Word(record->guarded,16+0x130)); }
        std::optional<void*> ReadState124ForAnalysis(void* actor) noexcept override
        { hostCodes[hostCount++]='S'; auto* record=KnownRecord(actor,2); if (omitState || !record) return std::nullopt; return static_cast<void*>(FindRecord(Word(record->guarded,16+0x124),4)); }
        std::optional<std::uint32_t> ReadStateWord130ForAnalysis(void* state) noexcept override
        { hostCodes[hostCount++]='V'; auto* record=KnownRecord(state,4); if (omitValue || !record) return std::nullopt; return Word(record->guarded,16+0x130); }
        std::uint32_t Native(const void* pointer) const
        {
            if (!pointer) return 0;
            if (pointer == &action->value) return input.native[0];
            for (const auto& record : records) if (pointer == &record) return record.native;
            for (const auto& entry : commands) if (pointer == &entry->value) return entry->native;
            throw std::runtime_error("known portable raw-record identity");
        }
        std::array<std::string,9> Snapshot() const
        {
            std::array<std::string,9> out;
            out[0]=Hex(action->before)+Hex(action->value.CapturedStateForAnalysis())+Hex(action->after);
            for (unsigned i=0;i!=6;++i) out[i+1]=Hex(records[i].guarded);
            for (unsigned i=0;i!=2;++i) out[i+7]=Hex(commands[i]->before)+Hex(commands[i]->value.GetBytesForAnalysis())+Hex(commands[i]->after);
            return out;
        }
        std::string Trace() const
        {
            std::ostringstream out; const auto& trace=action->value.LastAccessesForAnalysis();
            for (std::size_t i=0;i!=action->value.LastAccessCountForAnalysis();++i)
            {
                const auto& entry=trace[i]; if (i) out << ' ';
                out << (entry.kind==Access::Kind::WriteValue?'W':'R') << ':' << std::hex << std::uppercase << std::setfill('0') << std::setw(8)
                    << Native(entry.record)+entry.offset << ':' << std::dec << unsigned(entry.bytes) << ':' << std::hex << std::setw(8)
                    << (entry.kind==Access::Kind::ReadPointer?Native(entry.pointer):entry.value);
            }
            return out.str();
        }
        void Canaries() const
        {
            Check(Hex(action->before)==Hex(std::vector<std::uint8_t>(input.guarded[0].begin(),input.guarded[0].begin()+16))
                && Hex(action->after)==Hex(std::vector<std::uint8_t>(input.guarded[0].end()-16,input.guarded[0].end())),"portable action canaries");
            for (const auto& record:records)
                Check(std::equal(record.guarded.begin(),record.guarded.begin()+16,record.initial.begin()) && std::equal(record.guarded.end()-16,record.guarded.end(),record.initial.end()-16),"raw storage canaries");
            for (unsigned i=0;i!=2;++i)
                Check(Hex(commands[i]->before)==Hex(std::vector<std::uint8_t>(input.guarded[i+7].begin(),input.guarded[i+7].begin()+16))
                    && Hex(commands[i]->after)==Hex(std::vector<std::uint8_t>(input.guarded[i+7].end()-16,input.guarded[i+7].end())),"portable command canaries");
        }
        Result Invoke()
        { hostCount=0; const auto result=action->value.StateGateForAnalysis(); Canaries(); return result; }
        std::string HostTrace() const { return std::string(hostCodes.data(),hostCount); }
    };
    template<class Action> Input Base(std::uint8_t flag, std::uint32_t state)
    {
        Input in; const std::array<std::size_t,9> sizes{Action::CapturedBytesForAnalysis,OwnerBytes<Action>,OwnerBytes<Action>,308,308,308,308,116,116};
        for (unsigned i=0;i!=9;++i)
        { in.native[i]=0x41000010U+i*0x1000U; in.guarded[i].assign(sizes[i]+32,0xD7); std::fill(in.guarded[i].begin()+16,in.guarded[i].end()-16,static_cast<std::uint8_t>(0xA5+i)); }
        Put(in.guarded[0],16,Primary<Action>); Put(in.guarded[0],16+0x20,in.native[1]);
        Put(in.guarded[0],16+0x3AC,Spider<Action>?2:3); Put(in.guarded[0],16+0x3B0,0x12345678); Put(in.guarded[0],16+0x3B4,0xDEADBEEF); in.guarded[0][16+0x3BC]=flag;
        for (unsigned i=0;i!=2;++i)
        { Put(in.guarded[i+1],16+0x144,in.native[i+3]); Put(in.guarded[i+3],16+0x124,in.native[i+5]); Put(in.guarded[i+3],16+0x130,in.native[i+7]); Put(in.guarded[i+5],16+0x130,state); Put(in.guarded[i+7],16,0x703090); }
        return in;
    }
    template<class Action> void Units()
    {
        struct Expected { std::uint8_t flag; std::uint32_t state; int command,ownFlag; bool reset; };
        const std::array<Expected,9> cases{{{0,0,1,-1,false},{0,3,0,1,false},{0,0x103,1,-1,false},{0,0x80000003,1,-1,false},{0,0xFFFFFFFF,1,-1,false},
            {0x80,0,-1,0,true},{0x80,3,-1,-1,false},{0xFF,0x80000003,-1,0,true},{2,3,-1,-1,false}}};
        for (const auto c:cases)
        {
            Fixture<Action> f(Base<Action>(c.flag,c.state)); auto expected=f.Snapshot(); auto own=f.action->value.CapturedStateForAnalysis(); auto command=f.commands[0]->value.GetBytesForAnalysis();
            if (c.command>=0) command[CommandByte<Action>]=static_cast<std::uint8_t>(c.command);
            if (c.ownFlag>=0) own[0x3BC]=static_cast<std::uint8_t>(c.ownFlag);
            if (c.reset) { Put(own,0x3AC,0);Put(own,0x3B4,0);if constexpr (!Spider<Action>)Put(own,0x3B0,0); }
            expected[0]=Hex(f.action->before)+Hex(own)+Hex(f.action->after);expected[7]=Hex(f.commands[0]->before)+Hex(command)+Hex(f.commands[0]->after);
            Check(f.Invoke()==Result::Completed && f.Snapshot()==expected,"full-action/full-nine-region literal boundary expectation");
            Check(f.HostTrace()==(Spider<Action>?(c.flag==0?"ASVC":"ASV"):"ACSV"),"per-class conditional command read order");
            const auto& trace=f.action->value.LastAccessesForAnalysis();
            Check(trace[0].offset==(Spider<Action>?0x3BCU:0x20U),"Spider ownflag before owner; Shadow owner first");
            if constexpr (!Spider<Action>) Check(trace[2].kind==Access::Kind::ReadPointer && trace[2].offset==0x130 && trace[3].offset==0x3BC,"Shadow unconditional command preload before flag");
        }
        {
            Fixture<Action> f(Base<Action>(0,3),Profile::Unqualified);const auto before=f.Snapshot();Check(f.Invoke()==Result::UnqualifiedProfile && f.Snapshot()==before && f.hostCount==0 && f.action->value.LastAccessCountForAnalysis()==0,"default refuses before raw storage access");
        }
        {
            auto in=Base<Action>(0,3);Put(in.guarded[0],16,Primary<Action>+4);Fixture<Action> f(in);const auto before=f.Snapshot();Check(f.Invoke()==Result::CapturedPrimaryNotAdmitted && f.Snapshot()==before && f.hostCount==0,"wrong captured primary refuses before mutation");
        }
        {
            Fixture<Action> f(Base<Action>(0,3));wxAIAction other;f.action->value.SetCurrentActionForAnalysis(&other);const auto before=f.Snapshot();Check(f.Invoke()==Result::UnqualifiedProfile && f.Snapshot()==before && f.hostCount==0,"nonempty current graph refuses");f.action->value.SetCurrentActionForAnalysis(nullptr);
            f.action->value.AddOwnedActionForAnalysis(std::make_unique<wxAIAction>());Check(f.Invoke()==Result::UnqualifiedProfile && f.Snapshot()==before && f.hostCount==0,"nonempty owned graph refuses");
        }
        for (unsigned missing=0;missing!=4;++missing)
        {
            Fixture<Action> f(Base<Action>(0,3));f.omitActor=missing==0;f.omitState=missing==1;f.omitValue=missing==2;f.omitCommand=missing==3;const auto before=f.Snapshot();Check(f.Invoke()!=Result::Completed && f.Snapshot()==before,"missing raw storage refuses without invented native rollback");
        }
        {
            Fixture<Action> f(Base<Action>(0,3),Profile::PcCapturedStableDisjointStorage,Command::ProfileForAnalysis::Unqualified);const auto before=f.Snapshot();Check(f.Invoke()==Result::CommandNotAdmitted && f.Snapshot()==before,"real command provider admission refusal precedes own flag mutation");
        }
        {
            Fixture<Action> f(Base<Action>(1,0));f.omitCommand=true;const auto before=f.Snapshot();
            if constexpr (Spider<Action>) Check(f.Invoke()==Result::Completed && f.HostTrace()=="ASV","Spider nonzero flag avoids command130 read");
            else Check(f.Invoke()==Result::MissingCommand && f.Snapshot()==before && f.HostTrace()=="AC","Shadow command preload required even before nonzero flag");
        }
        {
            Fixture<Action> f(Base<Action>(0,3));Check(f.Invoke()==Result::Completed,"first retained state call");const auto after=f.Snapshot();Check(f.Invoke()==Result::Completed && f.Snapshot()==after,"repeat uses retained own flag and preserves complete state");
            Put(f.records[4].guarded,16+0x130,0x80000003);Check(f.Invoke()==Result::Completed && Word(f.action->value.CapturedStateForAnalysis(),0x3AC)==0 && Word(f.action->value.CapturedStateForAnalysis(),0x3B4)==0,"changed fullDWORD state re-evaluated on retained receiver");
        }
        {
            Fixture<Action> f(Base<Action>(0,0));Check(f.Invoke()==Result::Completed,"first owner binding call");const auto oldCommand=f.commands[0]->value.GetBytesForAnalysis();
            Put(f.action->value.CapturedStateForFixture(),0x20,f.input.native[2]);f.action->value.SetOwnerForAnalysis(&f.records[1]);Put(f.records[5].guarded,16+0x130,3);
            Check(f.Invoke()==Result::Completed && f.commands[0]->value.GetBytesForAnalysis()==oldCommand && f.commands[1]->value.GetBytesForAnalysis()[CommandByte<Action>]==0 && f.action->value.CapturedStateForAnalysis()[0x3BC]==1,"changed raw owner/actor/state/command binding read freshly");
        }
    }
    template<class Action> void Replay()
    {
        std::string line;
        while (std::getline(std::cin,line))
        {
            if (line.empty())continue;std::istringstream fields(line);Input in;std::string token;
            for (auto& native:in.native) { Check(bool(fields>>token),"native record token");Check(token.size()==8,"eight native token hex digits");const auto bytes=Decode(token);native=0;for(auto b:bytes)native=(native<<8)|b; }
            for (auto& data:in.guarded) { Check(bool(fields>>token),"complete guarded raw region");data=Decode(token); }
            Check(!(fields>>token),"no extra replay data");Fixture<Action> f(in);Check(f.Invoke()==Result::Completed,"replay admission");const auto after=f.Snapshot();
            for (unsigned i=0;i!=9;++i) { if(i)std::cout<<' ';std::cout<<after[i]; }std::cout<<" | "<<f.Trace()<<'\n';
        }
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==2 && std::string(argv[1])=="--spider-replay")Replay<wxSpiderAttackAIAction>();
        else if(argc==2 && std::string(argv[1])=="--shadow-replay")Replay<wxShadowBeastAttackAIAction>();
        else { Units<wxSpiderAttackAIAction>();Units<wxShadowBeastAttackAIAction>();std::cout<<"Spider/Shadow captured state-helper contracts passed\n"; }
        return 0;
    }
    catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
