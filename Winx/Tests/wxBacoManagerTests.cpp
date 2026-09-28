#include "Analysis/PC/wxBacoManagerAbi.h"
#include "Analysis/PS2/wxBacoManagerAbi.h"
#include "Code/wxBacoManager.h"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace
{
    using namespace winx::reconstruction;
    using namespace sparkplug::reconstruction;

    void Require(bool value, const char* message)
    {
        if (!value) { std::cerr << "FAILED: " << message << '\n'; std::exit(EXIT_FAILURE); }
    }

    class Host final : public wxBacoManagerHost
    {
    public:
        std::uint32_t tick = 1000;
        std::array<float, 3> player{-4097.0f, 950.0f, -1111.0f};
        std::vector<std::uint32_t> random{0, 2000, 234, 7, 8, 9, 10, 11, 12};
        std::size_t randomIndex = 0;
        std::vector<unsigned> calls;
        std::uint32_t GetTickForAnalysis() noexcept override { return tick; }
        std::uint32_t NextRandomForAnalysis() noexcept override { return random[randomIndex++]; }
        std::array<float, 3> GetPlayerPositionForAnalysis() noexcept override { return player; }
        void SetupSceneForAnalysis(wxBacoManager&) noexcept override { calls.push_back(8); }
        void SpawnMemberForAnalysis(wxBacoManager&, void* member,
            std::uint32_t slot) noexcept override
        {
            calls.push_back(slot);
            spawned.push_back(member);
        }
        std::vector<void*> spawned;
    };
}

int main()
{
    static_assert(sizeof(winx::evidence::pc::wxBacoManagerLayout) == 0x1EC);
    static_assert(sizeof(winx::evidence::ps2::wxBacoManagerLayout) == 0x200);
    wxBacoManager manager;
    Require(manager.IsExactly(wxBacoManager::ClassID)
        && manager.IsKindOf(0x796A1869)
        && manager.IsKindOf(spNamedObject::ClassID), "RTTI ancestry");
    Require(manager.GetStateForAnalysis().deadlines[0] == 0
        && !manager.GetStateForAnalysis().reachedFirst
        && !manager.GetStateForAnalysis().waveSecond, "constructor flags");
    Require(dynamic_cast<wxBacoManager*>(manager.Clone().get()) != nullptr,
        "default clone");
    auto factory = spRTTIManager::Instance().Create(wxBacoManager::ClassID);
    Require(dynamic_cast<wxBacoManager*>(factory.get()) != nullptr, "factory");

    Host host;
    manager.SetHostForAnalysis(&host);
    const wxBacoManagerMessageForAnalysis setup{0x1C};
    manager.vfunc_0C(&setup);
    Require(host.calls == std::vector<unsigned>{8}
        && manager.GetStateForAnalysis().nextProximityCheck == 1500,
        "setup notification and first polling deadline");
    int first = 1, second = 2, third = 3;
    for (void* member : {static_cast<void*>(&first), static_cast<void*>(&second), static_cast<void*>(&third)})
    {
        const wxBacoManagerMessageForAnalysis add{0x273D, member};
        manager.vfunc_0C(&add);
    }
    Require(manager.GetMembersForAnalysis().size() == 3, "append notification");

    manager.CheckProximityForAnalysis({-3697.0f, 950.0f, -1111.0f});
    Require(!manager.GetStateForAnalysis().reachedFirst
        && manager.GetStateForAnalysis().nextProximityCheck == 1500,
        "radius boundary is excluded");
    manager.CheckProximityForAnalysis(host.player);
    const auto& state = manager.GetStateForAnalysis();
    Require(state.reachedFirst && state.waveFirst && !state.reachedSecond
        && state.deadlines[0] == 4000 && state.deadlines[1] == 6000
        && state.deadlines[2] == 4234 && state.nextProximityCheck == 1500,
        "first trigger timing");
    manager.CheckProximityForAnalysis({-2722.0f, 950.0f, -736.0f});
    Require(state.reachedSecond && state.waveSecond && state.secondCount == 1
        && state.deadlines[3] == 2007 && state.deadlines[4] == 2008
        && state.deadlines[5] == 2009,
        "second trigger timing and initial count");
    const wxBacoManagerMessageForAnalysis update{0x1E};
    manager.vfunc_0C(&update);
    Require(host.calls == std::vector<unsigned>{8}, "future deadlines do not spawn");
    host.tick = 2007;
    manager.vfunc_0C(&update);
    Require(host.calls == std::vector<unsigned>{8}, "equal deadline does not spawn");
    host.tick = 3000;
    manager.vfunc_0C(&update);
    Require(host.calls == std::vector<unsigned>({8, 3, 4})
        && host.spawned == std::vector<void*>({&second, &third})
        && !state.waveSecond && state.secondCount == 3
        && state.deadlines[3] == 4010 && state.deadlines[4] == 4011,
        "second wave follows deadline order");
    host.tick = 7000;
    manager.vfunc_0C(&update);
    Require(host.calls == std::vector<unsigned>({8, 3, 4, 0})
        && host.spawned.back() == &first
        && !state.waveFirst && state.firstCount == 1
        && state.deadlines[0] == 8012,
        "first wave stops after half the members");
    Require(manager.Clone() == nullptr, "modified clone boundary");

    Host pollingHost;
    wxBacoManager polling;
    polling.SetHostForAnalysis(&pollingHost);
    pollingHost.tick = 500;
    polling.vfunc_0C(&update);
    Require(polling.GetStateForAnalysis().reachedFirst
        && polling.GetStateForAnalysis().nextProximityCheck == 1000,
        "update runs first proximity check");
    pollingHost.player = {-2722.0f, 950.0f, -736.0f};
    pollingHost.tick = 1000;
    polling.vfunc_0C(&update);
    Require(!polling.GetStateForAnalysis().reachedSecond,
        "equal polling deadline is excluded");
    pollingHost.tick = 1001;
    polling.vfunc_0C(&update);
    Require(polling.GetStateForAnalysis().reachedSecond,
        "later update runs second proximity check");
    std::cout << "wxBacoManager reconstruction tests passed\n";
}
