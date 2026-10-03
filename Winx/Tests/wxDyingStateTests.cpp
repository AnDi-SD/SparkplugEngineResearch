#include "Code/wxDyingState.h"
#include "Code/wxBloomDyingState.h"
#include "Analysis/Host/wxDyingStateHost.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool value, const char* text) { if (!value) throw std::runtime_error(text); }
    void* Pointer(std::uintptr_t value) { return reinterpret_cast<void*>(value); }
    std::uintptr_t Token(void* value) { return reinterpret_cast<std::uintptr_t>(value); }
    unsigned Sent(wxCharacterState& s)
    {
        if (auto* p = dynamic_cast<wxDyingState*>(&s)) return p->GetSentDyingOverByteForAnalysis();
        return static_cast<wxBloomDyingState&>(s).GetSentDyingOverByteForAnalysis();
    }
    void SetSent(wxCharacterState& s, unsigned value)
    {
        if (auto* p = dynamic_cast<wxDyingState*>(&s)) p->SetSentDyingOverByteForAnalysis(std::uint8_t(value));
        else static_cast<wxBloomDyingState&>(s).SetSentDyingOverByteForAnalysis(std::uint8_t(value));
    }
    struct Host final : wxBloomDyingStateHost
    {
        wxCharacterState& state; wxAnimationRequestForAnalysis& request;
        std::uintptr_t next = 22, first = 0, second = 0;
        bool predicate = false; unsigned ownerKind = 24, ownerFlag = 0, gate = 0;
        unsigned target = 1, querySent = 256, resetSent = 256, messageSent = 256;
        unsigned control = 0xFFFFFFFFu, speed = 0; std::string trace;
        Host(wxCharacterState& s, wxAnimationRequestForAnalysis& r) : state(s), request(r) {}
        unsigned Flags() const
        { auto f = state.GetTransitionFlagsForAnalysis(); unsigned n = 0; for (unsigned i = 0; i < 5; ++i) if (f[i]) n |= 1u << i; return n; }
        void Event(const std::string& name)
        {
            if (!trace.empty()) trace += ';';
            trace += name + '@' + std::to_string(Token(state.GetPendingHandleForAnalysis())) + ':'
                + std::to_string(Flags()) + ':' + std::to_string(request.packedKey) + ':' + std::to_string(Sent(state));
        }
        void* ResolveAnimationForAnalysis(void*, std::uint32_t key) override { Event("lookup:" + std::to_string(key)); return Pointer(next); }
        bool OwnerPredicateForAnalysis(void*) override { Event("predicate"); return predicate; }
        void ResetCompletionForAnalysis(void*, void* h) override { Event("reset:" + std::to_string(Token(h))); }
        void StartAnimationForAnalysis(void*, void* h, bool mode, std::uint32_t fade, bool interrupt) override
        { Event("start:" + std::to_string(Token(h)) + ':' + std::to_string(mode) + ':' + std::to_string(fade) + ':' + std::to_string(interrupt)); }
        void StopAnimationForAnalysis(void*, void* h) override { Event("stop:" + std::to_string(Token(h))); }
        void FadeAnimationForAnalysis(void*, void* h, float v) override { Check(v == 0.4f, "fade word"); Event("fade:" + std::to_string(Token(h))); }
        bool IsPendingAnimationCompleteForAnalysis(void*, void* h, bool consume) override
        {
            Check(consume, "completion consumption"); auto token = Token(h); Event("query:" + std::to_string(token));
            if (querySent < 256) SetSent(state, querySent);
            if (!token) return true;
            const bool result = first == token || second == token;
            if (first == token) first = 0; if (second == token) second = 0;
            return result;
        }
        void ClearOwnerActionControlForAnalysis(void*) override { Event("word"); control = 0; }
        void SetConsumerSpeedForAnalysis(void*, float value) override
        { Check(value == 1.0f, "speed1"); Event("speed1"); speed = 0x3F800000; }
        std::uint32_t DyingOwnerKindForAnalysis(void*) override { return ownerKind; }
        std::uint8_t DyingOwnerFlagForAnalysis(void*) override { return std::uint8_t(ownerFlag); }
        void ResetDyingControlForAnalysis(void*) override
        { Event("control-reset"); control = 0; if (resetSent < 256) SetSent(state, resetSent); }
        void* DyingNotificationTargetForAnalysis(void*) override { return target ? Pointer(0x300) : nullptr; }
        void SendDyingNotificationForAnalysis(void* to, wxCharacterState& source, std::uint32_t code, std::uint32_t p0, std::uint32_t p1) override
        {
            Check(to == Pointer(0x300) && &source == &state, "packet receiver/source");
            Event("message:" + std::to_string(code) + ':' + std::to_string(p0) + ':' + std::to_string(p1));
            if (messageSent < 256) SetSent(state, messageSent);
            if (target == 2) target = 0;
        }
        void DyingDiagnosticForAnalysis(const char* format, wxCharacterState& source) override
        { Check(&source == &state && std::string(format) == "Dying State: m_bSentDyingOverMsg = true for : 0x%x", "diagnostic format"); Event("diagnostic"); }
        std::uint8_t BloomDyingOwnerFlagForAnalysis(void*) override { return std::uint8_t(ownerFlag); }
        std::uint32_t BloomDyingUpdateGateForAnalysis(void*) override { return gate; }
        void BloomDyingManager593B00ForAnalysis(std::uint32_t v, std::uint32_t mode) override
        { Check(v == 7000 && mode == 0, "entry manager words"); Event("manager593B00"); }
        void BloomDyingManager593A60ForAnalysis(std::uint32_t v, std::uint32_t mode) override
        { Check(v == 1000 && mode == 0, "completion manager words"); Event("manager593A60"); }
        void BloomDyingManager595450ForAnalysis(std::uint32_t v, std::uint32_t mode) override
        { Check(v == 0x35 && mode == 1, "completion manager words"); Event("manager595450"); }
        void BloomDyingManager5953E0ForAnalysis(std::uint32_t v, std::uint32_t mode) override
        { Check(v == 0x40 && mode == 0, "completion manager words"); Event("manager5953E0"); }
    };
    std::string Case(const std::vector<std::uint64_t>& v)
    {
        Check(v.size() == 17, "case field count");
        std::unique_ptr<wxCharacterState> state = v[0] ? std::unique_ptr<wxCharacterState>(new wxBloomDyingState)
            : std::unique_ptr<wxCharacterState>(new wxDyingState);
        wxAnimationRequestForAnalysis request{std::uint32_t(v[2])}; Host host(*state, request);
        state->SetBindingsForAnalysis(Pointer(0x100), Pointer(0x200), &host);
        state->SetPendingHandleForAnalysis(Pointer(v[3])); state->SetTransitionFlagsForAnalysis(v[6]&1, v[6]&2, v[6]&4, v[6]&8, v[6]&16);
        SetSent(*state, unsigned(v[7])); host.next=v[4]; host.predicate=v[5]!=0; host.ownerKind=unsigned(v[8]); host.ownerFlag=unsigned(v[9]);
        host.gate=unsigned(v[10]); host.first=v[11]; host.second=v[12]; host.target=unsigned(v[13]);
        host.querySent=unsigned(v[14]); host.resetSent=unsigned(v[15]); host.messageSent=unsigned(v[16]); unsigned result=2;
        if(v[1]==0x1C) result=state->vfunc_1C(request); else if(v[1]==0x20) result=state->vfunc_20(request);
        else if(v[1]==0x30) state->vfunc_30(request); else if(v[1]==0x34) result=state->vfunc_34(request.packedKey);
        else if(v[1]==0x38) result=state->vfunc_38(request.packedKey); else if(v[1]==0x40) state->vfunc_40_ResetForAnalysis();
        else throw std::logic_error("Unknown slot");
        std::ostringstream out; out<<result<<' '<<request.packedKey<<' '<<Token(state->GetPendingHandleForAnalysis())<<' '
            <<host.control<<' '<<host.first<<' '<<host.second<<' '<<host.Flags()<<' '<<Sent(*state)<<' '<<host.speed<<' '<<host.target<<'|'<<host.trace;
        return out.str();
    }
    template<class T> void Lifecycle()
    {
        T state; Check(state.GetStateSelectorForAnalysis()==11&&state.GetSentDyingOverByteForAnalysis()==0,"constructor defaults");
        state.SetSentDyingOverByteForAnalysis(255); state.vfunc_40_ResetForAnalysis(); Check(state.GetSentDyingOverByteForAnalysis()==255,"reset preserves own byte");
        spCloneManager manager; auto clone=state.vfunc_10(manager); auto& typed=static_cast<T&>(*clone);
        Check(manager.FindClone(state)==clone.get()&&typed.GetSentDyingOverByteForAnalysis()==0&&typed.GetStateSelectorForAnalysis()==11,"fresh clone defaults");
        T dest;dest.SetSentDyingOverByteForAnalysis(127);Check(state.vfunc_14(dest,manager)&&dest.GetSentDyingOverByteForAnalysis()==127,"copy preserves own byte");
        Check(typed.IsExactly(T::ClassID)&&typed.IsKindOf(wxCharacterState::ClassID),"physical RTTI base");
        Check(bool(spRTTIManager::Instance().Create(T::ClassID)),"registered factory");
        wxAnimationRequestForAnalysis request;bool rejected=false;try{(void)state.vfunc_1C(request);}catch(const std::logic_error&){rejected=true;}
        Check(rejected,"missing host rejected");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==19&&std::string(argv[1])=="--case")
        {std::vector<std::uint64_t> v;for(int i=2;i<argc;++i)v.push_back(std::stoull(argv[i]));std::cout<<Case(v)<<'\n';return 0;}
        if(argc==2&&std::string(argv[1])=="--stream")
        {std::string line;while(std::getline(std::cin,line)){std::istringstream in(line);std::vector<std::uint64_t> v;std::uint64_t n;while(in>>n)v.push_back(n);std::cout<<Case(v)<<'\n';}return 0;}
        Lifecycle<wxDyingState>();Lifecycle<wxBloomDyingState>();
        std::vector<std::uint64_t> v{0,0x30,0,11,22,0,31,1,24,0,0,11,11,1,256,256,256};
        Check(Case(v).find("query:")==std::string::npos,"ordinary Dying guard before query");v[0]=1;
        Check(Case(v).find("query:")!=std::string::npos,"Bloom query before guard");v[10]=1;
        Check(Case(v).find("control-reset")==std::string::npos,"Bloom gate precedes control reset");
        std::cout<<"PASS Dying/BloomDying operations and lifetime\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
