#include "Code/wxBeforeTrollFightState.h"
#include "Code/SparkBase/spSubscriptionManager.h"
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    void Require(bool value, const char* message)
    { if (!value) { std::cerr << message << '\n'; std::exit(EXIT_FAILURE); } }
    void* Pointer(std::uintptr_t value) { return reinterpret_cast<void*>(value); }
    std::uintptr_t Token(void* value) { return reinterpret_cast<std::uintptr_t>(value); }
    unsigned Flags(const wxCharacterState& state)
    {
        unsigned result = 0; const auto flags = state.GetTransitionFlagsForAnalysis();
        for (unsigned i = 0; i < 5; ++i) if (flags[i]) result |= 1u << i;
        return result;
    }
    struct Host final : wxBeforeTrollFightStateHost
    {
        wxBeforeTrollFightState* state = nullptr;
        wxAnimationRequestForAnalysis request{0x13572468};
        wxBeforeTrollProfileForAnalysis profile = wxBeforeTrollProfileForAnalysis::PC;
        std::vector<unsigned> random{0, 1499};
        std::size_t randomIndex = 0;
        unsigned clock = 100, children = 2;
        bool predicate = false, resetObject = true;
        bool integrateManager = false;
        spSubscriptionManager subscriptions;
        std::string trace;
        void Event(const std::string& name)
        {
            if (!trace.empty()) trace += ';';
            const auto fields = state->GetFieldsForAnalysis();
            trace += name + '@' + std::to_string(Token(state->GetPendingHandleForAnalysis())) + ':'
                + std::to_string(Flags(*state)) + ':' + std::to_string(request.packedKey) + ':'
                + std::to_string(fields.deadline) + ':' + std::to_string(Token(fields.handle40)) + ':'
                + std::to_string(Token(fields.handle44)) + ':' + std::to_string(Token(fields.handle48));
        }
        wxBeforeTrollProfileForAnalysis ProfileForAnalysis() const noexcept override { return profile; }
        unsigned RandomWordForAnalysis() override
        { Require(randomIndex < random.size(), "random input exhausted"); const auto value = random[randomIndex++]; Event("random:" + std::to_string(value)); return value; }
        unsigned ClockWordForAnalysis() override { Event("clock"); return clock; }
        void AddCompletionSubscriberForAnalysis(void* consumer, wxBeforeTrollFightState& s) override
        { Require(consumer == Pointer(0x200) && &s == state, "consumer add binding"); Event("consumer-add"); }
        void RemoveCompletionSubscriberForAnalysis(void* consumer, wxBeforeTrollFightState& s) noexcept override
        { Require(consumer == Pointer(0x200) && &s == state, "consumer remove binding"); Event("consumer-remove"); }
        void SubscribeForAnalysis(unsigned key, wxBeforeTrollFightState& s) override
        { Require(key == 5 && &s == state, "subscription add"); Event("manager-add"); if (integrateManager) (void)subscriptions.SubscribeForAnalysis(key, s); }
        void UnsubscribeForAnalysis(unsigned key, wxBeforeTrollFightState& s) noexcept override
        { Require(key == 5 && &s == state, "subscription remove"); Event("manager-remove"); if (integrateManager) (void)subscriptions.UnsubscribeForAnalysis(key, s); }
        void* OwnerNodeForAnalysis(void*) override { return Pointer(0x400); }
        void InvokeNodeForAnalysis(void* node, unsigned a, unsigned b) override
        { Require(node == Pointer(0x400) && !a && b == 1, "node call"); Event("node:0:1"); }
        bool NodeHasChildrenForAnalysis(void*) override { return children != 0; }
        void* FirstNodeChildForAnalysis(void*) override { return children == 2 ? Pointer(0x500) : nullptr; }
        void WriteChildByteForAnalysis(void* child, unsigned offset, std::uint8_t value) override
        { Require(child == Pointer(0x500) && !value, "child write"); Event("child:" + std::to_string(offset)); }
        void* OwnerResetObjectForAnalysis(void*) override { return resetObject ? Pointer(0x600) : nullptr; }
        void ResetPCObjectForAnalysis(void* object) override
        { Require(object == Pointer(0x600), "reset object"); Event("reset-object"); }
        void WritePS2ResetWordForAnalysis(void* object, unsigned offset, unsigned value) override
        { Require(object == Pointer(0x600) && !value, "EE reset"); Event("reset-word:" + std::to_string(offset)); }
        void* ResolveAnimationForAnalysis(void*, unsigned key) override
        { Event("lookup:" + std::to_string(key)); return Pointer(key == 0 ? 11 : key == 0x800000 ? 22 : 33); }
        bool OwnerPredicateForAnalysis(void*) override { Event("predicate"); return predicate; }
        void ResetCompletionForAnalysis(void*, void* handle) override { Event("reset:" + std::to_string(Token(handle))); }
        void StartAnimationForAnalysis(void*, void* handle, bool mode, unsigned fade, bool interrupt) override
        { Event("start:" + std::to_string(Token(handle)) + ':' + std::to_string(mode) + ':' + std::to_string(fade) + ':' + std::to_string(interrupt)); }
        void StopAnimationForAnalysis(void*, void* handle) override { Event("stop:" + std::to_string(Token(handle))); }
        void FadeAnimationForAnalysis(void*, void* handle, float duration) override
        { Require(duration == 0.4f, "native fade"); Event("fade:" + std::to_string(Token(handle))); }
        bool IsPendingAnimationCompleteForAnalysis(void*, void*, bool) override
        { throw std::logic_error("unexpected completion query"); }
        void ClearOwnerActionControlForAnalysis(void*) override
        { throw std::logic_error("unexpected owner control write"); }
    };
    std::string Case(const std::uint64_t* v)
    {
        Host host; unsigned result = 2, pending = 0, flags = 0;
        wxBeforeTrollFightState::FieldsForAnalysis fields{};
        {
            wxBeforeTrollFightState state; host.state = &state;
            state.BindForAnalysis(Pointer(0x100), Pointer(0x200), host);
            state.SetPendingHandleForAnalysis(Pointer(v[1]));
            state.SetTransitionFlagsForAnalysis(v[2]&1, v[2]&2, v[2]&4, v[2]&8, v[2]&16);
            state.SetFieldsForAnalysis({unsigned(v[3]), Pointer(v[4]), Pointer(v[5]), Pointer(v[6])});
            host.random = {unsigned(v[7]), unsigned(v[8])}; host.clock = unsigned(v[9]);
            host.predicate = v[10] != 0; host.children = unsigned(v[14]); host.resetObject = v[15] != 0;
            host.profile = v[16] ? wxBeforeTrollProfileForAnalysis::PS2 : wxBeforeTrollProfileForAnalysis::PC;
            wxBeforeTrollNotificationForAnalysis packet{unsigned(v[11]), unsigned(v[12]), Pointer(v[13])};
            if (v[0] == 0x1C) result = state.vfunc_1C(host.request);
            else if (v[0] == 0x30) state.vfunc_30(host.request);
            else if (v[0] == 0x0C) state.vfunc_0C(&packet);
            else if (v[0] == 0x34) result = state.vfunc_34(unsigned(v[11]));
            else if (v[0] != 0) throw std::logic_error("BeforeTrollFight slot");
            pending = unsigned(Token(state.GetPendingHandleForAnalysis())); flags = Flags(state); fields = state.GetFieldsForAnalysis();
        }
        std::ostringstream out;
        out << result << ' ' << host.request.packedKey << ' ' << pending << ' ' << flags << ' ' << fields.deadline
            << ' ' << Token(fields.handle40) << ' ' << Token(fields.handle44) << ' ' << Token(fields.handle48) << '|' << host.trace;
        return out.str();
    }
}
int main(int argc, char** argv)
{
    if (argc == 19 && std::string(argv[1]) == "--case")
    { std::uint64_t v[17]{}; for (unsigned i = 0; i < 17; ++i) v[i] = std::stoull(argv[i+2]); std::cout << Case(v) << '\n'; return EXIT_SUCCESS; }
    wxBeforeTrollFightState disconnected;
    Require(disconnected.GetStateSelectorForAnalysis() == 8 && !disconnected.GetFieldsForAnalysis().deadline, "constructor");
    Require(dynamic_cast<wxBeforeTrollFightState*>(spRTTIManager::Instance().Create(wxBeforeTrollFightState::ClassID).get()), "RTTI factory");
    bool missingBinding = false; wxAnimationRequestForAnalysis request;
    try { disconnected.vfunc_30(request); } catch (const std::logic_error&) { missingBinding = true; }
    Require(missingBinding, "unbound operation must fail");
    Host host; host.integrateManager = true;
    {
        wxBeforeTrollFightState state; host.state = &state; state.BindForAnalysis(Pointer(0x100), Pointer(0x200), host);
        Require(state.vfunc_1C(host.request) && !state.GetPendingHandleForAnalysis() && state.GetFieldsForAnalysis().deadline == 2599, "native entry releases its own playback");
        Require(host.trace.find("consumer-add") < host.trace.find("manager-add") && !(Flags(state) & 2), "registration order and once flag");
        host.trace.clear(); host.clock = 2600; state.vfunc_30(host.request);
        Require(state.GetPendingHandleForAnalysis() == Pointer(33) && !state.GetFieldsForAnalysis().deadline, "expired deadline");
        host.randomIndex = 0; host.random = {0x80000000, 1501}; host.clock = 0xFFFFFF00;
        wxBeforeTrollNotificationForAnalysis packet{3, 0, Pointer(33)};
        host.subscriptions.DispatchForAnalysis(5, &packet);
        Require(state.GetPendingHandleForAnalysis() == Pointer(22) && state.GetFieldsForAnalysis().deadline == 744, "manager notification and uint32 deadline wrap");
        const auto saved = state.GetFieldsForAnalysis(); state.vfunc_40_ResetForAnalysis();
        Require(state.GetFieldsForAnalysis().deadline == saved.deadline && state.GetFieldsForAnalysis().handle44 == saved.handle44, "inherited reset preserves own fields");
        auto clone = state.Clone(); auto* fresh = dynamic_cast<wxBeforeTrollFightState*>(clone.get());
        Require(fresh && !fresh->GetFieldsForAnalysis().deadline && !fresh->GetFieldsForAnalysis().handle40 && Flags(*fresh) == 31, "fresh disconnected clone");
        wxBeforeTrollFightState target; target.SetFieldsForAnalysis(saved); spCloneManager manager;
        Require(state.vfunc_14(target, manager) && target.GetFieldsForAnalysis().deadline == saved.deadline, "native empty copy");
        bool rebound = false; try { state.BindForAnalysis(Pointer(1), Pointer(2), host); } catch (const std::logic_error&) { rebound = true; }
        Require(rebound, "active lifetime cannot be rebound");
        host.trace.clear();
    }
    Require(host.trace.find("consumer-remove") == 0 && host.trace.find("manager-remove") != std::string::npos, "destructor always removes consumer then manager");
    const auto afterDestroy = host.trace; wxBeforeTrollNotificationForAnalysis packet{3, 0, Pointer(33)};
    host.subscriptions.DispatchForAnalysis(5, &packet); Require(host.trace == afterDestroy, "no dangling manager subscriber");
    host.trace.clear();
    { wxBeforeTrollFightState state; host.state = &state; state.BindForAnalysis(Pointer(0x100), Pointer(0x200), host); }
    Require(host.trace.find("consumer-remove") == 0 && host.trace.find("manager-remove") != std::string::npos, "destruction without entry still unsubscribes");
    std::cout << "BeforeTrollFight checks passed\n";
}
