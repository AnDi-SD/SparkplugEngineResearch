#include "Code/wxMikaelWandringAIAction.h"
#include "Code/wxAICommandRecordForAnalysis.h"
#include "Code/Sparkplug/spNode.h"
#include "Analysis/PC/wxMikaelWandringAIActionAbi.h"
#include "Analysis/PC/wxPlanarAngleMath.h"
#include <cstring>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    using Result = wxMikaelWandringAIActionResultForAnalysis;
    using Profile = wxMikaelWandringAIAction::ProfileForAnalysis;
    using Own4CProfile = wxMikaelWandringAIAction::Own4CProfileForAnalysis;
    using Command = wxAICommandRecordForAnalysis;
    void Check(bool value, const char* text) { if (!value) throw std::runtime_error(text); }
    template<class Bytes> std::string Hex(const Bytes& bytes)
    { constexpr char digits[] = "0123456789abcdef"; std::string out; for (auto byte : bytes) { out += digits[byte >> 4]; out += digits[byte & 15]; } return out; }
    template<std::size_t N> std::array<std::uint8_t, N> Decode(const std::string& text)
    {
        Check(text.size() == 2 * N, "Complete captured raw input"); std::array<std::uint8_t, N> out{};
        const auto nibble = [](char c) { Check((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'), "Lowercase raw hex input"); return c <= '9' ? c - '0' : c - 'a' + 10; };
        for (std::size_t i = 0; i != N; ++i) out[i] = static_cast<std::uint8_t>((nibble(text[2*i]) << 4) | nibble(text[2*i+1])); return out;
    }
    template<class Bytes> std::uint32_t Word(const Bytes& raw, std::size_t offset)
    { std::uint32_t result = 0; for (unsigned i = 0; i != 4; ++i) result |= std::uint32_t{raw[offset+i]} << (i*8); return result; }
    template<class Bytes> void Put(Bytes& raw, std::size_t offset, std::uint32_t value)
    { for (unsigned i = 0; i != 4; ++i) raw[offset+i] = static_cast<std::uint8_t>(value >> (i*8)); }
    float Float(std::uint32_t bits) { float value; std::memcpy(&value, &bits, 4); return value; }
    std::uint32_t Bits(float value) { std::uint32_t bits; std::memcpy(&bits, &value, 4); return bits; }
    template<std::size_t N> std::array<float, N> Floats(const std::array<std::uint8_t, 180>& raw, std::size_t offset)
    { std::array<float, N> values{}; for (unsigned i = 0; i != N; ++i) values[i] = Float(Word(raw, offset + 4*i)); return values; }
    void SeedNode(spNode& node, const std::string& input)
    {
        const auto raw = Decode<180>(input);
        node.SetPositionForAnalysis(Floats<3>(raw, 0x74)); node.SetScaleForAnalysis(Floats<3>(raw, 0x80));
        node.SetOrientationForAnalysis(Floats<9>(raw, 0x8C)); node.MarkLocalTransformDirtyForAnalysis();
        Check(node.UpdateWorldForAnalysis(), "Prepare real Node WORLD caches");
        // Raw local setters do not invalidate caches. These independent input
        // decoys expose wrong LOCAL or scaled WORLD-matrix reads.
        node.SetPositionForAnalysis(Floats<3>(raw, 0x20)); node.SetScaleForAnalysis(Floats<3>(raw, 0x30));
        node.SetOrientationForAnalysis(Floats<9>(raw, 0x40));
    }
    std::string NodeProjection(const spNode& node)
    {
        std::array<std::uint8_t, 120> bytes{}; std::size_t offset = 0;
        const auto append = [&](const auto& values) { for (float value : values) { Put(bytes, offset, Bits(value)); offset += 4; } };
        append(node.GetPositionForAnalysis()); append(node.GetScaleForAnalysis()); append(node.GetOrientationForAnalysis());
        append(node.GetWorldPositionForAnalysis()); append(node.GetWorldScaleForAnalysis()); append(node.GetWorldOrientationForAnalysis());
        Check(offset == bytes.size(), "Complete represented Node transform projection"); return Hex(bytes);
    }
    struct Config
    {
        unsigned index = 0, calls = 1, targetID = 2;
        std::uint32_t duration = 2000, clearable = 0, previous = 0xA5A5A5A5, current = 3;
        std::string own3A8, own3B8, command, sourceNode, targetNode;
    };
    struct World final : wxMikaelWandringAIActionHost
    {
        spNode source, target;
        Command command;
        struct Owner { std::uint32_t previous, current; } owner;
        std::unique_ptr<wxMikaelWandringAIAction> action;
        std::vector<unsigned> observations;
        unsigned missing = 0;
        explicit World(const Config& c, Profile profile = Profile::PcCapturedFiniteEntryAndOwn48,
            Command::ProfileForAnalysis commandProfile = Command::ProfileForAnalysis::PcCaptured703090)
            : command(Decode<116>(c.command), commandProfile), owner{c.previous, c.current}
        {
            SeedNode(source, c.sourceNode); SeedNode(target, c.targetNode); observations.reserve(8);
            action = std::make_unique<wxMikaelWandringAIAction>(*this,
                wxMikaelWandringAIActionCapturedStateForAnalysis{&command, Decode<8>(c.own3A8), &source, c.targetID == 2 ? &target : nullptr, Decode<48>(c.own3B8)}, profile);
            action->SetOwnerForAnalysis(&owner); action->SetDurationMillisecondsForAnalysis(c.duration); action->SetClearableWordForAnalysis(c.clearable);
        }
        std::uint32_t* BorrowOwnerWord1C0ForAnalysis(void* input) noexcept override
        {
            if (input != &owner || command.GetBytesForAnalysis()[0x1B] != 1 ||
                (Word(command.GetBytesForAnalysis(), 4) != 0x3F800000 && Word(command.GetBytesForAnalysis(), 4) != 0x3D4CCCCD)) std::terminate();
            observations.push_back(1); return missing == 1 ? nullptr : &owner.current;
        }
        std::uint32_t* BorrowOwnerWord1BCForAnalysis(void* input) noexcept override
        { if (input != &owner) std::terminate(); observations.push_back(2); return missing == 2 ? nullptr : &owner.previous; }
        std::string Snapshot() const
        {
            const auto& state = action->CapturedStateForAnalysis(); std::ostringstream out;
            out << "{\"ownerID\":" << (action->GetOwnerForAnalysis() == &owner ? 1 : 0)
                << ",\"commandID\":" << (state.borrowedBaseCommand3A4 == &command ? 1 : 0)
                << ",\"sourceNodeID\":" << (state.borrowedSourceNode3B0 == &source ? 1 : 0)
                << ",\"targetNodeID\":" << (state.borrowedTargetNode3B4 == &target ? 2 : 0)
                << ",\"opaque3A8\":\"" << Hex(state.ownBytes3A8) << "\",\"opaque3B8\":\"" << Hex(state.ownBytes3B8)
                << "\",\"duration370\":" << action->GetDurationMillisecondsForAnalysis() << ",\"clearable374\":" << action->GetClearableWordForAnalysis()
                << ",\"commandBytes\":\"" << Hex(command.GetBytesForAnalysis()) << "\",\"ownerPrevious1BC\":" << owner.previous
                << ",\"ownerCurrent1C0\":" << owner.current << ",\"sourceNodeTransform\":\"" << NodeProjection(source)
                << "\",\"targetNodeTransform\":\"" << NodeProjection(target) << "\"}"; return out.str();
        }
    };
    void One(const Config& c, bool emit)
    {
        Check(c.index < 24 && c.calls >= 1 && c.calls <= 2, "Bounded Entry replay input"); World world(c);
        const auto source = NodeProjection(world.source), target = NodeProjection(world.target); std::ostringstream steps; steps << '[';
        for (unsigned ordinal = 0; ordinal != c.calls; ++ordinal)
        {
            const auto before = world.Snapshot(); const auto first = world.observations.size(); const auto current = world.owner.current;
            world.action->vfunc_30(); Check(world.action->LastResultForAnalysis() == Result::Completed, "Full captured Entry and actual own48 complete");
            Check(world.owner.previous == current && world.owner.current == 1 && NodeProjection(world.source) == source && NodeProjection(world.target) == target,
                "Captured old current becomes previous; both borrowed Node transforms preserved");
            Check(std::vector<unsigned>(world.observations.begin()+first, world.observations.end()) == std::vector<unsigned>{1,2}, "Command writes precede owner1C0 read and owner1BC borrow");
            if (ordinal) steps << ','; steps << "{\"ordinal\":" << ordinal << ",\"before\":" << before << ",\"after\":" << world.Snapshot() << '}';
        }
        steps << ']'; const auto command = world.command.GetBytesForAnalysis(); const auto previous = world.owner.previous, current = world.owner.current;
        world.action.reset(); Check(world.command.GetBytesForAnalysis() == command && world.owner.previous == previous && world.owner.current == current
            && NodeProjection(world.source) == source && NodeProjection(world.target) == target, "Portable teardown preserves borrowed Nodes, command and owner");
        if (emit) std::cout << "{\"steps\":" << steps.str() << "}\n";
    }
    Config Base()
    {
        Config c; std::array<std::uint8_t, 8> own8{}; own8.fill(0xA5); c.own3A8 = Hex(own8);
        std::array<std::uint8_t, 48> own48{}; own48.fill(0x5A); c.own3B8 = Hex(own48);
        std::array<std::uint8_t, 116> command{}; command.fill(0xCC); Put(command, 0, 0x703090); c.command = Hex(command);
        std::array<std::uint8_t, 180> node{}; Put(node, 0, 0x6DC4F4);
        for (auto offset : {0x30u,0x34u,0x38u,0x40u,0x50u,0x60u,0x80u,0x84u,0x88u,0x8Cu,0x9Cu,0xACu}) Put(node, offset, Bits(1.0f));
        Put(node, 0x20, Bits(901)); Put(node, 0x58, Bits(-7)); c.sourceNode = Hex(node);
        Put(node, 0x7C, Bits(100)); c.targetNode = Hex(node); return c;
    }
    void Contracts()
    {
        static_assert(sizeof(winx::evidence::pc::wxMikaelWandringAIActionLayout) == 1000);
        static_assert(!std::is_copy_constructible_v<wxMikaelWandringAIAction> && !std::is_move_constructible_v<wxMikaelWandringAIAction>);
        auto c = Base(); c.calls = 2; One(c, false); c.calls = 1;
        {
            World world(c, Profile::Unqualified); const auto before = world.Snapshot();
            Check(world.action->EnterForAnalysis() == Result::UnqualifiedProfile && world.Snapshot() == before && world.observations.empty(), "Default refusal before all effects");
        }
        for (auto profile : {Command::ProfileForAnalysis::Unqualified, Command::ProfileForAnalysis::PcCaptured703090})
        {
            auto input = c; if (profile == Command::ProfileForAnalysis::PcCaptured703090) { auto bytes = Decode<116>(input.command); Put(bytes, 0, 0); input.command = Hex(bytes); }
            World world(input, Profile::PcCapturedFiniteEntryAndOwn48, profile); const auto before = world.Snapshot();
            Check(world.action->EnterForAnalysis() == Result::CommandNotAdmitted && world.Snapshot() == before && world.observations.empty(), "Shared command profile/tag refusal before word08");
        }
        for (unsigned missing = 1; missing <= 2; ++missing)
        {
            World world(c); world.missing = missing;
            Check(world.action->EnterForAnalysis() == Result::MissingOwnerStorage && world.command.GetBytesForAnalysis()[0x1B] == 1
                && world.owner.previous == c.previous && world.owner.current == c.current, "Late owner-storage refusal preserves completed command prefix");
        }
        {
            World world(c); world.action->SetOwnerForAnalysis(nullptr);
            Check(world.action->EnterForAnalysis() == Result::MissingOwner && world.command.GetBytesForAnalysis()[0x1B] == 1 && world.observations.empty(), "Missing owner after actual command prefix");
        }
        {
            World world(c); auto state = world.action->CapturedStateForAnalysis(); state.borrowedTargetNode3B4 = nullptr;
            wxMikaelWandringAIAction action(world, state, Profile::PcCapturedFiniteEntryAndOwn48);
            const auto before = world.command.GetBytesForAnalysis(); Check(action.EnterForAnalysis() == Result::MissingTargetNode && world.command.GetBytesForAnalysis() == before, "Missing target refuses before geometry writes");
            state = world.action->CapturedStateForAnalysis(); state.borrowedSourceNode3B0 = nullptr;
            wxMikaelWandringAIAction missingSource(world, state, Profile::PcCapturedFiniteEntryAndOwn48);
            Check(missingSource.EnterForAnalysis() == Result::MissingSourceNode && world.command.GetBytesForAnalysis() == before, "Missing source refuses before writes");
            state = world.action->CapturedStateForAnalysis(); state.borrowedBaseCommand3A4 = nullptr;
            wxMikaelWandringAIAction missingCommand(world, state, Profile::PcCapturedFiniteEntryAndOwn48);
            Check(missingCommand.EnterForAnalysis() == Result::MissingCommand && world.command.GetBytesForAnalysis() == before, "Missing command refuses before publication");
        }
        for (float invalid : {1025.0f, std::numeric_limits<float>::quiet_NaN()})
        {
            World world(c); auto position = world.target.GetWorldPositionForAnalysis(); position[0] = invalid;
            world.target.SetPositionForAnalysis(position); world.target.MarkLocalTransformDirtyForAnalysis(); Check(world.target.UpdateWorldForAnalysis(), "Prepare refused geometry input");
            const auto command = world.command.GetBytesForAnalysis(); Check(world.action->EnterForAnalysis() == Result::UnqualifiedGeometry && world.command.GetBytesForAnalysis() == command && world.observations.empty(), "Finite/bounded domain refusal before command stores");
        }
        {
            World world(c); const auto before = world.Snapshot(); wxAIAction child;
            world.action->SetCurrentActionForAnalysis(&child); Check(world.action->EnterForAnalysis() == Result::UnqualifiedProfile && world.Snapshot() == before, "Nonempty current graph refuses");
            world.action->SetCurrentActionForAnalysis(nullptr); world.action->AddOwnedActionForAnalysis(std::make_unique<spBaseObject>());
            Check(world.action->EnterForAnalysis() == Result::UnqualifiedProfile && world.Snapshot() == before && world.observations.empty(), "Owned inherited graph refuses");
        }
        {
            World world(c); const auto before = world.Snapshot(); world.action->vfunc_0C(nullptr); Check(world.action->LastResultForAnalysis() == Result::UnqualifiedNotify, "Notify refuses");
            Check(!world.action->vfunc_24() && !world.action->vfunc_28() && !world.action->vfunc_2C() && !world.action->vfunc_3C(nullptr) && !world.action->vfunc_40(nullptr), "Unknown Boolean hooks refuse");
            world.action->vfunc_34_ClearForAnalysis(); world.action->vfunc_38(); spCloneManager clones; Check(!world.action->vfunc_14(*world.action, clones), "Copy refuses");
            bool refused = false; try { (void)world.action->vfunc_10(clones); } catch (const std::logic_error&) { refused = true; } Check(refused, "Clone refuses");
            refused = false; try { (void)spRTTIManager::Instance().Create(wxMikaelWandringAIAction::ClassID); } catch (const std::logic_error&) { refused = true; } Check(refused, "Generic factory requires explicit captured bindings");
            Check(world.Snapshot() == before && world.observations.empty() && world.action->IsKindOf(wxAIAction::ClassID), "Refusals preserve records and logical RTTI");
        }
        const double equal = winx::analysis::pc::PcPlanarAngleDistanceForAnalysis(0.0f, 0.08f);
        Check(equal == double(0.08f), "Shared metric preserves exact float tolerance for inclusive caller");
        bool facing = true; Check(winx::analysis::pc::PcPlanarFacingForAnalysis({0,0,1}, 0.1f, 0.1f, facing) && !facing, "Existing normalized Facing remains strict at equality");
    }
    void Replay()
    {
        Config c; while (std::cin >> c.index)
        {
            Check(bool(std::cin >> c.calls >> c.duration >> c.clearable >> c.previous >> c.current >> c.own3A8 >> c.own3B8 >> c.command >> c.sourceNode >> c.targetNode), "Complete captured Entry replay row"); One(c, true);
        }
        Check(std::cin.eof(), "Complete replay boundary");
    }
    void Own4COne(const Config& c, bool emit)
    {
        Check(c.index < 20 && c.calls >= 1 && c.calls <= 2 && (c.targetID == 0 || c.targetID == 2), "Bounded stored-point4C replay input");
        // Own4C admission is independent of the default-refused Entry profile.
        World world(c, Profile::Unqualified); const auto source = NodeProjection(world.source), target = NodeProjection(world.target);
        const auto own8 = world.action->CapturedStateForAnalysis().ownBytes3A8;
        const auto own48 = world.action->CapturedStateForAnalysis().ownBytes3B8;
        const auto flag = world.command.GetBytesForAnalysis()[0x1B]; std::ostringstream steps; steps << '[';
        for (unsigned ordinal = 0; ordinal != c.calls; ++ordinal)
        {
            const auto before = world.Snapshot(); const auto previous = world.owner.previous, current = world.owner.current;
            Check(world.action->OwnSlot4CForAnalysis(Own4CProfile::PcCapturedFiniteStoredPoint) == Result::Completed, "Actual stored-point4C source completes");
            Check(world.observations.empty() && world.owner.previous == previous && world.owner.current == current && world.command.GetBytesForAnalysis()[0x1B] == flag,
                "Stored-point4C never requests owner cells or changes owner/command flag");
            Check(world.action->CapturedStateForAnalysis().ownBytes3A8 == own8 && world.action->CapturedStateForAnalysis().ownBytes3B8 == own48
                && NodeProjection(world.source) == source && NodeProjection(world.target) == target, "Stored point/raw action payload and both borrowed Node transforms preserved");
            if (ordinal) steps << ','; steps << "{\"ordinal\":" << ordinal << ",\"before\":" << before << ",\"after\":" << world.Snapshot() << '}';
        }
        steps << ']'; const auto command = world.command.GetBytesForAnalysis(); const auto previous = world.owner.previous, current = world.owner.current;
        Check(world.action->EnterForAnalysis() == Result::UnqualifiedProfile, "Admitted4C does not admit Entry/self48");
        world.action.reset(); Check(world.command.GetBytesForAnalysis() == command && world.owner.previous == previous && world.owner.current == current
            && NodeProjection(world.source) == source && NodeProjection(world.target) == target, "Stored-point action teardown preserves borrowed data");
        if (emit) std::cout << "{\"steps\":" << steps.str() << "}\n";
    }
    void Own4CContracts()
    {
        auto c = Base(); auto own = Decode<48>(c.own3B8); Put(own, 0, Bits(0)); Put(own, 4, Bits(17)); Put(own, 8, Bits(100)); c.own3B8 = Hex(own);
        c.targetID = 0; c.calls = 2; Own4COne(c, false); c.targetID = 2; c.calls = 1; Own4COne(c, false);
        {
            World world(c); const auto before = world.Snapshot();
            Check(world.action->OwnSlot4CForAnalysis() == Result::UnqualifiedProfile && world.Snapshot() == before && world.observations.empty(), "Entry profile does not admit default4C");
        }
        for (auto profile : {Command::ProfileForAnalysis::Unqualified, Command::ProfileForAnalysis::PcCaptured703090})
        {
            auto input = c; if (profile == Command::ProfileForAnalysis::PcCaptured703090) { auto bytes = Decode<116>(input.command); Put(bytes, 0, 0); input.command = Hex(bytes); }
            World world(input, Profile::Unqualified, profile); const auto before = world.Snapshot();
            Check(world.action->OwnSlot4CForAnalysis(Own4CProfile::PcCapturedFiniteStoredPoint) == Result::CommandNotAdmitted
                && world.Snapshot() == before && world.observations.empty(), "Stored-point shared record profile/tag refusal before word08");
        }
        for (float invalid : {1025.0f, std::numeric_limits<float>::quiet_NaN()})
        {
            auto input = c; auto bytes = Decode<48>(input.own3B8); Put(bytes, 0, Bits(invalid)); input.own3B8 = Hex(bytes);
            World world(input, Profile::Unqualified); const auto before = world.Snapshot();
            Check(world.action->OwnSlot4CForAnalysis(Own4CProfile::PcCapturedFiniteStoredPoint) == Result::UnqualifiedGeometry
                && world.Snapshot() == before && world.observations.empty(), "Stored-point finite X/Z bounds refuse before writes");
        }
        {
            World world(c, Profile::Unqualified); auto state = world.action->CapturedStateForAnalysis(); state.borrowedSourceNode3B0 = nullptr;
            wxMikaelWandringAIAction action(world, state); const auto before = world.command.GetBytesForAnalysis();
            Check(action.OwnSlot4CForAnalysis(Own4CProfile::PcCapturedFiniteStoredPoint) == Result::MissingSourceNode && world.command.GetBytesForAnalysis() == before, "Stored-point missing source refuses");
            state = world.action->CapturedStateForAnalysis(); state.borrowedBaseCommand3A4 = nullptr;
            wxMikaelWandringAIAction missingCommand(world, state);
            Check(missingCommand.OwnSlot4CForAnalysis(Own4CProfile::PcCapturedFiniteStoredPoint) == Result::MissingCommand && world.command.GetBytesForAnalysis() == before, "Stored-point missing command refuses");
        }
        {
            World world(c, Profile::Unqualified); const auto before = world.Snapshot(); wxAIAction child; world.action->SetCurrentActionForAnalysis(&child);
            Check(world.action->OwnSlot4CForAnalysis(Own4CProfile::PcCapturedFiniteStoredPoint) == Result::UnqualifiedProfile && world.Snapshot() == before, "Stored-point inherited current graph refuses");
            world.action->SetCurrentActionForAnalysis(nullptr); world.action->AddOwnedActionForAnalysis(std::make_unique<spBaseObject>());
            Check(world.action->OwnSlot4CForAnalysis(Own4CProfile::PcCapturedFiniteStoredPoint) == Result::UnqualifiedProfile && world.Snapshot() == before && world.observations.empty(), "Stored-point inherited owned graph refuses");
        }
    }
    void Own4CReplay()
    {
        Config c; while (std::cin >> c.index)
        {
            Check(bool(std::cin >> c.calls >> c.targetID >> c.duration >> c.clearable >> c.previous >> c.current >> c.own3A8 >> c.own3B8 >> c.command >> c.sourceNode >> c.targetNode), "Complete stored-point replay row"); Own4COne(c, true);
        }
        Check(std::cin.eof(), "Complete stored-point replay boundary");
    }
}
int main(int argc, char** argv)
{
    try { if (argc == 2 && std::string(argv[1]) == "--replay") Replay(); else if (argc == 2 && std::string(argv[1]) == "--slot4c-replay") Own4CReplay(); else { Contracts(); Own4CContracts(); std::cout << "Mikael captured Entry/actual own48 finite conditional contracts passed\n"; } return 0; }
    catch (const std::exception& error) { std::cerr << "Mikael " << error.what() << '\n'; return 1; }
}
