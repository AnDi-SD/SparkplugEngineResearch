#include "Analysis/PC/wxBacoProjectileManagerAbi.h"
#include "Analysis/PS2/wxBacoProjectileManagerAbi.h"
#include "Code/wxBacoProjectileManager.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace
{
    using namespace winx::reconstruction;
    using namespace sparkplug::reconstruction;

    void Require(bool condition, const char* message)
    {
        if (!condition) { std::cerr << "FAILED: " << message << '\n'; std::exit(EXIT_FAILURE); }
    }
    bool Near(float actual, float expected)
    { return std::fabs(actual - expected) < 0.0001f; }

    class Host final : public wxBacoProjectileManagerHost
    {
    public:
        int objects[4]{};
        bool active[2]{};
        bool scene = true;
        Position emitterPosition{0.0f, 0.0f, 0.0f};
        Position playerPosition{10.0f, 0.0f, 0.0f};
        Position firedOrigin{}, firedDirection{};
        std::vector<int> calls;
        int firedSlot = -1;
        bool HasSceneForAnalysis(const wxBacoProjectileManager&) noexcept override { return scene; }
        void AttachSceneForAnalysis(wxBacoProjectileManager&) noexcept override { calls.push_back(1); }
        void* FindEmitterForAnalysis(std::size_t index) noexcept override
        { calls.push_back(2 + static_cast<int>(index)); return &objects[index + 2]; }
        void* CreateProjectileForAnalysis(std::size_t index) noexcept override
        { calls.push_back(4 + static_cast<int>(index)); return &objects[index]; }
        void BaseSetupForAnalysis(wxBacoProjectileManager&) noexcept override { calls.push_back(6); }
        void DestroyProjectileForAnalysis(void* object) noexcept override
        { calls.push_back(object == &objects[0] ? 20 : 21); }
        bool IsProjectileActiveForAnalysis(void* object) noexcept override
        { return active[object == &objects[0] ? 0 : 1]; }
        void LaunchProjectileForAnalysis(void* object, const Position& origin,
            const Position& direction) noexcept override
        {
            firedSlot = object == &objects[0] ? 0 : 1;
            firedOrigin = origin;
            firedDirection = direction;
            active[firedSlot] = true;
            calls.push_back(10 + firedSlot);
        }
        void UpdateProjectileForAnalysis(void* object) noexcept override
        { calls.push_back(object == &objects[0] ? 30 : 31); }
        Position GetEmitterPositionForAnalysis(void*) noexcept override { return emitterPosition; }
        bool TryGetPlayerPositionForAnalysis(Position& position) noexcept override
        { position = playerPosition; return true; }
    };
}

int main()
{
    static_assert(sizeof(winx::evidence::pc::wxBacoProjectileManagerLayout) == 0x1E4);
    static_assert(sizeof(winx::evidence::ps2::wxBacoProjectileManagerLayout) == 0x200);
    auto factory = spRTTIManager::Instance().Create(wxBacoProjectileManager::ClassID);
    Require(dynamic_cast<wxBacoProjectileManager*>(factory.get()) != nullptr, "factory");
    Require(factory->IsKindOf(0x7DB63B02) && factory->IsKindOf(0x796A1869),
        "native RTTI ancestry");
    Require(dynamic_cast<wxBacoProjectileManager*>(factory->Clone().get()) != nullptr,
        "empty clone");

    Host host;
    {
        wxBacoProjectileManager manager;
        manager.SetHostForAnalysis(&host);
        const wxBacoProjectileManagerMessageForAnalysis setup{0x1C}, update{0x1E};
        manager.vfunc_0C(&setup);
        Require(host.calls == std::vector<int>({1, 2, 3, 4, 5, 6}),
            "setup creates exactly two slots after scene lookup");
        Require(manager.GetProjectileForAnalysis(0) == &host.objects[0]
            && manager.GetProjectileForAnalysis(1) == &host.objects[1], "slot ownership");
        manager.vfunc_0C(&update);
        Require(host.calls == std::vector<int>({1, 2, 3, 4, 5, 6, 30, 31}),
            "update walks both slots in order");
        Require(manager.FireTowardPlayerForAnalysis() && host.firedSlot == 0,
            "first available slot");
        Require(Near(host.firedDirection[0], 0.894427f)
            && Near(host.firedDirection[1], 0.447214f)
            && Near(host.firedDirection[2], 0.0f), "aim normalization and vertical bias");
        Require(manager.FireDirectionForAnalysis({0.0f, 0.0f, 2.0f})
            && host.firedSlot == 1 && Near(host.firedDirection[2], 1.0f),
            "second available slot and direction normalization");
        Require(!manager.FireDirectionForAnalysis({1.0f, 0.0f, 0.0f}), "full pool");
        Require(manager.Clone() == nullptr, "live pool clone boundary");
    }
    Require(host.calls[host.calls.size()-2] == 20 && host.calls.back() == 21,
        "owned projectiles destroyed in order");
    std::cout << "wxBacoProjectileManagerTests passed\n";
}
