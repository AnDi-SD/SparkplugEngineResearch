#include "Analysis/PC/wxCharacterStateAbi.h"
#include "Analysis/PS2/wxCharacterStateAbi.h"
#include "Code/wxActionState.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
    using namespace winx::reconstruction;

    void Require(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    std::uintptr_t Token(void* value) { return reinterpret_cast<std::uintptr_t>(value); }
    void* Pointer(std::uintptr_t value) { return reinterpret_cast<void*>(value); }

    unsigned Flags(const wxCharacterState& state)
    {
        unsigned bits = 0, shift = 0;
        for (bool flag : state.GetTransitionFlagsForAnalysis())
            bits |= static_cast<unsigned>(flag) << shift++;
        return bits;
    }

    // Borrowed tokens are never dereferenced. Each event records the state
    // visible to the external callee, including ordering of handle/flag writes.
    class Host final : public wxCharacterStateHost
    {
    public:
        explicit Host(wxCharacterState& value) : state(value) {}
        wxCharacterState& state;
        bool complete = false;
        bool predicate = false;
        void* next = nullptr;
        unsigned control = 0xFFFFFFFF;
        std::string trace;

        void Event(const std::string& text)
        {
            if (!trace.empty()) trace += ';';
            trace += text + '@' + std::to_string(Token(state.GetPendingHandleForAnalysis()))
                + ':' + std::to_string(Flags(state));
        }
        void CheckOwner(void* owner) { Require(Token(owner) == 0x100, "borrowed owner"); }
        void CheckConsumer(void* consumer) { Require(Token(consumer) == 0x200, "borrowed consumer"); }
        void* ResolveAnimationForAnalysis(void* owner, std::uint32_t key) override
        {
            CheckOwner(owner); Event("lookup:" + std::to_string(key)); return next;
        }
        bool OwnerPredicateForAnalysis(void* owner) override
        {
            CheckOwner(owner); Event("predicate"); return predicate;
        }
        void ResetCompletionForAnalysis(void* consumer, void* handle) override
        {
            CheckConsumer(consumer); Event("reset:" + std::to_string(Token(handle)));
        }
        void StartAnimationForAnalysis(void* consumer, void* handle, bool mode,
            std::uint32_t fade, bool interrupt) override
        {
            CheckConsumer(consumer);
            Event("start:" + std::to_string(Token(handle)) + ':' + std::to_string(mode)
                + ':' + std::to_string(fade) + ':' + std::to_string(interrupt));
        }
        void StopAnimationForAnalysis(void* consumer, void* handle) override
        {
            CheckConsumer(consumer); Event("stop:" + std::to_string(Token(handle)));
        }
        void FadeAnimationForAnalysis(void* consumer, void* handle, float duration) override
        {
            CheckConsumer(consumer);
            std::uint32_t bits = 0;
            static_assert(sizeof(bits) == sizeof(duration));
            std::memcpy(&bits, &duration, sizeof(bits));
            Event("fade:" + std::to_string(Token(handle)) + ':' + std::to_string(bits));
        }
        bool IsPendingAnimationCompleteForAnalysis(void* consumer, void* handle, bool flag) override
        {
            CheckConsumer(consumer);
            Event("query:" + std::to_string(Token(handle)) + ':' + std::to_string(flag));
            return complete;
        }
        void ClearOwnerActionControlForAnalysis(void* owner) override
        {
            CheckOwner(owner); Event("clear"); control = 0;
        }
    };

    int Protocol()
    {
        unsigned slot, key, flags, pending, next, complete, predicate;
        while (std::cin >> slot >> key >> flags >> pending >> next >> complete >> predicate)
        {
            wxActionState state;
            Host host(state);
            state.SetBindingsForAnalysis(Pointer(0x100), Pointer(0x200), &host);
            state.SetTransitionFlagsForAnalysis((flags & 1) != 0, (flags & 2) != 0,
                (flags & 4) != 0, (flags & 8) != 0, (flags & 16) != 0);
            state.SetPendingHandleForAnalysis(Pointer(pending));
            host.next = Pointer(next);
            host.complete = complete != 0;
            host.predicate = predicate != 0;
            wxAnimationRequestForAnalysis request{key};
            unsigned result = 2; // Void slots have no defined native return value.
            switch (slot)
            {
            case 0x1C: result = state.vfunc_1C(request); break;
            case 0x20: result = state.vfunc_20(request); break;
            case 0x24: state.vfunc_24(); break;
            case 0x28: state.vfunc_28(request); break;
            case 0x2C: state.vfunc_2C(request); break;
            case 0x30: state.vfunc_30(request); break;
            case 0x40: state.vfunc_40_ResetForAnalysis(); break;
            default: Require(false, "unknown protocol slot");
            }
            std::cout << result << ' ' << request.packedKey << ' '
                << Token(state.GetPendingHandleForAnalysis()) << ' ' << Flags(state)
                << ' ' << host.control << '|' << host.trace << '\n';
        }
        Require(std::cin.eof(), "malformed protocol input");
        return EXIT_SUCCESS;
    }
}

