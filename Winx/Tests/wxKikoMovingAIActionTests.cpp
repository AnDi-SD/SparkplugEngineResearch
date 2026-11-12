#include "Code/wxKikoMovingAIAction.h"
#include "Analysis/PC/wxKikoMovingAIActionAbi.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

using namespace winx::reconstruction;
namespace
{
    using Action=wxKikoMovingAIAction;
    using Profile=Action::ProfileForAnalysis;
    using Result=Action::ResultForAnalysis;
    using Access=Action::AccessForAnalysis;
    void Check(bool value,const char* text) { if(!value)throw std::runtime_error(text); }
    template<class Bytes> void Put(Bytes& bytes,std::size_t at,std::uint32_t value)
    { for(unsigned i=0;i!=4;++i)bytes[at+i]=static_cast<std::uint8_t>(value>>(i*8)); }
    template<class Bytes> std::uint32_t Word(const Bytes& bytes,std::size_t at)
    { std::uint32_t v=0;for(unsigned i=0;i!=4;++i)v|=std::uint32_t{bytes[at+i]}<<(i*8);return v; }
    template<class Bytes> std::string Hex(const Bytes& bytes)
    { std::ostringstream out;out<<std::hex<<std::setfill('0');for(auto b:bytes)out<<std::setw(2)<<unsigned(b);return out.str(); }
    std::vector<std::uint8_t> ParseHex(const std::string& s)
    {
        Check(s.size()%2==0,"raw hex length");std::vector<std::uint8_t> bytes;
        for(std::size_t i=0;i<s.size();i+=2)
        { std::size_t used=0;const auto v=std::stoul(s.substr(i,2),&used,16);Check(used==2,"raw hex digit");bytes.push_back(static_cast<std::uint8_t>(v)); }
        return bytes;
    }
    std::uint32_t ParseWord(const std::string& s)
    { Check(s.size()==8,"word width");std::size_t used=0;const auto v=std::stoul(s,&used,16);Check(used==8 && v<=0xFFFFFFFFUL,"word value");return static_cast<std::uint32_t>(v); }
    struct GuardedAction final
    {
        std::array<std::uint8_t,16> before;
        Action value;
        std::array<std::uint8_t,16> after;
        GuardedAction(const Action::CapturedState& s,Profile p):value(s,p)
        {before.fill(0xD7);after.fill(0xD7);}
        void Canaries() const
        {Check(std::all_of(before.begin(),before.end(),[](auto b){return b==0xD7;})&&std::all_of(after.begin(),after.end(),[](auto b){return b==0xD7;}),"portable object canaries");}
    };
    std::string Trace(const Action& action)
    {
        std::ostringstream out;const auto& trace=action.LastAccessesForAnalysis();
        for(std::size_t i=0;i<action.LastAccessCountForAnalysis();++i)
        {const auto& v=trace[i];if(i)out<<' ';out<<(v.kind==Access::Kind::Read?'R':'W')<<':'<<(v.role==Access::Role::MessageValue?'M':'S')<<':'
            <<std::hex<<std::uppercase<<std::setfill('0')<<std::setw(8)<<v.offset<<':'<<std::dec<<unsigned(v.bytes)<<':'<<std::hex<<std::setw(8)<<v.value;}
        return out.str();
    }
    Action::CapturedState Pattern()
    {
        Action::CapturedState state{};for(std::size_t i=0;i<state.size();++i)state[i]=static_cast<std::uint8_t>(0xA5+37*i);
        Put(state,0,winx::evidence::pc::wxKikoMovingAIActionPrimary);Put(state,0x3A8,0xDEADBEEF);Put(state,0x3AC,0xBAADF00D);return state;
    }
    void Units()
    {
        static_assert(sizeof(wxAIActionMessageForAnalysis)==4);
        static_assert(!std::is_copy_constructible_v<Action> && !std::is_move_constructible_v<Action>);
        for(const auto code:std::array<std::uint32_t,9>{0x27AA,0,0x27A9,0x27AB,0x000127AA,0xFFFF27AA,0x800027AA,0xFFFFFFFF,0x80000000})
        {
            const auto input=Pattern();GuardedAction guarded(input,Profile::PcCapturedReadOnlyDisjointMessage);auto expected=input;
            wxAIActionMessageForAnalysis message{code};const auto original=message;
            if(code==0x27AA){Put(expected,0x3A8,3);Put(expected,0x3AC,2);}
            Check(guarded.value.HandleMessageForAnalysis(message)==Result::Completed,"admitted code operation");guarded.Canaries();
            Check(guarded.value.CapturedStateForAnalysis()==expected && message.code==original.code,"complete raw7160 preservation and readonly value");
            Check(guarded.value.LastAccessCountForAnalysis()==(code==0x27AA?3U:1U),"oneDWORDread and two-or-zero DWORDstores");
            const auto& trace=guarded.value.LastAccessesForAnalysis();Check(trace[0].kind==Access::Kind::Read&&trace[0].role==Access::Role::MessageValue&&trace[0].bytes==4&&trace[0].value==code,"fullDWORD message read");
            if(code==0x27AA)Check(trace[1].offset==0x3A8&&trace[1].value==3&&trace[1].bytes==4&&trace[2].offset==0x3AC&&trace[2].value==2&&trace[2].bytes==4,"literal store order/width/value");
        }
        {
            const auto state=Pattern();GuardedAction guarded(state,Profile::Unqualified);Check(guarded.value.HandleMessageForAnalysis({0x27AA})==Result::UnqualifiedProfile&&guarded.value.CapturedStateForAnalysis()==state&&guarded.value.LastAccessCountForAnalysis()==0,"default admission before raw mutation");guarded.Canaries();
        }
        {
            auto state=Pattern();Put(state,0,0);GuardedAction guarded(state,Profile::PcCapturedReadOnlyDisjointMessage);Check(guarded.value.HandleMessageForAnalysis({0x27AA})==Result::CapturedPrimaryNotAdmitted&&guarded.value.CapturedStateForAnalysis()==state&&guarded.value.LastAccessCountForAnalysis()==0,"captured primary admission before mutation");guarded.Canaries();
        }
        {
            const auto state=Pattern();GuardedAction guarded(state,Profile::PcCapturedReadOnlyDisjointMessage);wxAIAction child;guarded.value.SetCurrentActionForAnalysis(&child);
            Check(guarded.value.HandleMessageForAnalysis({0x27AA})==Result::UnqualifiedProfile&&guarded.value.CapturedStateForAnalysis()==state,"nonempty current graph admission");guarded.value.SetCurrentActionForAnalysis(nullptr);
            guarded.value.AddOwnedActionForAnalysis(std::make_unique<wxAIAction>());Check(guarded.value.HandleMessageForAnalysis({0x27AA})==Result::UnqualifiedProfile&&guarded.value.CapturedStateForAnalysis()==state,"nonempty owned graph admission");guarded.Canaries();
        }
        {
            auto expected=Pattern();GuardedAction guarded(expected,Profile::PcCapturedReadOnlyDisjointMessage);int opaqueOwner=0;guarded.value.SetOwnerForAnalysis(&opaqueOwner);
            Check(guarded.value.HandleMessageForAnalysis({0x27AA})==Result::Completed,"first retained call");Put(expected,0x3A8,3);Put(expected,0x3AC,2);
            Check(guarded.value.HandleMessageForAnalysis({0x27AA})==Result::Completed&&guarded.value.CapturedStateForAnalysis()==expected,"repeat same instance idempotence");
            Put(guarded.value.CapturedStateForFixture(),0x3A8,0x13572468);Put(guarded.value.CapturedStateForFixture(),0x3AC,0xFEDCBA98);expected=guarded.value.CapturedStateForAnalysis();
            Check(guarded.value.HandleMessageForAnalysis({0x800027AA})==Result::Completed&&guarded.value.CapturedStateForAnalysis()==expected,"changed code reevaluated fullDWORD with supplied between-call state");
            Check(guarded.value.HandleMessageForAnalysis({0x27AA})==Result::Completed,"return to matching code");Put(expected,0x3A8,3);Put(expected,0x3AC,2);
            Check(guarded.value.CapturedStateForAnalysis()==expected&&guarded.value.GetOwnerForAnalysis()==&opaqueOwner,"unread portable owner and complete rawprefix retained");guarded.Canaries();
        }
        {
            const auto state=Pattern();GuardedAction guarded(state,Profile::PcCapturedReadOnlyDisjointMessage);guarded.value.vfunc_0C(nullptr);
            Check(guarded.value.LastResultForAnalysis()==Result::UnqualifiedHook&&guarded.value.CapturedStateForAnalysis()==state,"generic void-pointer delivery remains unqualified");
            Check(!guarded.value.vfunc_24()&&!guarded.value.vfunc_28()&&!guarded.value.vfunc_2C(),"other hooks refused");guarded.value.vfunc_30();guarded.value.vfunc_34_ClearForAnalysis();guarded.value.vfunc_38();
            Check(!guarded.value.vfunc_3C(nullptr)&&!guarded.value.vfunc_40(nullptr)&&guarded.value.CapturedStateForAnalysis()==state,"unsupported hooks do not clear raw state");
            sparkplug::reconstruction::spCloneManager manager;bool threw=false;try{auto copy=guarded.value.vfunc_10(manager);(void)copy;}catch(const std::logic_error&){threw=true;}
            wxAIAction other;Check(threw&&!guarded.value.vfunc_14(other,manager),"Clone/Copy not represented by base-only substitutes");guarded.Canaries();
        }
    }
    std::string Replay(const std::string& line)
    {
        std::istringstream in(line);unsigned calls=0,mask=0;std::string receiverHex,messageHex,codeHex,aHex,bHex,extra;
        Check(bool(in>>calls>>receiverHex>>messageHex>>mask>>codeHex>>aHex>>bHex)&&!(in>>extra),"replay syntax");
        Check((calls==1||calls==2)&&mask<=7&&(calls==2||mask==0),"bounded same-instance replay controls");
        auto receiver=ParseHex(receiverHex),message=ParseHex(messageHex);Check(receiver.size()==7192&&message.size()==36,"guarded native projection extents");
        const auto initialReceiver=receiver,initialMessage=message;Action::CapturedState state{};std::copy_n(receiver.begin()+16,state.size(),state.begin());
        GuardedAction action(state,Profile::PcCapturedReadOnlyDisjointMessage);const auto nextCode=ParseWord(codeHex),nextA=ParseWord(aHex),nextB=ParseWord(bHex);std::ostringstream out;
        for(unsigned call=0;call<calls;++call)
        {
            if(call==1){if(mask&1)Put(message,16,nextCode);if(mask&2)Put(action.value.CapturedStateForFixture(),0x3A8,nextA);if(mask&4)Put(action.value.CapturedStateForFixture(),0x3AC,nextB);}
            const auto beforeMessage=message;Check(action.value.HandleMessageForAnalysis({Word(message,16)})==Result::Completed,"admitted replay operation");action.Canaries();
            std::copy(action.value.CapturedStateForAnalysis().begin(),action.value.CapturedStateForAnalysis().end(),receiver.begin()+16);
            Check(message==beforeMessage&&std::equal(receiver.begin(),receiver.begin()+16,initialReceiver.begin())&&std::equal(receiver.end()-16,receiver.end(),initialReceiver.end()-16)
                &&std::equal(message.begin(),message.begin()+16,initialMessage.begin())&&std::equal(message.end()-16,message.end(),initialMessage.end()-16),"raw projection/message canaries");
            if(call)out<<" || ";out<<Hex(receiver)<<' '<<Hex(message)<<" | "<<Trace(action.value);
        }
        return out.str();
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==2&&std::string(argv[1])=="--replay") {std::string line;while(std::getline(std::cin,line))if(!line.empty())std::cout<<Replay(line)<<'\n';return 0;}
        Check(argc==1,"unknown arguments");Units();std::cout<<"KikoMoving captured message contracts passed\n";return 0;
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
