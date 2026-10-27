#include "Code/wxPerception.h"
#include "Code/wxGameTimer.h"
#include "Analysis/PC/wxPerceptionAbi.h"
#include "Analysis/PS2/wxPerceptionAbi.h"
#include "Analysis/PC/wxPerceptionNumeric.h"

#include <bit>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;

namespace
{
    void Check(bool condition, const char* text)
    {
        if (!condition) throw std::runtime_error(text);
    }
    template<class Exception, class Function> void Throws(Function function, const char* text)
    {
        try { function(); } catch (const Exception&) { return; }
        throw std::runtime_error(text);
    }
    struct Host final : wxPerceptionHost
    {
        std::map<const void*, std::array<float, 3>> positions;
        mutable std::vector<const void*> reads;
        std::vector<void*> detached;
        bool detachedAfterClear = true;
        std::array<float, 3> ReadNodePositionForAnalysis(const void* object) const override
        {
            reads.push_back(object);
            return positions.at(object);
        }
        void DetachOwnerForAnalysis(wxPerception& perception, void* owner) noexcept override
        {
            detachedAfterClear &= perception.TargetsForAnalysis().empty();
            detached.push_back(owner);
        }
    };
    std::array<std::uint32_t, 3> Words(std::array<float, 3> value)
    {
        return {std::bit_cast<std::uint32_t>(value[0]),
            std::bit_cast<std::uint32_t>(value[1]), std::bit_cast<std::uint32_t>(value[2])};
    }
    std::array<std::uint32_t, 15> CopiedValues(const wxPerceptionObservedStateForAnalysis& state)
    {
        const auto scratch = state.scratchWords30.value();
        return {state.words24[0], state.words24[1], state.words24[2],
            scratch[0], scratch[1], scratch[2], state.word3C, state.word44,
            state.word4C, state.word50, state.word54, state.word74,
            state.byte40, state.byte48, state.byte70};
    }
    void Seed(wxPerceptionObservedStateForAnalysis& state, unsigned seed)
    {
        std::array<std::uint32_t, 12> words{};
        for (unsigned i = 0; i != words.size(); ++i)
            for (unsigned byte = 0; byte != 4; ++byte)
                words[i] |= ((seed + i * 29 + byte * 13) & 255u) << (byte * 8);
        state.words24 = {words[0], words[1], words[2]};
        state.scratchWords30 = std::array<std::uint32_t, 3>{words[3], words[4], words[5]};
        state.word3C = words[6]; state.word44 = words[7]; state.word4C = words[8];
        state.word50 = words[9]; state.word54 = words[10]; state.word74 = words[11];
        state.byte40 = static_cast<std::uint8_t>(seed + 12 * 29);
        state.byte48 = static_cast<std::uint8_t>(seed + 13 * 29);
        state.byte70 = static_cast<std::uint8_t>(seed + 14 * 29);
    }

