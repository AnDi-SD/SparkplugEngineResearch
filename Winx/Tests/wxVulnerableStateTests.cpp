#include "Code/wxVulnerableState.h"
#include "Analysis/Host/wxVulnerableStateHost.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool value, const char* text) { if (!value) throw std::runtime_error(text); }
    void* Pointer(std::uintptr_t value) { return reinterpret_cast<void*>(value); }
    std::uintptr_t Token(void* value) { return reinterpret_cast<std::uintptr_t>(value); }
    struct Host final : wxVulnerableStateHost
    {
        wxVulnerableState& state; wxAnimationRequestForAnalysis& request;
        std::uintptr_t next = 22, first = 0, second = 0;
        bool predicate = false; unsigned block = 0, managerActive = 0, managerOwner = 0, exitOwnerActive = 0, kind = 24;
        unsigned knee = 255, managerFlag = 255, exitOwnerFlag = 255;
        std::uint32_t control = 0xFFFFFFFF;
        wxVulnerableStateProfileForAnalysis profile = wxVulnerableStateProfileForAnalysis::PC;
        std::string tag = "unknown", trace;
        Host(wxVulnerableState& s, wxAnimationRequestForAnalysis& r) : state(s), request(r) {}
        unsigned Flags() const
        { auto flags = state.GetTransitionFlagsForAnalysis(); unsigned n = 0; for (unsigned i = 0; i < 5; ++i) if (flags[i]) n |= 1u << i; return n; }
        void Event(const std::string& name)
        {
            const auto f = state.GetOwnFieldsForAnalysis();
            if (!trace.empty()) trace += ';';
            trace += name + '@' + std::to_string(Token(state.GetPendingHandleForAnalysis())) + ':' + std::to_string(Flags()) + ':' + std::to_string(request.packedKey)
                + ':' + std::to_string(f.field3C) + ':' + std::to_string(f.field3D) + ':' + std::to_string(f.field3E);
        }
        void* ResolveAnimationForAnalysis(void*, std::uint32_t key) override { Event("lookup:" + std::to_string(key)); return Pointer(next); }
        bool OwnerPredicateForAnalysis(void*) override { Event("predicate"); return predicate; }
        void ResetCompletionForAnalysis(void*, void* h) override { Event("reset:" + std::to_string(Token(h))); }
        void StartAnimationForAnalysis(void*, void* h, bool mode, std::uint32_t fade, bool interrupt) override
        { Event("start:" + std::to_string(Token(h)) + ':' + std::to_string(mode) + ':' + std::to_string(fade) + ':' + std::to_string(interrupt)); }
        void StopAnimationForAnalysis(void*, void* h) override { Event("stop:" + std::to_string(Token(h))); }
        void FadeAnimationForAnalysis(void*, void* h, float value) override { Check(value == 0.4f, "fade word"); Event("fade:" + std::to_string(Token(h))); }
        bool IsPendingAnimationCompleteForAnalysis(void*, void* h, bool consume) override
        {
            Check(consume, "completion consumed"); const auto token = Token(h); Event("query:" + std::to_string(token));
            if (!token) return true;
            const bool result = first == token || second == token;
            if (first == token) first = 0;
            if (second == token) second = 0;
            return result;
        }
        void ClearOwnerActionControlForAnalysis(void*) override { Event("control"); control = 0; }
        wxVulnerableStateProfileForAnalysis ProfileForAnalysis() const noexcept override { return profile; }
        const char* EventTagNameForAnalysis(const void*) override { return tag.c_str(); }
        void LogAnimationTagForAnalysis(const char* value) override { Event("log:" + std::string(value)); }
        std::uint32_t ReadOwnerEventKindForAnalysis(void*) override { Event("kind:" + std::to_string(kind)); return kind; }
        void WriteOwnerKneeFlagForAnalysis(void*, std::uint8_t value) override { Event("knee:" + std::to_string(value)); knee = value; }
        std::uint8_t ReadOwnerExitBlockForAnalysis(void*) override { Event("block:" + std::to_string(block)); return static_cast<std::uint8_t>(block); }
        void* RequireExitManagerForAnalysis() override { return Pointer(0x300); }
        std::uint8_t ReadExitManagerActiveForAnalysis(void*) override { Event("manager-active:" + std::to_string(managerActive)); return static_cast<std::uint8_t>(managerActive); }
        void ClearExitManagerFlagForAnalysis(void*) override { Event("manager-clear"); managerFlag = 0; }
        void* ReadExitManagerOwnerForAnalysis(void*) override { Event("manager-owner:" + std::to_string(managerOwner)); return managerOwner ? Pointer(0x400) : nullptr; }
        std::uint8_t ReadExitOwnerActiveForAnalysis(void*) override { Event("exit-owner-active:" + std::to_string(exitOwnerActive)); return static_cast<std::uint8_t>(exitOwnerActive); }
        void ClearExitOwnerFlagForAnalysis(void*) override { Event("exit-owner-clear"); exitOwnerFlag = 0; }
    };
    std::string Case(const std::uint64_t* v, const std::string& tag, unsigned profile)
    {
        wxVulnerableState state; wxAnimationRequestForAnalysis request{std::uint32_t(v[1])}; Host host(state, request);
        state.SetBindingsForAnalysis(Pointer(0x100), Pointer(0x200), &host);
        state.SetPendingHandleForAnalysis(Pointer(v[2])); host.next = v[3]; host.predicate = v[4] != 0;
        state.SetTransitionFlagsForAnalysis(v[5] & 1, v[5] & 2, v[5] & 4, v[5] & 8, v[5] & 16);
        host.first = v[6]; host.second = v[7]; state.SetSelectorForAnalysis(std::uint32_t(v[8]));
        state.SetOwnFieldsForAnalysis({std::uint8_t(v[9]), std::uint8_t(v[10]), std::uint8_t(v[11])});
        host.block = unsigned(v[12]); host.managerActive = unsigned(v[13]); host.managerOwner = unsigned(v[14]); host.exitOwnerActive = unsigned(v[15]); host.kind = unsigned(v[16]); host.tag = tag;
        host.profile = profile ? wxVulnerableStateProfileForAnalysis::PS2 : wxVulnerableStateProfileForAnalysis::PC;
        unsigned result = 2;
        switch (v[0])
        {
        case 0x1C: result = state.vfunc_1C(request); break;
        case 0x20: result = state.vfunc_20(request); break;
        case 0x30: state.vfunc_30(request); break;
        case 0x34: result = state.vfunc_34(request.packedKey); break;
        case 0x38: result = state.vfunc_38(request.packedKey); break;
        case 0x3C: state.vfunc_3C(nullptr); break;
        case 0x40: state.vfunc_40_ResetForAnalysis(); break;
        default: throw std::logic_error("unknown slot");
        }
        const auto f = state.GetOwnFieldsForAnalysis();
        std::ostringstream out;
        out << result << ' ' << request.packedKey << ' ' << Token(state.GetPendingHandleForAnalysis()) << ' ' << host.control
            << ' ' << host.first << ' ' << host.second << ' ' << host.Flags() << ' ' << unsigned(f.field3C) << ' ' << unsigned(f.field3D) << ' ' << unsigned(f.field3E)
            << ' ' << host.knee << ' ' << host.managerFlag << ' ' << host.exitOwnerFlag << '|' << host.trace;
        return out.str();
    }
    void Lifecycle()
    {
        wxVulnerableState state;
        Check(state.GetStateSelectorForAnalysis() == 28, "constructor selector");
        auto fields = state.GetOwnFieldsForAnalysis();
        Check(fields.field3C == 0 && fields.field3D == 0 && fields.field3E == 1, "constructor own defaults");
        state.SetOwnFieldsForAnalysis({2, 3, 4}); state.SetSelectorForAnalysis(33);
        state.vfunc_40_ResetForAnalysis(); fields = state.GetOwnFieldsForAnalysis();
        Check(fields.field3C == 2 && fields.field3D == 3 && fields.field3E == 4 && state.GetStateSelectorForAnalysis() == 33, "inherited reset preserves own fields/selector");
        spCloneManager manager; auto clone = state.vfunc_10(manager);
        Check(bool(clone) && manager.FindClone(state) == clone.get(), "clone registered");
        auto& s = static_cast<wxVulnerableState&>(*clone); fields = s.GetOwnFieldsForAnalysis();
        Check(s.GetStateSelectorForAnalysis() == 28 && fields.field3C == 0 && fields.field3D == 0 && fields.field3E == 1, "clone retains fresh constructor defaults");
        Check(s.IsExactly(wxVulnerableState::ClassID) && s.IsKindOf(wxCharacterState::ClassID), "RTTI physical base");
        Check(bool(spRTTIManager::Instance().Create(wxVulnerableState::ClassID)), "factory");
        wxAnimationRequestForAnalysis request; bool rejected = false;
        try { state.vfunc_20(request); } catch (const std::logic_error&) { rejected = true; }
        Check(rejected, "missing required host");
    }
}
int main(int argc, char** argv)
{
    try
    {
        if (argc == 21 && std::string(argv[1]) == "--case")
        {
            std::uint64_t v[17]; for (unsigned i = 0; i < 17; ++i) v[i] = std::stoull(argv[i + 2]);
            std::cout << Case(v, argv[19], unsigned(std::stoul(argv[20]))) << '\n'; return 0;
        }
        Lifecycle();
        std::uint64_t v[17]{0x1C, 0x87654321, 11, 22, 0, 31, 0, 0, 28, 0, 0, 1, 0, 1, 1, 1, 24};
        Check(Case(v, "unknown", 0).find("reset:22") != std::string::npos, "entry resets completion");
        v[0] = 0x30;
        Check(Case(v, "unknown", 0).find("reset:22") == std::string::npos, "selector28 update uses mode one");
        v[3] = 11;
        Check(Case(v, "unknown", 0).find("start:") == std::string::npos, "equal handle update avoids queue/release");
        v[0] = 0x3C;
        Check(Case(v, "event_knee_end", 0).find("knee:1") != std::string::npos, "knee end event");
        v[0] = 0x20;
        Check(Case(v, "unknown", 0).find("manager-active:1") < Case(v, "unknown", 0).find("manager-clear"), "PC field read before write");
        Check(Case(v, "unknown", 1).find("manager-clear") < Case(v, "unknown", 1).find("manager-active:1"), "PS2 field write before read");
        std::cout << "PASS Vulnerable state operations and lifecycle\n";
        return 0;
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
