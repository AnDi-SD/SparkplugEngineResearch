#include "Code/SparkplugPS2/spPS2Mouse.h"
#include "Analysis/PS2/spPS2MouseAbi.h"
#include <cmath>
#include <cstring>
#include <deque>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

using namespace sparkplug::reconstruction;
namespace host=sparkplug::analysis::host;
namespace
{
    unsigned checks=0;
    void Check(bool value,const char* label){++checks;if(!value)throw std::runtime_error(label);}
    std::uint32_t Bits(float value){std::uint32_t bits;std::memcpy(&bits,&value,4);return bits;}
    float Float(std::uint32_t bits){float value;std::memcpy(&value,&bits,4);return value;}
    // Explicit test-only backend: exact integral single operations within their
    // representable range. It does not claim a general PS2 FPU implementation.
    class Boundary final:public host::spPS2MouseBoundaryForAnalysis
    {
    public:
        struct Reply{unsigned function;std::int32_t status=0;std::vector<std::uint8_t> payload;};
        std::uint8_t gate=1;
        std::uint32_t semaphore=10;
        std::int32_t created=10,bindStatus=0;
        unsigned binds=0,readyAfter=2,creates=0;
        bool mutateSemaphore=false;
        spPS2Mouse* mouse=nullptr;
        std::deque<Reply> replies;
        std::map<std::uint32_t,std::uint8_t> bytes;
        std::map<std::uint32_t,std::uint32_t> words;
        std::vector<std::string> trace;
        std::vector<host::spPS2MouseRpcRequestForAnalysis> requests;
        std::function<void()> onReport;
        std::optional<std::uint8_t> ReadGpByte(std::int32_t offset) override
        {Check(offset==-0x4554,"exact initialization gate gp offset");return gate;}
        std::optional<std::uint32_t> ReadGpWord(std::int32_t offset) override
        {Check(offset==-0x4558,"exact semaphore gp offset");return semaphore;}
        void WriteGpWord(std::int32_t offset,std::uint32_t value) override
        {Check(offset==-0x4558,"exact semaphore store gp offset");semaphore=value;}
        std::optional<std::uint8_t> ReadByte(std::uint32_t address) override
        {auto i=bytes.find(address);return i==bytes.end()?std::nullopt:std::optional<std::uint8_t>(i->second);}
        std::optional<std::uint32_t> ReadWord(std::uint32_t address) override
        {auto i=words.find(address);return i==words.end()?std::nullopt:std::optional<std::uint32_t>(i->second);}
        void WriteWord(std::uint32_t address,std::uint32_t value) override {words[address]=value;}
        std::optional<std::int32_t> CreateSemaphore(const host::spPS2MouseSemaphoreDescriptorForAnalysis& descriptor) override
        {++creates;Check(!descriptor.word00&&descriptor.word04==1&&descriptor.word08==1,"unwritten descriptor word preserved");return created;}
        void WaitSemaphore(std::uint32_t id) override
        {trace.push_back("W"+std::to_string(id));if(mutateSemaphore)++semaphore;}
        void SignalSemaphore(std::uint32_t id) override
        {trace.push_back("S"+std::to_string(id));if(mutateSemaphore)++semaphore;}
        void SignalSemaphoreFromInterrupt(std::uint32_t id) override
        {trace.push_back("I"+std::to_string(id));if(mutateSemaphore)++semaphore;}
        void InitializeRpc(std::uint32_t mode) override {Check(mode==0,"native RPC initialize mode0");}
        std::optional<std::int32_t> BindRpc(std::uint32_t client,std::uint32_t identifier,std::uint32_t mode) override
        {Check(client==0x4B7900&&identifier==0x80000210&&mode==0,"native bind arguments");words[client+0x24]=++binds>=readyAfter?99:0;return bindStatus;}
        std::optional<std::int32_t> CallRpc(const host::spPS2MouseRpcRequestForAnalysis& request) override
        {
            Check(!replies.empty()&&replies.front().function==request.function,"native function order");
            Check(request.client==0x4B7900&&request.mode==1&&request.send==0x4B7940&&request.sendBytes==32&&
                request.receiveBytes==32&&request.completionAddress==0x1F1F80&&request.completionArgument==0,
                "all native RPC call arguments retained");
            auto reply=std::move(replies.front());replies.pop_front();requests.push_back(request);
            for(unsigned i=0;i<reply.payload.size();++i)bytes[request.receive+i]=reply.payload[i];
            if(mouse)Check(mouse->RpcCompletionForAnalysis(),"original callback dispatch");
            return reply.status;
        }
        void Report(std::uint32_t address,std::optional<std::int32_t> argument) override
        {trace.push_back("R"+std::to_string(address)+(argument?":"+std::to_string(*argument):""));if(onReport)onReport();}
        static bool Integral(float value){return std::isfinite(value)&&std::trunc(value)==value;}
        std::optional<std::uint32_t> AddSignedByte(std::uint32_t bits,std::int8_t offset) override
        {
            const double result=double(Float(bits))+offset;
            if(!Integral(Float(bits))||std::abs(result)>16777216)return std::nullopt;
            return Bits(static_cast<float>(result));
        }
        std::optional<std::uint32_t> SubtractSingle(std::uint32_t first,std::uint32_t second) override
        {const double result=double(Float(first))-Float(second);if(!Integral(Float(first))||!Integral(Float(second)))return std::nullopt;
            const float stored=static_cast<float>(result);if(double(stored)!=result)return std::nullopt;return Bits(stored);}
        std::optional<std::int32_t> ConvertSingleToWord(std::uint32_t bits) override
        {const double value=Float(bits);if(!Integral(Float(bits))||value<-2147483648.0||value>=2147483648.0)return std::nullopt;
            return static_cast<std::int32_t>(value);}
        std::optional<bool> CompareSingleLess(std::uint32_t a,std::uint32_t b) override
        {if(!std::isfinite(Float(a))||!std::isfinite(Float(b)))return std::nullopt;return Float(a)<Float(b);}
        std::optional<bool> CompareSingleLessEqual(std::uint32_t a,std::uint32_t b) override
        {if(!std::isfinite(Float(a))||!std::isfinite(Float(b)))return std::nullopt;return Float(a)<=Float(b);}
    };
    void DefaultsAndInitialize()
    {
        spPS2Mouse mouse;
        Check(mouse.GetField44ForAnalysis()==0,"common field44 default");
        for(const auto& cell:mouse.GetTailForAnalysis())Check(!cell,"complete native tail stays unwritten");
        Check(mouse.PollForAnalysis()==true,"uninitialized poll returns true without boundary or payload");
        const auto unwritten=mouse.GetTailForAnalysis();mouse.Native1F1740ForAnalysis();
        mouse.PhysicalSlot5ForAnalysis(0xFFFFFFFFU,1,2,3);
        Check(mouse.GetTailForAnalysis()==unwritten&&Bits(mouse.PhysicalSlot6ForAnalysis(0xFFFFFFFFU))==0U,
            "actual void/command leaves preserve unwritten payload and float leaf returns positive zero");
        Check(!mouse.InitializeForAnalysis(),"unbound initialization reports unresolved");
        Check(spPS2Mouse::StaticRTTI().factory()->IsExactly(spPS2Mouse::ClassID)&&
            mouse.IsKindOf(spPS2InputDevice::ClassID)&&mouse.IsKindOf(spInputDevice::ClassID),"factory and registered hierarchy");
        Boundary b;mouse.SetBoundaryForAnalysis(&b);b.mouse=&mouse;b.gate=0;mouse.SetField44ForAnalysis(7);
        Check(mouse.InitializeForAnalysis()==true&&mouse.GetField44ForAnalysis()==7&&b.creates==0,
            "zero global initialization gate preserves existing flag and skips RPC");
        Check(!mouse.ReadTailWordForAnalysis(0x48),"zero gate keeps tail untouched");
        b.gate=1;b.created=-1;
        Check(mouse.InitializeForAnalysis()==false&&b.semaphore==0xFFFFFFFFU&&mouse.GetField44ForAnalysis()==7&&
            !mouse.ReadTailWordForAnalysis(0x50),"semaphore failure publishes -1 and preserves object state");
        b.created=10;b.bindStatus=-7;
        Check(mouse.InitializeForAnalysis()==false&&mouse.GetField44ForAnalysis()==7&&b.semaphore==10,
            "bind failure preserves original object fields after semaphore creation");
        b.bindStatus=0;b.binds=0;
        Check(mouse.InitializeForAnalysis()==true&&mouse.GetField44ForAnalysis()==1&&b.binds==2,
            "live server cell drives repeated bind until ready");
        Check(mouse.ReadTailWordForAnalysis(0x48)==2&&!mouse.ReadTailWordForAnalysis(0x4C),
            "initialize sets48 and leaves4C untouched");
        for(unsigned offset=0x50;offset<0xE0;offset+=4)Check(mouse.ReadTailWordForAnalysis(offset)==0,
            "three full48-byte zero ranges complete native initialized state");
    }
    void QueriesAndClone()
    {
        Boundary b;spPS2Mouse mouse(&b);b.mouse=&mouse;Check(mouse.InitializeForAnalysis()==true,"query fixture init");
        mouse.WriteTailWordForAnalysis(0x50,1);mouse.WriteTailWordForAnalysis(0x54,2);mouse.WriteTailWordForAnalysis(0xB0,1);
        Check(mouse.PhysicalSlot1ForAnalysis(100)==true&&mouse.PhysicalSlot1ForAnalysis(101)==false&&
            mouse.PhysicalSlot1ForAnalysis(99)==false&&mouse.PhysicalSlot2ForAnalysis(100)==true&&
            mouse.PhysicalSlot2ForAnalysis(108)==false,"exact-one current/changed words and native code bounds");
        mouse.WriteTailWordForAnalysis(0x74,Bits(-7));mouse.WriteTailWordForAnalysis(0x78,Bits(2147483904.0F));
        mouse.WriteTailWordForAnalysis(0x7C,Bits(-120));
        Check(mouse.PhysicalSlot3ForAnalysis(108)==0xFFFFFFF9U&&mouse.PhysicalSlot3ForAnalysis(109)==0x80000100U,
            "unsigned path preserves direct negative bits and subtract2^31/or high-bit path");
        Check(mouse.PhysicalSlot4ForAnalysis(108)==-7&&mouse.PhysicalSlot4ForAnalysis(110)==-120&&
            mouse.PhysicalSlot4ForAnalysis(111)==0,"signed conversion routing and wheel code");
        mouse.WriteTailWordForAnalysis(0x74,Bits(0.5F));
        Check(!mouse.PhysicalSlot4ForAnalysis(108),"explicit test backend refuses an unqualified fractional conversion");
        mouse.SetField44ForAnalysis(0);
        Check(mouse.PhysicalSlot3ForAnalysis(108)==0U&&mouse.PhysicalSlot4ForAnalysis(108)==0,
            "disabled queries do not request math or read unknown payload");
        mouse.SetField44ForAnalysis(255);mouse.SetName("PS2 mouse");mouse.AppendBindingForAnalysis(100,9);
        const auto queries=mouse.GetQueriesForAnalysis();
        Check(mouse.QuerySlot1ForAnalysis(9,queries)==true,"common logical routing reaches recovered physical provider");
        auto cloned=mouse.Clone();auto* clone=dynamic_cast<spPS2Mouse*>(cloned.get());
        Check(clone&&clone->GetField44ForAnalysis()==0&&clone->GetBindingsForAnalysis().empty()&&
            std::string(clone->GetName())=="PS2 mouse","Clone copies name and rebuilds common fields");
        for(const auto& cell:clone->GetTailForAnalysis())Check(!cell,"Clone does not invent tail defaults or copy payload");
        Check(clone->PollForAnalysis()==true,"fresh clone gate remains inactive");
        namespace math=sparkplug::evidence::ps2::mouse_exact_integer_math;
        Check(math::AddSignedByte(Bits(-12),12)==0U&&math::AddSignedByte(0x80000000U,0)==0U&&
            math::SubtractSingle(0x80000000U,0)==0x80000000U&&math::SubtractSingle(Bits(2147483648.0F),Bits(2147483648.0F))==0U,
            "shared exact operations preserve EE cancellation and signed-zero table");
        Check(math::ConvertSingleToWord(Bits(7.75F))==7&&math::ConvertSingleToWord(Bits(-7.75F))==-7&&
            math::ConvertSingleToWord(Bits(2147483648.0F))==2147483647&&
            math::ConvertSingleToWord(Bits(-2147483904.0F))==(-2147483647-1),
            "shared normal conversion follows documented truncation and saturation");
        Check(!math::ConvertSingleToWord(1)&&!math::ConvertSingleToWord(0x7F800000U),
            "unqualified denormal and encoded255 conversion remains unresolved");
    }
    void PollOperation()
    {
        Boundary b;spPS2Mouse mouse(&b);b.mouse=&mouse;Check(mouse.InitializeForAnalysis()==true,"poll fixture init");
        b.mutateSemaphore=true;b.trace.clear();
        b.replies={{1,0,{2,0,1,1}},{3,0,{}},{2,0,{4,1,127,128,255}},
            {2,0,{3,1,127,128,0}},{2,0,{4,0,127,127,2}},{2,0,{0}},
            {3,0,{}},{2,0,{3,2,128,127,0}},{2,0,{0}}};
        Check(mouse.PollForAnalysis()==true&&b.replies.empty()&&b.requests.size()==9,
            "whole count/info/event polling operation across two devices completes");
        Check(mouse.ReadTailWordForAnalysis(0x74)==Bits(-128)&&mouse.ReadTailWordForAnalysis(0x78)==Bits(127)&&
            mouse.ReadTailWordForAnalysis(0x7C)==0&&mouse.ReadTailWordForAnalysis(0xA4)==Bits(-128),
            "each successful info resets axes, event accumulation publishes current and previous snapshots");
        Check(mouse.PhysicalSlot1ForAnalysis(100)==false&&mouse.PhysicalSlot1ForAnalysis(101)==true&&
            mouse.PhysicalSlot2ForAnalysis(100)==false&&mouse.PhysicalSlot2ForAnalysis(101)==true,
            "last event replaces changed packet and multi-device button masks retain native ordering");
        Check(b.trace.size()==36&&b.trace[0]=="W10"&&b.trace[1]=="I11"&&b.trace[2]=="W12"&&b.trace[3]=="S13",
            "semaphore cell is reread across wait/RPC completion/wait/signal callbacks");
        Check(b.words[0x4B7940]==1,"last device index is written to original shared send cell");
        // Native skips rewriting current words if the byte masks agree, even
        // when an established object contains values inconsistent with masks.
        for(unsigned i=0;i<8;++i)mouse.WriteTailWordForAnalysis(0x50+4*i,99);
        mouse.MutableTailForAnalysis()[0x70-0x48]=1;mouse.MutableTailForAnalysis()[0xA0-0x48]=1;
        b.replies={{1,0,{1,0,1}},{3,0,{}},{2,0,{3,1,0,0}},{2,0,{0}}};
        Check(mouse.PollForAnalysis()==true&&mouse.ReadTailWordForAnalysis(0x50)==99&&
            mouse.ReadTailWordForAnalysis(0x80)==99&&mouse.ReadTailWordForAnalysis(0xB0)==0,
            "equal masks retain current words while copying previous snapshot and clearing changed packet");
        b.replies={{1,-5,{}}};const auto tail=mouse.GetTailForAnalysis();
        Check(mouse.PollForAnalysis()==false&&mouse.GetTailForAnalysis()==tail&&mouse.GetField44ForAnalysis()==1,
            "top-level RPC failure returns false without clearing active flag or packets");
        b.onReport=[&]{b.bytes[0x4B7780]=1;};
        b.replies={{1,0,{2,0,1,1}},{3,7,{}}};
        Check(mouse.PollForAnalysis()==true&&b.replies.empty(),
            "info failure continues and loop termination rereads callback-mutated device count");
        b.onReport={};b.replies={{1,0,{0}}};
        const auto preserved=mouse.GetTailForAnalysis();
        Check(mouse.PollForAnalysis()==true&&mouse.GetTailForAnalysis()==preserved,
            "no-device poll preserves stale changed packet and axes");
    }
}
int main()
{
    try{DefaultsAndInitialize();QueriesAndClone();PollOperation();std::cout<<"PASS "<<checks<<" PS2 mouse checks\n";return 0;}
    catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