    void Replay(std::istream& input)
    {
        std::string operation;
        while (input >> operation)
        {
            if (operation == "q" || operation == "p")
            {
                unsigned hasOwner = 0, count = 0;
                std::array<float, 3> origin{};
                std::array<std::uint32_t, 3> scratch{};
                input >> hasOwner >> count;
                if (operation == "p")
                {
                    std::array<std::uint32_t, 3> raw{};
                    input >> raw[0] >> raw[1] >> raw[2];
                    for (std::size_t axis = 0; axis != 3; ++axis)
                        origin[axis] = std::bit_cast<float>(raw[axis]);
                }
                else input >> origin[0] >> origin[1] >> origin[2];
                input >> scratch[0] >> scratch[1] >> scratch[2];
                Check(count <= 32, "replay target bound");
                Host host;
                int owner = 0;
                std::vector<int> objects(count);
                std::vector<void*> targets;
                host.positions[&owner] = origin;
                for (unsigned i = 0; i != count; ++i)
                {
                    std::array<float, 3> position{};
                    if (operation == "p")
                    {
                        std::array<std::uint32_t, 3> raw{};
                        input >> raw[0] >> raw[1] >> raw[2];
                        for (std::size_t axis = 0; axis != 3; ++axis)
                            position[axis] = std::bit_cast<float>(raw[axis]);
                    }
                    else input >> position[0] >> position[1] >> position[2];
                    targets.push_back(&objects[i]); host.positions[&objects[i]] = position;
                }
                Check(bool(input), "complete replay query");
                wxPerception perception;
                perception.StateForAnalysis().scratchWords30 = scratch;
                perception.BindForAnalysis(host, hasOwner ? &owner : nullptr, targets);
                const auto* selected = perception.FindNearestForAnalysis(operation == "p" ?
                    wxPerceptionNumericProfileForAnalysis::PcFiniteNearest64 :
                    wxPerceptionNumericProfileForAnalysis::ExactSmallInteger);
                int index = -1;
                for (unsigned i = 0; i != count; ++i)
                    if (selected == &objects[i]) index = static_cast<int>(i);
                const auto result = perception.StateForAnalysis().scratchWords30.value();
                std::cout << index << ' ' << result[0] << ' ' << result[1] << ' ' << result[2] << '\n';
            }
            else if (operation == "c")
            {
                unsigned seed = 0; input >> seed;
                wxPerception source, destination;
                Seed(source.StateForAnalysis(), seed);
                destination.StateForAnalysis().word58 = 0x11223344;
                destination.StateForAnalysis().byte78 = 0xA5;
                spCloneManager manager;
                Check(source.vfunc_14(destination, manager), "replay copy accepted");
                Check(destination.StateForAnalysis().word58 == 0x11223344 &&
                    destination.StateForAnalysis().byte78 == 0xA5, "copy preserves omitted fields");
                bool first = true;
                for (const auto value : CopiedValues(destination.StateForAnalysis()))
                {
                    if (!first) std::cout << ' ';
                    first = false; std::cout << value;
                }
                std::cout << '\n';
            }
            else if (operation == "r")
            {
                unsigned paused, hasOwner, opaque, expiry, now, dirty;
                input >> paused >> hasOwner >> opaque >> expiry >> now >> dirty;
                Host host;int owner = 0;wxPerception perception;
                perception.BindForAnalysis(host, hasOwner ? &owner : nullptr, {});
                auto& state = perception.StateForAnalysis();state.opaque68=opaque;state.opaque6C=expiry;state.byte40=std::uint8_t(dirty);
                spTimer clock;wxGameTimer timer(clock);timer.SetPausedByteForAnalysis(std::uint8_t(paused));
                auto time=timer.GetStateForAnalysis();time.currentMilliseconds=now;timer.SetStateForAnalysis(time);
                const auto result=perception.RefreshBoundedPcForAnalysis(timer);
                std::cout << unsigned(result) << ' ' << state.opaque68 << ' ' << state.opaque6C << ' ' << unsigned(state.byte40) << '\n';
                perception.ResetForAnalysis();
            }
            else throw std::invalid_argument("unknown replay operation");
        }
    }

    void Queries()
    {
        Host host;
        int owner = 0, first = 1, second = 2, third = 3;
        host.positions = {{&owner, {10, -20, 30}}, {&first, {11, -20, 30}},
            {&second, {9, -20, 30}}, {&third, {20, -20, 30}}};
        wxPerception perception;
        const std::array<std::uint32_t, 3> sentinel{0x12345678, 0x7F812345, 0x80000000};
        perception.StateForAnalysis().scratchWords30 = sentinel;
        perception.BindForAnalysis(host, nullptr, {&first});
        Check(perception.FindNearestForAnalysis() == nullptr && host.reads.empty() &&
            perception.StateForAnalysis().scratchWords30 == sentinel, "null owner skips traversal and scratch");
        perception.ResetForAnalysis();
        perception.BindForAnalysis(host, &owner, {});
        Check(perception.FindNearestForAnalysis() == nullptr && host.reads.size() == 1 &&
            perception.StateForAnalysis().scratchWords30 == sentinel, "empty list reads owner but preserves scratch");
        perception.ResetForAnalysis();
        host.reads.clear();
        perception.BindForAnalysis(host, &owner, {&first, &second, &third});
        Check(perception.FindNearestForAnalysis() == &second, "equal distance chooses later target");
        Check(perception.StateForAnalysis().scratchWords30 == Words({20, -20, 30}),
            "scratch is last visited position rather than selected position");
        Check(host.reads == std::vector<const void*>{&owner, &first, &second, &third}, "native read order");
        perception.ResetForAnalysis();
        Check(host.detached.size() == 2 && host.detachedAfterClear, "list clear before owner detach");
        host.positions[&owner] = {-1024, -1024, -1024};
        host.positions[&first] = {1024, 1024, 1024};
        perception.BindForAnalysis(host, &owner, {&first});
        Check(perception.FindNearestForAnalysis() == &first, "exact integer domain maximum delta");
        host.positions[&first] = {0.5f, 0, 0};
        Throws<std::domain_error>([&] { (void)perception.FindNearestForAnalysis(); }, "fractional arithmetic fails closed");
        Check(perception.StateForAnalysis().scratchWords30 == Words({0.5f, 0, 0}), "guard does not erase already written scratch");
        host.positions[&first] = {-0.0f, 0, 0};
        Throws<std::domain_error>([&] { (void)perception.FindNearestForAnalysis(); }, "negative zero outside EE profile");
        host.positions[&first] = {std::numeric_limits<float>::infinity(), 0, 0};
        Throws<std::domain_error>([&] { (void)perception.FindNearestForAnalysis(); }, "nonfinite arithmetic fails closed");
    }

