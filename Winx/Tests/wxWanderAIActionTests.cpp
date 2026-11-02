#include "Code/wxWanderAIAction.h"
#include "Analysis/PC/wxWanderAIActionAbi.h"
#include "Code/Sparkplug/spNode.h"
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool value, const char* label) { if (!value) throw std::runtime_error(label); }
    float Float(std::uint32_t word) { float value; std::memcpy(&value, &word, 4); return value; }
    std::string Hex(const wxWanderCapturedStateForAnalysis& state)
    {
        std::ostringstream out; out << std::hex << std::setfill('0');
        for (auto word : state.nativeTailWords3A8)
            for (unsigned byte = 0; byte != 4; ++byte) out << std::setw(2) << ((word >> (byte * 8)) & 255);
        return out.str();
    }
    wxWanderCapturedStateForAnalysis Parse(const std::string& text)
    {
        Check(text.size() == 0x68 * 2, "Complete104-byte captured tail");
        wxWanderCapturedStateForAnalysis state{};
        for (unsigned i = 0; i != 104; ++i)
            state.nativeTailWords3A8[i / 4] |= std::stoul(text.substr(i * 2, 2), nullptr, 16) << ((i % 4) * 8);
        return state;
    }
    struct Entity { spNode* node; };
    struct Owner { Entity* entity; };
    struct Host final : wxWanderAIActionHost
    {
        bool admitted = true;
        wxWanderAIAction* action = nullptr;
        unsigned reads = 0;
        bool clearsBeforeRead = true;
        ProfileForAnalysis EntryProfileForAnalysis() const noexcept override
        { return admitted ? ProfileForAnalysis::PcCapturedBorrowedPosition : ProfileForAnalysis::Unqualified; }
        std::optional<spNode*> ReadOwnerNodeForAnalysis(void* raw) noexcept override
        {
            ++reads;
            const auto& tail = action->CapturedStateForAnalysis().nativeTailWords3A8;
            clearsBeforeRead &= tail[4] == 0 && tail[5] == 0;
            auto* owner = static_cast<Owner*>(raw);
            if (!owner->entity) return std::nullopt;
            return owner->entity->node;
        }
    };
    struct Fixture
    {
        spNode node;
        Entity entity{&node};
        Owner owner{&entity};
        Host host;
        std::unique_ptr<wxWanderAIAction> action;
        explicit Fixture(const wxWanderCapturedStateForAnalysis& state)
            : action(std::make_unique<wxWanderAIAction>(host, &owner, state)) { host.action = action.get(); }
    };
    void Replay()
    {
        std::string tail;
        std::uint32_t x, y, z, duration, clearable, calls;
        while (std::cin >> tail >> x >> y >> z >> duration >> clearable >> calls)
        {
            Check(calls == 1 || calls == 2, "Bounded replay call count");
            Fixture f(Parse(tail)); f.node.SetPositionForAnalysis({Float(x), Float(y), Float(z)});
            f.action->SetDurationMillisecondsForAnalysis(duration); f.action->SetClearableWordForAnalysis(clearable);
            const auto before = Hex(f.action->CapturedStateForAnalysis());
            for (unsigned i = 0; i != calls; ++i) { f.action->vfunc_30(); Check(f.action->LastResultForAnalysis() == wxWanderResultForAnalysis::Completed, "Admitted Entry completes"); }
            std::cout << "{\"beforeTail\":\"" << before << "\",\"afterTail\":\"" << Hex(f.action->CapturedStateForAnalysis())
                << "\",\"duration\":" << f.action->GetDurationMillisecondsForAnalysis() << ",\"clearable\":" << f.action->GetClearableWordForAnalysis()
                << ",\"ownerStable\":" << (f.action->GetOwnerForAnalysis() == &f.owner ? "true" : "false")
                << ",\"reads\":" << f.host.reads << ",\"clearsBeforeRead\":" << (f.host.clearsBeforeRead ? "true" : "false") << "}\n";
        }
        Check(std::cin.eof(), "Complete Wander replay record");
    }
    void Contracts()
    {
        static_assert(!std::is_copy_constructible_v<wxWanderAIAction> && !std::is_move_constructible_v<wxWanderAIAction>);
        static_assert(sizeof(winx::evidence::pc::wxWanderAIActionLayout) == 0x410);
        wxWanderCapturedStateForAnalysis state{};
        for (unsigned i = 0; i != 26; ++i) state.nativeTailWords3A8[i] = 0xA5A50000u + i;
        {
            Fixture f(state); f.node.SetPositionForAnalysis({Float(0x80000000), Float(1), Float(0xFFC12345)});
            f.action->SetDurationMillisecondsForAnalysis(17); f.action->SetClearableWordForAnalysis(0xDEADBEEF);
            const auto nodeBefore = f.node.GetPositionForAnalysis();
            f.action->vfunc_30(); f.action->vfunc_30();
            auto expected = state; expected.nativeTailWords3A8[0] = 0x80000000; expected.nativeTailWords3A8[1] = 1;
            expected.nativeTailWords3A8[2] = 0xFFC12345; expected.nativeTailWords3A8[4] = expected.nativeTailWords3A8[5] = 0;
            Check(Hex(f.action->CapturedStateForAnalysis()) == Hex(expected) && f.host.reads == 2 && f.host.clearsBeforeRead, "Exact signedzero/subnormal/rawZ and repeats preserve opaque tail");
            Check(f.action->GetDurationMillisecondsForAnalysis() == 17 && f.action->GetClearableWordForAnalysis() == 0xDEADBEEF, "Parent fields are unchanged");
            const auto nodeAfter = f.node.GetPositionForAnalysis(); Check(std::memcmp(nodeBefore.data(), nodeAfter.data(), 12) == 0, "Borrowed Node position unchanged");
        }
        {
            Fixture f(state); f.host.admitted = false;
            Check(f.action->EnterForAnalysis() == wxWanderResultForAnalysis::UnqualifiedProfile && Hex(f.action->CapturedStateForAnalysis()) == Hex(state) && !f.host.reads, "Default profile refuses before effects");
            f.host.admitted = true; f.action->SetOwnerForAnalysis(nullptr);
            Check(f.action->EnterForAnalysis() == wxWanderResultForAnalysis::MissingOwner && !f.host.reads, "Missing owner is analytical refusal after clears");
            const auto& tail = f.action->CapturedStateForAnalysis().nativeTailWords3A8; Check(!tail[4] && !tail[5] && tail[0] == state.nativeTailWords3A8[0], "Missing owner retains native preceding clears");
        }
        {
            Fixture f(state); f.owner.entity = nullptr;
            Check(f.action->EnterForAnalysis() == wxWanderResultForAnalysis::MissingEntity, "Missing entity wrapper refusal");
            f.owner.entity = &f.entity; f.entity.node = nullptr;
            Check(f.action->EnterForAnalysis() == wxWanderResultForAnalysis::MissingNode, "Null Node link refusal");
            struct Derived final : spNode {} derived; f.entity.node = &derived;
            Check(f.action->EnterForAnalysis() == wxWanderResultForAnalysis::UnqualifiedNodeType, "Derived Node remains unqualified");
        }
        for (auto word : {0x7F800000u, 0xFF800000u, 0x7FC12345u, 0x7F812345u})
        {
            Fixture f(state); f.node.SetPositionForAnalysis({Float(word), 0, 0});
            Check(f.action->EnterForAnalysis() == wxWanderResultForAnalysis::UnqualifiedPosition && f.action->CapturedStateForAnalysis().nativeTailWords3A8[0] == state.nativeTailWords3A8[0], "Nonfinite x87 input refuses without position writes");
        }
        {
            Fixture f(state); wxAIAction other; f.action->SetCurrentActionForAnalysis(&other);
            Check(f.action->EnterForAnalysis() == wxWanderResultForAnalysis::UnqualifiedProfile && Hex(f.action->CapturedStateForAnalysis()) == Hex(state), "Current-action graph refuses");
            f.action->SetCurrentActionForAnalysis(nullptr); f.action->AddOwnedActionForAnalysis(std::make_unique<spBaseObject>());
            Check(f.action->EnterForAnalysis() == wxWanderResultForAnalysis::UnqualifiedProfile && Hex(f.action->CapturedStateForAnalysis()) == Hex(state), "Owned-action graph refuses");
        }
        {
            Fixture f(state); f.action->vfunc_0C(nullptr); Check(f.action->LastResultForAnalysis() == wxWanderResultForAnalysis::UnqualifiedNotify, "Notify refusal");
            Check(!f.action->vfunc_24() && !f.action->vfunc_28() && !f.action->vfunc_2C() && !f.action->vfunc_3C(nullptr) && !f.action->vfunc_40(nullptr), "Unknown bool hooks refuse");
            f.action->vfunc_34_ClearForAnalysis(); f.action->vfunc_38(); Check(f.action->LastResultForAnalysis() == wxWanderResultForAnalysis::UnqualifiedHook, "Unknown void hooks report refusal");
            spCloneManager clones; Check(!f.action->vfunc_14(*f.action, clones), "Copy refusal"); bool refused = false;
            try { (void)f.action->vfunc_10(clones); } catch (const std::logic_error&) { refused = true; } Check(refused, "Clone refusal");
            refused = false; try { (void)spRTTIManager::Instance().Create(wxWanderAIAction::ClassID); } catch (const std::logic_error&) { refused = true; } Check(refused, "Generic factory requires capture");
            Check(f.action->IsExactly(wxWanderAIAction::ClassID) && f.action->IsKindOf(wxAIAction::ClassID) && Hex(f.action->CapturedStateForAnalysis()) == Hex(state) && !f.host.reads, "Identity and refused hooks preserve state");
            f.action.reset(); Check(f.node.IsExactly(spNode::ClassID) && f.entity.node == &f.node && f.owner.entity == &f.entity, "Borrowed controls survive portable teardown");
        }
    }
}
int main(int argc, char** argv)
{
    try { if (argc == 2 && std::string(argv[1]) == "--replay") Replay(); else { Contracts(); std::cout << "Wander bounded Entry contracts passed\n"; } return 0; }
    catch (const std::exception& error) { std::cerr << "Wander " << error.what() << '\n'; return 1; }
}