int main(int argc, char** argv)
{
    using namespace winx::reconstruction;
    using namespace sparkplug::reconstruction;
    if (argc == 2 && std::string(argv[1]) == "--protocol") return Protocol();

    Require(sizeof(winx::evidence::pc::wxCharacterStateLayout) == 0x3C
        && sizeof(winx::evidence::ps2::wxCharacterStateLayout) == 0x3C, "paired layouts");
    wxActionState state;
    Require(state.GetStateSelectorForAnalysis() == 25 && Flags(state) == 31, "factory defaults");
    Require(state.IsExactly(wxActionState::ClassID) && state.IsKindOf(wxCharacterState::ClassID)
        && state.IsKindOf(spBaseObject::ClassID), "RTTI ancestry");
    Require(state.vfunc_1C(wxActionState::ClassID) && state.vfunc_20(wxCharacterState::ClassID),
        "RTTI overloads remain accessible");
    auto created = spRTTIManager::Instance().Create(wxActionState::ClassID);
    Require(dynamic_cast<wxActionState*>(created.get()) != nullptr, "registered factory");

    Host host(state);
    state.SetBindingsForAnalysis(Pointer(0x100), Pointer(0x200), &host);
    wxAnimationRequestForAnalysis request{0xFFFFFFFF};
    host.next = Pointer(11);
    Require(!state.vfunc_1C(request) && state.GetPendingHandleForAnalysis() == Pointer(11)
        && Flags(state) == 30 && request.packedKey == 0xF027FF8F, "entry transition starts");
    host.trace.clear();
    Require(!state.vfunc_1C(request) && host.trace.find("start:") == std::string::npos,
        "entry waits without replay");
    host.complete = true;
    host.predicate = true;
    host.next = Pointer(22);
    host.trace.clear();
    Require(state.vfunc_1C(request) && state.GetPendingHandleForAnalysis() == Pointer(22),
        "completed entry advances to base and virtual slot 30");
    Require(host.trace.find("fade:11:1053609165") != std::string::npos
        && host.trace.find("start:22:1:2:1") != std::string::npos, "release and restart arguments");
    host.complete = false;
    host.next = Pointer(33);
    Require(!state.vfunc_20(request) && request.packedKey == 0xF047FF8F, "exit starts");
    host.complete = true;
    Require(state.vfunc_20(request) && state.GetPendingHandleForAnalysis() == nullptr,
        "completed exit releases the last handle");

    state.SetTransitionFlagsForAnalysis(false, false, false, false, false);
    state.SetPendingHandleForAnalysis(Pointer(44));
    auto clone = state.Clone();
    auto* actionClone = dynamic_cast<wxActionState*>(clone.get());
    Require(actionClone && actionClone->GetStateSelectorForAnalysis() == 25
        && Flags(*actionClone) == 31 && !actionClone->GetPendingHandleForAnalysis(),
        "clone preserves factory defaults, not runtime state");
    bool unboundRejected = false;
    try { actionClone->vfunc_30(request); }
    catch (const std::logic_error&) { unboundRejected = true; }
    Require(unboundRejected, "clone does not copy host bindings or fake successful playback");
    wxActionState destination;
    destination.SetPendingHandleForAnalysis(Pointer(55));
    spCloneManager manager;
    Require(state.vfunc_14(destination, manager)
        && destination.GetPendingHandleForAnalysis() == Pointer(55), "inherited copy is empty");
    state.vfunc_40_ResetForAnalysis();
    Require(Flags(state) == 7 && !state.GetPendingHandleForAnalysis()
        && state.GetStateSelectorForAnalysis() == 25, "reset preserves transition enables and selector");
    Require(state.vfunc_34(request) && !state.vfunc_38(request), "constant permission slots");

    wxCharacterState base;
    Require(base.GetStateSelectorForAnalysis() == 0 && base.vfunc_1C(request)
        && base.vfunc_20(request), "base default hooks");
    base.vfunc_24(); base.vfunc_28(request); base.vfunc_2C(request); base.vfunc_3C(request);
    Require(base.Clone()->IsExactly(wxCharacterState::ClassID), "base clone type");

    // Every bit of the caller-owned key is covered independently. Repeated
    // slot 30 calls with the same result must still clear owner control.
    for (unsigned bit = 0; bit < 32; ++bit)
    {
        for (unsigned slot : {0x1Cu, 0x20u, 0x30u})
        {
            wxActionState sample;
            Host adapter(sample);
            sample.SetBindingsForAnalysis(Pointer(0x100), Pointer(0x200), &adapter);
            adapter.next = Pointer(11);
            wxAnimationRequestForAnalysis key{std::uint32_t{1} << bit};
            const auto before = key.packedKey;
            if (slot == 0x1C)
            {
                Require(!sample.vfunc_1C(key), "entry bit case");
                Require(key.packedKey == ((before & 0xF027FF8F) | 0x00200000), "entry mask");
            }
            else if (slot == 0x20)
            {
                Require(!sample.vfunc_20(key), "exit bit case");
                Require(key.packedKey == ((before & 0xF047FF8F) | 0x00400000), "exit mask");
            }
            else
            {
                sample.vfunc_30(key);
                Require(key.packedKey == (before & 0xF007FF8F), "ordinary mask");
                adapter.trace.clear();
                adapter.control = 0xFFFFFFFF;
                sample.vfunc_30(key);
                Require(adapter.trace == "lookup:" + std::to_string(key.packedKey)
                    + "@11:31;clear@11:31" && adapter.control == 0,
                    "unchanged handle skips playback but not owner control clear");
            }
        }
    }

    // Original base hooks release before dispatching slot 30; the handle
    // remains visible during the stop/fade callback and is cleared afterward.
    for (unsigned slot : {0x24u, 0x28u, 0x2Cu})
    {
        for (bool fade : {false, true})
        {
            wxActionState sample;
            Host adapter(sample);
            sample.SetBindingsForAnalysis(Pointer(0x100), Pointer(0x200), &adapter);
            sample.SetPendingHandleForAnalysis(Pointer(11));
            adapter.next = Pointer(22);
            adapter.predicate = fade;
            wxAnimationRequestForAnalysis key{0};
            if (slot == 0x24) sample.vfunc_24();
            if (slot == 0x28) sample.vfunc_28(key);
            if (slot == 0x2C) sample.vfunc_2C(key);
            std::string expected = "predicate@11:31;";
            expected += fade ? "fade:11:1053609165@11:31" : "stop:11@11:31";
            if (slot == 0x2C)
                expected += std::string(";lookup:0@0:31;predicate@0:31;start:22:1:")
                    + (fade ? "2" : "0") + ":1@0:31;clear@22:31";
            Require(adapter.trace == expected, "base release and virtual dispatch ordering");
        }
    }
    std::cout << "wxActionState reconstruction tests passed\n";
    return EXIT_SUCCESS;
}