    void CopiesAndLifetime()
    {
        wxPerception source, destination;
        Check(!source.StateForAnalysis().scratchWords30 && source.StateForAnalysis().word50 == 0x3F3504F3 &&
            source.StateForAnalysis().word74 == 0x7F7FFFFF, "PC constructor scalar defaults and unwritten scratch");
        Seed(source.StateForAnalysis(), 255);
        destination.StateForAnalysis().word58 = 0x12345678;
        destination.StateForAnalysis().word5C = 0x87654321;
        destination.StateForAnalysis().word60 = 0xABCDEF01;
        destination.StateForAnalysis().byte64 = 17;
        destination.StateForAnalysis().byte65 = 29;
        destination.StateForAnalysis().byte78 = 43;
        spCloneManager manager;
        Check(source.vfunc_14(destination, manager) &&
            CopiedValues(source.StateForAnalysis()) == CopiedValues(destination.StateForAnalysis()), "all observed copy fields");
        Check(destination.StateForAnalysis().word58 == 0x12345678 &&
            destination.StateForAnalysis().word5C == 0x87654321 && destination.StateForAnalysis().word60 == 0xABCDEF01 &&
            destination.StateForAnalysis().byte64 == 17 && destination.StateForAnalysis().byte65 == 29 &&
            destination.StateForAnalysis().byte78 == 43, "omitted copy fields preserve destination state");
        auto clone = manager.Clone(source);
        auto* typed = dynamic_cast<wxPerception*>(clone.get());
        Check(typed != nullptr && CopiedValues(typed->StateForAnalysis()) == CopiedValues(source.StateForAnalysis()) &&
            typed->StateForAnalysis().word58 == 0, "clone uses native constructor defaults for omitted fields");
        spBaseObject wrong;
        Check(!source.vfunc_14(wrong, manager), "portable destination type guard");
        source.StateForAnalysis().opaque68 = 1;
        Throws<std::logic_error>([&] { source.vfunc_14(destination, manager); }, "opaque references are unqualified");
        source.StateForAnalysis().opaque68 = 0;
        Host host;
        int owner = 1, previous = 2, target = 3;
        source.BindForAnalysis(host, &owner, {});
        destination.BindForAnalysis(host, &previous, {});
        Check(source.vfunc_14(destination, manager) && host.detached == std::vector<void*>{&previous} &&
            destination.OwnerForAnalysis() == &owner, "destination reset precedes borrowed owner assignment");
        Check(source.vfunc_14(source, manager) && source.OwnerForAnalysis() == nullptr,
            "self copy observes reset of its own owner");
        source.BindForAnalysis(host, &owner, {&target});
        const auto detached = host.detached.size();
        Throws<std::logic_error>([&] { source.vfunc_14(destination, manager); }, "populated container copy is unqualified");
        Check(host.detached.size() == detached && destination.OwnerForAnalysis() == &owner,
            "portable qualification guard precedes side effects");
        // Host outlives all bound objects, including explicit borrowed-owner teardown.
        source.ResetForAnalysis(); destination.ResetForAnalysis();
        Check(host.detachedAfterClear, "teardown clears borrowed list before detach");
        Check(wxPerception::StaticRTTI().baseClassID == spBaseObject::ClassID, "independent physical base identity");
        auto factory = spRTTIManager::Instance().Create(wxPerception::ClassID);
        Check(dynamic_cast<wxPerception*>(factory.get()) != nullptr && factory->IsKindOf(spBaseObject::ClassID), "factory and RTTI");
    }

