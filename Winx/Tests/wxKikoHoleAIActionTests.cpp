#include "Code/wxKikoHoleAIAction.h"
#include "Code/Sparkplug/spNode.h"
#include "Analysis/PC/wxKikoHoleAIActionAbi.h"
#include <array>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool value, const char* text) { if (!value) throw std::runtime_error(text); }
    void Put(std::array<std::uint8_t, 32>& bytes, std::size_t offset, std::uint32_t value)
    { std::memcpy(bytes.data() + offset, &value, 4); }
    struct Host final : wxKikoHoleAIActionHost
    {
        int owner = 7;
        std::array<std::uint8_t, 0x74> command;
        unsigned reads = 0, writes = 0;
        wxKikoHoleAIAction* action = nullptr;
        std::uint32_t mutatePhase = 0xFFFFFFFF;
        Host() { command.fill(0xA5); }
        void* ReadCommandForAnalysis(void* value) noexcept override
        { if (value != &owner) std::terminate(); ++reads; return command.data(); }
        void WriteCommandByteForAnalysis(void* value, std::uint32_t offset, std::uint8_t byte) noexcept override
        {
            if (value != command.data() || offset != 0x1B || byte > 1) std::terminate();
            command[offset] = byte; ++writes;
            if (mutatePhase != 0xFFFFFFFF) action->StateForAnalysis().phase3B4 = mutatePhase;
        }
    };
    struct World final
    {
        std::array<std::shared_ptr<spNode>, 4> nodes;
        explicit World(unsigned shape, unsigned enabled)
        {
            for (auto& node : nodes) node = std::make_shared<spNode>();
            if (shape == 1)
                for (unsigned i = 1; i != 4; ++i) Check(nodes[0]->AttachChildForAnalysis(nodes[i]), "star child input");
            else if (shape == 2)
                for (unsigned i = 1; i != 4; ++i) Check(nodes[i-1]->AttachChildForAnalysis(nodes[i]), "chain child input");
            else Check(shape == 0, "bounded child graph shape");
            for (unsigned i = 0; i != 4; ++i)
            {
                nodes[i]->SetEnabledForAnalysis((enabled & (1U << i)) != 0, false);
                nodes[i]->SetStaticForAnalysis((i & 1) != 0);
                nodes[i]->SetAnimatedForAnalysis((i & 2) != 0);
                nodes[i]->SetBoneForAnalysis(i == 3);
                nodes[i]->SetBillboardAxisForAnalysis(i % 3);
            }
        }
    };
    template<class Bytes> void Hex(const Bytes& bytes)
    { constexpr char digits[] = "0123456789abcdef"; for (auto b : bytes) std::cout << digits[b >> 4] << digits[b & 15]; }
    void Replay()
    {
        std::string operation;
        while (std::cin >> operation)
        {
            std::uint32_t code, payload, flag, phase, once, target, own, shape, enabled, repeats, mutation;
            std::cin >> code >> payload >> flag >> phase >> once >> target >> own >> shape >> enabled >> repeats >> mutation;
            Check(bool(std::cin) && (operation == "notify" || operation == "update" || operation == "sequence"), "complete Kiko replay input");
            Check(repeats >= 1 && repeats <= 2 && own <= 1 && flag <= 255 && enabled < 16, "bounded Kiko replay values");
            World world(shape, enabled); Host host;
            wxKikoHoleStateForAnalysis input{own ? world.nodes[0].get() : nullptr, world.nodes[3].get(), target, phase, once};
            wxKikoHoleAIAction action(host, input); action.SetOwnerForAnalysis(&host.owner); host.action = &action; host.mutatePhase = mutation;
            std::array<std::uint32_t, 4> beforeFlags{};
            for (unsigned i = 0; i != 4; ++i) beforeFlags[i] = world.nodes[i]->GetFlagsForAnalysis();
            std::array<std::uint8_t, 32> message{}; Put(message, 0, code); Put(message, 0x18, payload); Put(message, 0x1C, 0xA5A5A500 | flag);
            for (unsigned i = 0; i != repeats; ++i)
            {
                if (operation == "notify") action.vfunc_0C(message.data());
                else
                {
                    if (operation == "sequence" && i == 0)
                    {
                        action.vfunc_0C(message.data());
                        Check(action.LastResultForAnalysis() == wxKikoHoleResultForAnalysis::Completed, "whole preceding Kiko Notify completed");
                    }
                    Check(action.vfunc_24(), "whole admitted Kiko Update returns true");
                }
                Check(action.LastResultForAnalysis() == wxKikoHoleResultForAnalysis::Completed, "admitted Kiko operation completed");
            }
            const auto& state = action.StateForAnalysis();
            Check(state.ownNode == input.ownNode && state.playerNode == input.playerNode, "typed borrowed links preserved");
            std::cout << state.targetWord3B0 << ' ' << state.phase3B4 << ' ' << state.once3B8;
            for (const auto flags : beforeFlags) std::cout << ' ' << flags;
            for (const auto& node : world.nodes) std::cout << ' ' << node->GetFlagsForAnalysis();
            std::cout << ' '; Hex(host.command); std::cout << ' ' << host.reads << ' ' << host.writes << '\n';
        }
    }
    void EqualParentReplay()
    {
        std::uint32_t identity, shape, enabled, repeats, notify, flag, phase, once, mutation;
        while (std::cin >> identity >> shape >> enabled >> repeats >> notify >> flag >> phase >> once >> mutation)
        {
            Check(identity != 0 && shape < 3 && enabled < 16 && repeats >= 1 && repeats <= 2
                && notify <= 1 && flag >= 1 && flag <= 255, "complete equal-parent replay input");
            World world(shape, enabled); Host host; auto parent = std::make_shared<spNode>();
            Check(parent->AttachChildForAnalysis(world.nodes[0]), "established equal-parent input");
            parent->SetEnabledForAnalysis(false, false); parent->SetStaticForAnalysis(false);
            parent->SetAnimatedForAnalysis(false); parent->SetBoneForAnalysis(false); parent->SetBillboardAxisForAnalysis(1);
            wxKikoHoleStateForAnalysis input{world.nodes[0].get(), world.nodes[3].get(), identity, phase, once,
                {identity, parent.get()}};
            wxKikoHoleAIAction action(host, input); action.SetOwnerForAnalysis(&host.owner);
            host.action = &action; host.mutatePhase = mutation;
            std::array<std::uint32_t, 5> beforeFlags{};
            for (unsigned i = 0; i != 4; ++i) beforeFlags[i] = world.nodes[i]->GetFlagsForAnalysis();
            beforeFlags[4] = parent->GetFlagsForAnalysis();
            std::array<std::uint8_t, 32> message{};
            Put(message, 0, 0x2738); Put(message, 0x18, identity); Put(message, 0x1C, 0xA5A5A500 | flag);
            if (notify) action.vfunc_0C(message.data());
            for (unsigned i = 0; i != repeats; ++i) Check(action.vfunc_24(), "whole equal-parent Update admitted");
            const auto& state = action.StateForAnalysis();
            Check(state.ownNode == world.nodes[0].get() && state.playerNode == world.nodes[3].get()
                && state.equalParentBinding.parent == parent.get() && state.equalParentBinding.nativeWord == identity,
                "borrowed binding is preserved independently of raw notification word");
            Check(world.nodes[0]->GetParentForAnalysis() == parent.get() && parent->GetParentForAnalysis() == nullptr
                && parent->GetChildCountForAnalysis() == 1 && parent->GetChildForAnalysis(0) == world.nodes[0].get()
                && parent->GetFlagsForAnalysis() == beforeFlags[4], "parent topology and flags preserved");
            std::cout << state.targetWord3B0 << ' ' << state.phase3B4 << ' ' << state.once3B8;
            for (auto flags : beforeFlags) std::cout << ' ' << flags;
            for (const auto& node : world.nodes) std::cout << ' ' << node->GetFlagsForAnalysis();
            std::cout << ' ' << parent->GetFlagsForAnalysis() << ' '; Hex(host.command);
            std::cout << ' ' << host.reads << ' ' << host.writes << '\n';
        }
    }
    struct DerivedNode final : spNode {};
    void EqualParentContracts()
    {
        World world(2, 0); auto parent = std::make_shared<spNode>();
        Check(parent->AttachChildForAnalysis(world.nodes[0]), "captured parent graph");
        const wxKikoHoleStateForAnalysis input{world.nodes[0].get(), world.nodes[3].get(), 0x31ABCDEF, 2, 0,
            {0x31ABCDEF, parent.get()}};
        const auto refuses = [&](const wxKikoHoleStateForAnalysis& value)
        {
            Host host; wxKikoHoleAIAction action(host, value); action.SetOwnerForAnalysis(&host.owner);
            const auto flags = world.nodes[0]->GetFlagsForAnalysis(); const auto parentFlags = parent->GetFlagsForAnalysis();
            Check(!action.vfunc_24() && action.LastResultForAnalysis() == wxKikoHoleResultForAnalysis::UnqualifiedPhase2,
                "incomplete/stale/nonplain binding refuses");
            Check(host.reads == 0 && host.writes == 0 && action.StateForAnalysis().once3B8 == 0
                && world.nodes[0]->GetFlagsForAnalysis() == flags && parent->GetFlagsForAnalysis() == parentFlags,
                "binding refusal precedes Node/command/latch effects");
        };
        auto bad = input; bad.equalParentBinding = {}; refuses(bad);
        bad = input; bad.equalParentBinding.nativeWord = 0; refuses(bad);
        bad = input; bad.equalParentBinding.parent = nullptr; refuses(bad);
        bad = input; ++bad.targetWord3B0; refuses(bad);
        auto other = std::make_shared<spNode>();
        bad = input; bad.equalParentBinding.parent = other.get(); refuses(bad);
        bad = input; bad.targetWord3B0 = 0; refuses(bad);
        auto extra = std::make_shared<spNode>();
        Check(world.nodes[3]->AttachChildForAnalysis(extra), "oversized captured subtree");
        refuses(input); Check(bool(world.nodes[3]->DetachChildForAnalysis(*extra)), "retire extra child");
        auto derived = std::make_shared<DerivedNode>(); auto plainChild = std::make_shared<spNode>();
        Check(derived->AttachChildForAnalysis(plainChild), "foreign parent input");
        bad = input; bad.ownNode = plainChild.get(); bad.equalParentBinding.parent = derived.get(); refuses(bad);
        Host host; wxKikoHoleAIAction action(host, input); action.SetOwnerForAnalysis(&host.owner); host.action = &action;
        const auto parentFlags = parent->GetFlagsForAnalysis();
        Check(action.vfunc_24() && action.StateForAnalysis().once3B8 == 1 && host.reads == 1 && host.writes == 1,
            "typed equal parent enables own subtree and writes command exactly once");
        for (const auto& node : world.nodes) Check(node->IsEnabledForAnalysis(), "all own descendants enabled");
        Check(parent->GetFlagsForAnalysis() == parentFlags && action.vfunc_24() && host.writes == 1,
            "parent stays unchanged and repeated latched Update has no effect");
    }
    struct Observer final : wxAIAction
    { unsigned notifications = 0; void vfunc_0C(const void*) noexcept override { ++notifications; } };
    void Contracts()
    {
        Host host; World world(2, 15);
        wxKikoHoleStateForAnalysis captured{world.nodes[0].get(), world.nodes[3].get(), 0x12345678, 2, 0};
        wxKikoHoleAIAction action(host, captured), destination(host, captured);
        Check(action.IsKindOf(wxAIAction::ClassID) && wxKikoHoleAIAction::StaticRTTI().factory == nullptr,
            "typed RTTI but no native constructor/factory claim");
        Check(!action.vfunc_24() && action.LastResultForAnalysis() == wxKikoHoleResultForAnalysis::UnqualifiedPhase2,
            "active reparent phase refuses rather than replacing its body");
        Check(action.StateForAnalysis().once3B8 == 0 && host.reads == 0 && host.writes == 0, "phase2 refusal precedes effects");
        Check(!action.vfunc_2C() && action.LastResultForAnalysis() == wxKikoHoleResultForAnalysis::UnqualifiedEntry,
            "Entry subscription has explicit boundary");
        spCloneManager clones;
        Check(!action.vfunc_10(clones) && !action.vfunc_14(destination, clones), "unsupported Clone and Copy refuse");
        Check(destination.StateForAnalysis().targetWord3B0 == captured.targetWord3B0, "lifecycle refusals preserve destination");
        Observer observer; action.SetCurrentActionForAnalysis(&observer);
        const wxAIActionMessageForAnalysis unknown{0x1C}; action.vfunc_0C(&unknown);
        Check(observer.notifications == 0 && action.StateForAnalysis().phase3B4 == 2, "own unknown Notify does not forward or read payload");
        action.SetCurrentActionForAnalysis(nullptr); // borrowed observer dies before action
        action.StateForAnalysis().phase3B4 = 1; action.StateForAnalysis().ownNode = nullptr;
        Check(!action.vfunc_24() && action.LastResultForAnalysis() == wxKikoHoleResultForAnalysis::UnqualifiedNode,
            "missing required plain Node is not successful host behavior");
        action.StateForAnalysis().once3B8 = 7;
        Check(action.vfunc_24(), "already-latched phase1 does not require a Node");
        const auto flags = world.nodes[1]->GetFlagsForAnalysis();
        wxKikoHoleStateForAnalysis nonroot{world.nodes[1].get(), world.nodes[3].get(), 0, 2, 0};
        wxKikoHoleAIAction blocked(host, nonroot); blocked.SetOwnerForAnalysis(&host.owner);
        Check(!blocked.vfunc_24() && blocked.LastResultForAnalysis() == wxKikoHoleResultForAnalysis::UnqualifiedPhase2,
            "zero payload with a nonnull parent still refuses the reparent branch");
        Check(world.nodes[1]->GetFlagsForAnalysis() == flags && host.reads == 0 && host.writes == 0,
            "unsupported phase2 refuses before Node or command effects");
    }
}
int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--replay") Replay();
        else if (argc == 2 && std::string(argv[1]) == "--equal-parent-replay") EqualParentReplay();
        else { Contracts(); EqualParentContracts(); std::cout << "KikoHole bounded own contracts passed\n"; }
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
