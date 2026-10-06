#include "Code/Sparkplug/spInputDevice.h"
#include "Code/SparkplugPC/spDXInputDevice.h"
#include "Code/SparkplugPS2/spPS2InputDevice.h"
#include "Analysis/PC/spInputDeviceAbi.h"
#include "Analysis/PS2/spInputDeviceAbi.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <tuple>

using namespace sparkplug::reconstruction;
namespace
{
    unsigned checks = 0;
    void Check(bool value, const char* label)
    { ++checks; if (!value) throw std::runtime_error(label); }
    std::uint32_t Bits(float value)
    { std::uint32_t result; std::memcpy(&result, &value, sizeof(result)); return result; }
    int Batch(bool constant = false)
    {
        unsigned op, count, cases = 0;
        std::uint32_t logical, answer = 0;
        while (std::cin >> op >> logical >> count)
        {
            if (++cases > 512 || !op || op > 6 || count > 16)
                throw std::runtime_error("bounded input-device batch");
            if (constant && !(std::cin >> answer)) throw std::runtime_error("missing constant provider");
            spInputDevice device;
            for (unsigned i = 0; i < count; ++i)
            {
                std::uint32_t physical, mapped;
                if (!(std::cin >> physical >> mapped)) throw std::runtime_error("truncated map");
                device.AppendBindingForAnalysis(physical, mapped);
            }
            std::vector<std::vector<std::uint32_t>> trace;
            spInputDevice::QueriesForAnalysis queries;
            queries.slot1 = [&](auto p) { trace.push_back({1,p}); return p % 3 == 2; };
            queries.slot2 = [&](auto p) { trace.push_back({2,p}); return p % 3 == 1; };
            queries.slot3 = [&](auto p) { trace.push_back({3,p}); return p ^ 0x12345678u; };
            queries.slot4 = [&](auto p) { trace.push_back({4,p}); return std::int32_t(p ^ 0x80000000u); };
            queries.slot5 = [&](auto p, auto a, auto b, auto c) { trace.push_back({5,p,a,b,c}); };
            queries.slot6 = [&](auto p) { trace.push_back({6,p}); return float(p & 0xffffu); };
            if (constant)
            {
                queries.slot1 = [&](auto p) { trace.push_back({1,p}); return bool(answer & 255u); };
                queries.slot2 = [&](auto p) { trace.push_back({2,p}); return bool(answer & 255u); };
                queries.slot3 = [&](auto p) { trace.push_back({3,p}); return answer; };
                queries.slot4 = [&](auto p) { trace.push_back({4,p}); return std::int32_t(answer); };
                queries.slot6 = [&](auto p) { trace.push_back({6,p}); float value; std::memcpy(&value,&answer,4);return value; };
            }
            std::uint32_t result = 0;
            if (op == 1) { const auto v = device.QuerySlot1ForAnalysis(logical,queries); Check(v.has_value(),"slot1 admission"); result=*v; }
            if (op == 2) { const auto v = device.QuerySlot2ForAnalysis(logical,queries); Check(v.has_value(),"slot2 admission"); result=*v; }
            if (op == 3) { const auto v = device.QuerySlot3ForAnalysis(logical,queries); Check(v.has_value(),"slot3 admission"); result=*v; }
            if (op == 4) { const auto v = device.QuerySlot4ForAnalysis(logical,queries); Check(v.has_value(),"slot4 admission"); result=std::uint32_t(*v); }
            if (op == 5) { Check(device.CommandSlot5ForAnalysis(logical,0x11223344u,0xffffffffu,7,queries),"slot5 admission"); result=1; }
            if (op == 6) { const auto v = device.QuerySlot6ForAnalysis(logical,queries); Check(v.has_value(),"slot6 admission"); result=Bits(*v); }
            std::cout << '[' << result << ",[";
            for (unsigned i=0;i<trace.size();++i)
            {
                if(i) std::cout << ',';
                std::cout << '[';
                for(unsigned j=0;j<trace[i].size();++j) { if(j)std::cout<<',';std::cout<<trace[i][j]; }
                std::cout << ']';
            }
            std::cout << "],[";
            for(unsigned i=0;i<device.GetScratchForAnalysis().size();++i)
            {
                if(i)std::cout<<',';
                const auto& v=device.GetScratchForAnalysis()[i];
                if(v)std::cout<<*v;else std::cout<<"null";
            }
            std::cout << "]]\n";
        }
        return 0;
    }
    struct References final : spDXInputDevice::ForeignReferencesForAnalysis
    {
        std::vector<std::pair<unsigned,std::uintptr_t>> calls;
        spDXInputDevice* receiver = nullptr;
        std::uintptr_t root = 0x1111;
        std::uintptr_t GetInputInterface() noexcept override { calls.emplace_back(0,root); return root; }
        void AddReference(std::uintptr_t p) noexcept override { calls.emplace_back(1,p); }
        void ReleaseReference(std::uintptr_t p) noexcept override { calls.emplace_back(2,p); }
        void Unacquire(std::uintptr_t p) noexcept override
        {
            calls.emplace_back(3,p);
            auto state=receiver->GetStateForAnalysis();state.device50=0x3333;receiver->SetStateForAnalysis(state);
        }
    };
    void Tests()
    {
        spInputDevice device;
        Check(!spInputDevice::StaticRTTI().factory && !device.Clone(),"native abstract factory and null clone");
        Check(device.IsKindOf(spCrossPlatform::ClassID),"common RTTI");
        for(auto v:device.GetScratchForAnalysis())Check(!v,"unwritten scratch explicit");
        Check(device.QuerySlot1ForAnalysis(9,{}).value()==false && device.QuerySlot6ForAnalysis(9,{}).value()==0,
            "empty matching set requires no foreign interface");
        device.AppendBindingForAnalysis(1,9); device.AppendBindingForAnalysis(2,9);device.AppendBindingForAnalysis(3,8);
        const auto before=device.GetScratchForAnalysis();std::string error;
        Check(!device.QuerySlot1ForAnalysis(9,{},&error) && !error.empty() && device.GetScratchForAnalysis()==before,
            "unresolved interface has no fake result");
        std::vector<std::uint32_t> calls;
        spInputDevice::QueriesForAnalysis q;
        q.slot1=[&](auto p){calls.push_back(p);return p==1;};
        Check(device.QuerySlot1ForAnalysis(9,q).value() && calls==std::vector<std::uint32_t>{1},"Boolean short circuit");
        Check(device.GetScratchForAnalysis()[1].value()==2,"all matches collected before short circuit");
        q.slot3=[&](auto p){calls.push_back(p);return p+10;};calls.clear();
        Check(device.QuerySlot3ForAnalysis(9,q).value()==11 && calls==std::vector<std::uint32_t>{1},"scalar first match");
        q.slot1=[&](auto p){calls.push_back(p);device.ClearBindingsForAnalysis();return false;};calls.clear();
        Check(!device.QuerySlot1ForAnalysis(9,q).value() && calls==std::vector<std::uint32_t>{1,2},"map mutation preserves collected codes");
        Check(device.GetScratchForAnalysis()[0].value()==1,"clear leaves scratch");
        for(unsigned i=0;i<6;++i)device.AppendBindingForAnalysis(i,9);
        Check(!device.QuerySlot1ForAnalysis(9,q),"native scratch overflow domain rejected");
        device.ClearBindingsForAnalysis();
        device.AppendBindingForAnalysis(10,1);device.AppendBindingForAnalysis(20,1);
        device.AppendBindingForAnalysis(30,2);device.AppendBindingForAnalysis(40,2);
        q.slot1=[&](auto p){calls.push_back(p);if(p==10)(void)device.QuerySlot3ForAnalysis(2,q);return false;};calls.clear();
        Check(!device.QuerySlot1ForAnalysis(1,q).value() && calls==std::vector<std::uint32_t>{10,30,40},"nested query overwrites shared scratch");
        spPS2InputDevice ps2;
        Check(ps2.GetField44ForAnalysis()==0 && ps2.IsKindOf(spInputDevice::ClassID) && !ps2.Clone() &&
            !spPS2InputDevice::StaticRTTI().factory,"PS2 constructor identity and null factory");
        References refs;
        {
            spDXInputDevice dx(refs);refs.receiver=&dx;
            Check(dx.IsKindOf(spInputDevice::ClassID) && !dx.Clone() && !spDXInputDevice::StaticRTTI().factory,"DX identity/null clone");
            const auto& state=dx.GetStateForAnalysis();
            Check(state.field44==0 && state.acquired45==0 && state.field48==0 && state.inputInterface4C==0x1111 &&
                state.device50==0 && state.field80==3 && state.field84==1,"DX confirmed constructor payload");
            auto changed=state;changed.device50=0x2222;changed.acquired45=255;dx.SetStateForAnalysis(changed);
        }
        Check(refs.calls==std::vector<std::pair<unsigned,std::uintptr_t>>{{0,0x1111},{1,0x1111},{2,0x1111},{3,0x2222},{2,0x3333}},
            "root release then device unacquire then fresh device release");
        refs.root=0;refs.calls.clear();bool threw=false;
        try { spDXInputDevice missing(refs); }catch(const std::invalid_argument&){threw=true;}
        Check(threw && refs.calls.size()==1,"missing interface admission before retain");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==2 && std::string(argv[1])=="--batch")return Batch();
        if(argc==2 && std::string(argv[1])=="--batch-constant")return Batch(true);
        Tests();std::cout<<"PASS "<<checks<<'/'<<checks<<": portable input-device checks\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