    void BoundedPcRefresh()
    {
        using Result=wxPerceptionPcRefreshResultForAnalysis;
        Host host;int owner=1;wxPerception perception;
        perception.BindForAnalysis(host,&owner,{});
        auto& state=perception.StateForAnalysis();state.opaque68=0x12345678;state.opaque6C=99;state.byte40=0xA5;
        spTimer clock;wxGameTimer timer(clock);timer.SetPausedByteForAnalysis(7);
        auto time=timer.GetStateForAnalysis();time.currentMilliseconds=100;timer.SetStateForAnalysis(time);
        Check(perception.RefreshBoundedPcForAnalysis(timer)==Result::PausedReturnFalse &&
            state.opaque68==0x12345678 && state.opaque6C==99 && state.byte40==0xA5,
            "any nonzero actual GameTimer paused byte returns false before Perception effects");
        timer.SetPausedByteForAnalysis(0);
        Check(perception.RefreshBoundedPcForAnalysis(timer)==Result::UnqualifiedOwnerScan &&
            state.opaque68==0 && state.opaque6C==0 && state.byte40==0,
            "expired6C cleared before68 and owner scan prefix clears40; no fabricated scan");
        perception.ResetForAnalysis();
        state.opaque68=0x12345678;state.opaque6C=100;state.byte40=0xA5;
        Check(perception.RefreshBoundedPcForAnalysis(timer)==Result::OwnerNullReturnTrue &&
            state.opaque68==0x12345678 && state.opaque6C==100 && state.byte40==0,
            "deadline equality retains opaque words and null owner finishes scan");
        state.opaque68=0;state.opaque6C=99;state.byte40=0xA5;
        Check(perception.RefreshBoundedPcForAnalysis(timer)==Result::OwnerNullReturnTrue && state.opaque6C==99,
            "zero68 skips reading or clearing6C");
        if constexpr(sizeof(std::uintptr_t)>4)
        {
            state.opaque68=1;state.opaque6C=static_cast<std::uintptr_t>(std::uint64_t{1}<<40);state.byte40=0xA5;
            Check(perception.RefreshBoundedPcForAnalysis(timer)==Result::UnqualifiedNativeDeadline && state.byte40==0xA5,
                "analytical deadline outside native32 representation refuses without inventing expiry");
        }
    }

    void PcNumerics()
    {
        using Profile = wxPerceptionNumericProfileForAnalysis;
        Host host;
        int owner = 0, first = 1, second = 2;
        host.positions = {{&owner, {0, 0, 0}}, {&first, {1, 0, 0}}, {&second, {1, 0x1p-30f, 0}}};
        wxPerception perception;
        perception.BindForAnalysis(host, &owner, {&first, &second});
        Check(perception.FindNearestForAnalysis(Profile::PcFiniteNearest64) == &first,
            "PC extended comparison distinguishes a distance lost by binary64");
        host.positions[&second] = {1, 0x1p-32f, 0};
        Check(perception.FindNearestForAnalysis(Profile::PcFiniteNearest64) == &second,
            "PC nearest-even extended half rounds to the even significand");
        host.positions[&second][1] = std::nextafter(0x1p-32f, 1.0f);
        Check(perception.FindNearestForAnalysis(Profile::PcFiniteNearest64) == &first,
            "PC upper neighbor of extended half crosses the comparison boundary");
        host.positions[&first] = {1, 0x1p-13f, 0};
        host.positions[&second] = {1, 0x1p-30f, 0};
        Check(perception.FindNearestForAnalysis(Profile::PcFiniteNearest64) == &first,
            "rounded-down stored minimum can reject a geometrically closer target");
        host.positions[&first] = {std::bit_cast<float>(2u), 0, 0};
        host.positions[&second] = {std::bit_cast<float>(1u), 0, 0};
        Check(perception.FindNearestForAnalysis(Profile::PcFiniteNearest64) == &first,
            "subnormal distance store underflows the running minimum to zero");
        host.positions[&first] = {-0.0f, 0, 0}; host.positions[&second] = {0, -0.0f, -0.0f};
        Check(perception.FindNearestForAnalysis(Profile::PcFiniteNearest64) == &second &&
            perception.StateForAnalysis().scratchWords30 == Words({0, -0.0f, -0.0f}), "PC signed zero scratch and ties");
        const float largest = std::numeric_limits<float>::max();
        host.positions[&first] = {largest, largest, largest}; host.positions[&second] = {-largest, -largest, -largest};
        Check(perception.FindNearestForAnalysis(Profile::PcFiniteNearest64) == nullptr &&
            perception.StateForAnalysis().scratchWords30 == Words({-largest, -largest, -largest}),
            "all finite distances above FLT_MAX leave selected null but update scratch");
        host.positions[&second] = {std::numeric_limits<float>::quiet_NaN(), 0, 0};
        Throws<std::domain_error>([&] { (void)perception.FindNearestForAnalysis(Profile::PcFiniteNearest64); },
            "PC profile explicitly rejects nonfinite arithmetic");
        Throws<std::invalid_argument>([&] { (void)perception.FindNearestForAnalysis(static_cast<Profile>(99)); },
            "unknown numeric profile rejected");

        float minimum = 1;
        Check(!winx::analysis::pc::UpdatePerceptionMinimumForAnalysis({0, 0, 0}, {1, 0x1p-30f, 0}, minimum) && minimum == 1,
            "PC rejected distance preserves the minimum");
        minimum = largest;
        Check(winx::analysis::pc::UpdatePerceptionMinimumForAnalysis({0, 0, 0}, {1, 0x1p-13f, 0}, minimum) && minimum == 1,
            "PC accepted distance is rounded only by its float32 store");
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--replay") Replay(std::cin);
        else { Queries(); CopiesAndLifetime(); PcNumerics(); BoundedPcRefresh(); std::cout << "wxPerception checks passed\n"; }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
