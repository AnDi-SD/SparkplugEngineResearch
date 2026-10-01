#include "Analysis/Host/wxDispelStateHost.h"
#include "Code/wxDispelState.h"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    void Require(bool value, const char* message)
    { if (!value) { std::cerr << message << '\n'; std::exit(EXIT_FAILURE); } }
    void* Pointer(std::uintptr_t value) { return reinterpret_cast<void*>(value); }
    std::uintptr_t Token(void* value) { return reinterpret_cast<std::uintptr_t>(value); }
    struct Host final : wxDispelStateHost
    {
        wxDispelState& state;
        void* next = Pointer(22);
        bool predicate = false, present = true;
        std::uint32_t word4 = 0xFFFFFFFF, word68 = 0xFFFFFFFF;
        mutable std::string trace;
        explicit Host(wxDispelState& value) : state(value) {}
        void Event(const std::string& name) const
        {
            if (!trace.empty()) trace += ';';
            trace += name + '@' + std::to_string(Token(state.GetPendingHandleForAnalysis()))
                + ':' + std::to_string(state.GetExitMessageFlagForAnalysis());
        }
        void ClearOwnerControlWord68ForAnalysis(void*) override { Event("word68"); word68 = 0; }
        bool HasNotificationManagerForAnalysis() const override { Event("manager"); return present; }
        void DispatchExitMessageForAnalysis(const wxDispelStateMessageForAnalysis& message) override
        {
            Require(message.code==0x2806 && !message.word04 && !message.word08 && !message.word0C
                && message.source==&state && !message.word14 && message.word18==0x22 && message.word1C==1,
                "all eight native exit-message words"); Event("message");
        }
        void* ResolveAnimationForAnalysis(void*, std::uint32_t key) override
        { Event("lookup:" + std::to_string(key)); return next; }
        bool OwnerPredicateForAnalysis(void*) override { Event("predicate"); return predicate; }
        void ResetCompletionForAnalysis(void*, void* handle) override
        { Event("reset:" + std::to_string(Token(handle))); }
        void StartAnimationForAnalysis(void*, void* handle, bool mode,
            std::uint32_t fade, bool interrupt) override
        {
            Event("start:" + std::to_string(Token(handle)) + ':' + std::to_string(mode)
                + ':' + std::to_string(fade) + ':' + std::to_string(interrupt));
        }
        void StopAnimationForAnalysis(void*, void* handle) override
        { Event("stop:" + std::to_string(Token(handle))); }
        void FadeAnimationForAnalysis(void*, void* handle, float duration) override
        { Require(duration==0.4f,"native fade"); Event("fade:" + std::to_string(Token(handle))); }
        bool IsPendingAnimationCompleteForAnalysis(void*, void*, bool) override
        { throw std::logic_error("unexpected query in this fixture"); }
        void ClearOwnerActionControlForAnalysis(void*) override { Event("word4"); word4 = 0; }
    };
    std::string Case(unsigned slot, std::uint32_t key, std::uintptr_t pending,
        std::uintptr_t next, bool predicate, bool flag, bool present)
    {
        wxDispelState state; Host host(state);
        state.SetBindingsForAnalysis(Pointer(0x100),Pointer(0x200),&host);
        if (flag)
        {
            wxAnimationRequestForAnalysis prepare{0};
            Require(state.vfunc_1C(prepare), "prepare exit-message flag through entry");
            host.trace.clear(); host.word4 = host.word68 = 0xFFFFFFFF;
        }
        state.SetPendingHandleForAnalysis(Pointer(pending));
        host.next=Pointer(next); host.predicate=predicate; host.present=present;
        wxAnimationRequestForAnalysis request{key}; unsigned result=2;
        if (slot==0x1C) result=state.vfunc_1C(request);
        else if (slot==0x20) result=state.vfunc_20(request);
        else if (slot==0x30) state.vfunc_30(request);
        else throw std::logic_error("unknown case slot");
        std::ostringstream output;
        output << result << ' ' << request.packedKey << ' ' << Token(state.GetPendingHandleForAnalysis())
            << ' ' << state.GetExitMessageFlagForAnalysis() << ' ' << host.word4 << ' ' << host.word68
            << '|' << host.trace;
        return output.str();
    }
}
int main(int argc, char** argv)
{
    if (argc==9 && std::string(argv[1])=="--case")
    {
        std::uint64_t v[7]{}; for (unsigned i=0;i<7;++i) v[i]=std::stoull(argv[i+2]);
        std::cout << Case(unsigned(v[0]),std::uint32_t(v[1]),v[2],v[3],v[4]!=0,v[5]!=0,v[6]!=0) << '\n';
        return EXIT_SUCCESS;
    }
    wxDispelState state; Host host(state);
    Require(state.GetStateSelectorForAnalysis()==0 && !state.GetExitMessageFlagForAnalysis()
        && state.IsExactly(wxDispelState::ClassID) && state.IsKindOf(wxCharacterState::ClassID),
        "constructor selector, byte3C and RTTI");
    Require(dynamic_cast<wxDispelState*>(spRTTIManager::Instance().Create(wxDispelState::ClassID).get()),
        "registered factory");
    Require(state.vfunc_34(123), "null handle permits transition without host");
    state.SetBindingsForAnalysis(Pointer(1),Pointer(2),&host);
    wxAnimationRequestForAnalysis request{0}; Require(state.vfunc_1C(request),"entry succeeds");
    Require(state.GetExitMessageFlagForAnalysis(),"zero upper field sets flag");
    state.vfunc_40_ResetForAnalysis();
    Require(state.GetExitMessageFlagForAnalysis() && !state.GetPendingHandleForAnalysis(),
        "base reset preserves own flag");
    state.SetPendingHandleForAnalysis(Pointer(11)); host.trace.clear();
    auto clone=state.Clone(); auto* fresh=dynamic_cast<wxDispelState*>(clone.get());
    Require(fresh && !fresh->GetExitMessageFlagForAnalysis() && !fresh->GetPendingHandleForAnalysis(),
        "clone keeps fresh tail and empty binding");
    wxDispelState target; spCloneManager manager;
    Require(state.vfunc_14(target,manager) && !target.GetExitMessageFlagForAnalysis(),
        "empty Copy does not transfer own byte");
    Require(host.trace.empty(),"clone and Copy have no game effects");
    Require(Case(0x1C,0xFFFFFFFF,11,22,false,false,true)==
        "1 4286611328 22 0 0 0|lookup:4286611328@11:0;reset:22@11:0;predicate@11:0;start:22:0:0:1@11:0;word68@22:0;word4@22:0",
        "entry key, flag and ordered control clear without old release");
    Require(Case(0x20,0,11,22,true,true,true)==
        "1 0 0 1 4294967295 4294967295|manager@11:1;message@11:1;predicate@11:1;fade:11@11:1",
        "message precedes release and flag remains set");
    Require(Case(0x20,0,11,22,false,true,false)==
        "1 0 0 1 4294967295 4294967295|manager@11:1;predicate@11:1;stop:11@11:1",
        "absent optional manager skips delivery while release runs");
    std::cout << "Dispel state checks passed\n";
}
