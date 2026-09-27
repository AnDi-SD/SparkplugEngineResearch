#include "Code/wxArrowTrap.h"
#include "Analysis/PC/wxArrowTrapAbi.h"
#include "Analysis/PS2/wxArrowTrapAbi.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
    using namespace winx::reconstruction;
    using namespace sparkplug::reconstruction;
    void Check(bool ok, const char* message)
    {
        if (!ok) { std::cerr << message << '\n'; std::exit(1); }
    }
    struct Node
    {
        std::string name;
        wxArrowPosition position{};
        std::vector<void*> children;
        bool render = false;
        void* component = nullptr;
    };
    struct Host final : wxArrowTrapHost
    {
        Node root{"root", {0,0,0}}, first{"fleche_a", {300,50,0}},
            second{"fleche_b", {0,80,200}}, start{"arrowstart_main", {0,10,0}},
            emitter{"drawable", {}, {}, true};
        std::uint16_t componentWord = 0;
        std::uint8_t componentFlag = 0;
        int constructed = 0, destroyed = 0, copied = 0, created = 0, freed = 0;
        int notifications = 0, transformUpdates = 0, targetUpdates = 0, dirty = 0;
        std::uint32_t clock = 17, random = 1, entityTime = 0;
        float delta = 0.5f;
        bool copyOkay = true;
        std::string properties;
        Host()
        {
            root.children = {&first, &second, &start};
            first.children = {&emitter};
        }
        void ConstructEntityForAnalysis(wxArrowTrap& trap) override
        {
            ++constructed;
            trap.GetStateForAnalysis().speed = 17.0f;
            trap.GetStateForAnalysis().entityWord84 = 99;
        }
        void DestroyEntityForAnalysis(wxArrowTrap&) noexcept override { ++destroyed; }
        bool CopyEntityForAnalysis(const wxArrowTrap&, wxArrowTrap&, spCloneManager&) const override
        { ++const_cast<Host*>(this)->copied; return copyOkay; }
        void* RootNodeForAnalysis(const wxArrowTrap&) const noexcept override { return const_cast<Node*>(&root); }
        std::vector<void*> ChildrenForAnalysis(void* p) const override { return static_cast<Node*>(p)->children; }
        std::string_view NameForAnalysis(void* p) const noexcept override { return static_cast<Node*>(p)->name; }
        wxArrowPosition PositionForAnalysis(void* p) const noexcept override { return static_cast<Node*>(p)->position; }
        void SetPositionAndDirtyForAnalysis(void* p, const wxArrowPosition& v) noexcept override
        { static_cast<Node*>(p)->position = v; ++dirty; }
        bool HasRenderDataForAnalysis(void* p) const noexcept override { return static_cast<Node*>(p)->render; }
        void* ComponentForAnalysis(void* p) const noexcept override { return static_cast<Node*>(p)->component; }
        void* CreateComponentForAnalysis() override { ++created; return &componentWord; }
        void AttachComponentForAnalysis(void* p, void* c) noexcept override { static_cast<Node*>(p)->component = c; }
        void EnableComponentFlagForAnalysis(void*, std::uint8_t f) noexcept override { componentFlag |= f; }
        void SetComponentWordForAnalysis(void*, std::uint16_t w) noexcept override { componentWord = w; }
        void DestroyComponentForAnalysis(void*) noexcept override { ++freed; }
        std::uint32_t RandomForAnalysis() noexcept override { return random; }
        float DeltaSecondsForAnalysis() noexcept override { return delta; }
        std::uint32_t MillisecondsForAnalysis() noexcept override { return clock; }
        void UpdateEntityTransformForAnalysis(wxArrowTrap&) noexcept override { ++transformUpdates; }
        void UpdateTargetTransformForAnalysis(wxArrowTrap&, void*) noexcept override { ++targetUpdates; }
        void NormalizeForAnalysis(wxArrowPosition& v) noexcept override
        {
            const float length = std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
            if (length > 0) for (float& x : v) x /= length;
        }
        void SetEntityTimeForAnalysis(wxArrowTrap&, std::uint32_t t) noexcept override { entityTime = t; }
        void NotifyForAnalysis(wxArrowTrap&, std::uint32_t c, std::uint32_t g,
            std::uint32_t a, std::uint32_t b) noexcept override
        { Check(c==0x27D1&&g==0x11&&a==0xBB&&b==0, "notification payload"); ++notifications; }
        void AfterSetupForAnalysis(wxArrowTrap&, void* p) noexcept override { Check(p==&root,"setup root"); }
        void RegisterFloatPropertyForAnalysis(const char* s, std::uint32_t o) override
        { Check(o==0x128,"speed offset"); properties+=s; }
        void RegisterWordPropertyForAnalysis(const char* s, std::uint32_t o) override
        { Check(o==0x12C,"pause offset"); properties+=';';properties+=s; }
        bool RegisterEntityPropertiesForAnalysis() override { properties+=";entity";return false; }
    };
}
int main()
{
    using namespace winx::reconstruction;
    using namespace sparkplug::reconstruction;
    static_assert(winx::evidence::pc::wxArrowTrapFactory == 0x407A60);
    static_assert(winx::evidence::ps2::wxArrowTrapFactory == 0x3E6A90);
    Host host;
    {
        wxArrowTrap trap(host);
        auto& s = trap.GetStateForAnalysis();
        Check(s.speed==2000.0f&&s.pauseMilliseconds==1000&&s.entityWord84==1&&s.inRange==1,
            "constructor defaults");
        wxArrowTrapMessageForAnalysis setup{0x1C};
        trap.vfunc_0C(&setup);
        Check(s.active==1&&s.intervalSeconds==1.0f&&s.timer[1]==0.5f&&s.timer[0]==0,
            "setup and staggered timer");
        Check(s.arrows[0]==&host.first&&s.arrows[1]==&host.second&&s.arrows[2]==&host.start,
            "scene child selection");
        Check(s.emitters[0]==&host.emitter&&s.emitters[1]==&host.emitter&&
            host.created==1&&host.componentWord==0x200&&host.componentFlag==2&&host.entityTime==17,
            "emitter discovery and component ownership");
        wxArrowTrapMessageForAnalysis update{0x1E};
        trap.vfunc_0C(&update);
        Check(s.phase[0]==0&&s.phase[1]==1&&host.notifications==1&&host.transformUpdates==1,
            "independent channel timer");
        s.speed=100.0f;host.delta=0.5f;
        trap.vfunc_0C(&update);
        Check(host.targetUpdates==1&&s.phase[1]==1&&host.second.position[0]==0&&
            host.second.position[1]==80&&host.second.position[2]>0&&host.second.position[2]<200,
            "arrow flight and native Y coordinate");
        host.delta=2.0f;
        trap.vfunc_0C(&update);
        Check(s.phase[1]==0&&s.timer[1]==0&&host.second.position==s.arrowPositions[1],
            "arrow endpoint resets channel");
        Check(trap.vfunc_2C_InRangeForAnalysis({1000,1000,0})&&s.inRange==1,
            "inclusive squared distance boundary");
        Check(trap.vfunc_2C_InRangeForAnalysis({1001,1000,0})&&s.inRange==0,
            "far distance flag");
        const int updates=host.transformUpdates;
        host.delta=0;trap.vfunc_0C(&update);
        Check(host.transformUpdates==updates,"far target skips entity transform");
        spCloneManager manager;
        auto clone = manager.Clone(trap);
        Check(clone&&clone->IsKindOf(0x796A1869),"factory ancestry and clone");
        auto& target=static_cast<wxArrowTrap&>(*clone);
        Check(target.GetStateForAnalysis().speed==100.0f&&target.GetStateForAnalysis().active==0,
            "copy transfers own three fields only");
        host.copyOkay=false;s.speed=50;
        Check(!trap.vfunc_14(target,manager)&&target.GetStateForAnalysis().speed==100,
            "parent Copy failure stops leaf writes");
        host.copyOkay=true;
        Check(wxArrowTrap::RegisterPropertiesForAnalysis(host)&&
            host.properties=="Speed (cm/s);Pause between arrow shot (ms);entity", "properties");
        wxArrowTrap::SetFactoryHostForAnalysis(&host);
        auto made=spRTTIManager::Instance().Create(wxArrowTrap::ClassID);
        Check(made&&made->IsExactly(wxArrowTrap::ClassID),"registered factory");
        wxArrowTrap::SetFactoryHostForAnalysis(nullptr);
    }
    Check(host.freed==1&&host.destroyed==3,"owned component and entity teardown");
    {
        Host incomplete;
        incomplete.root.children.pop_back();
        wxArrowTrap trap(incomplete);
        auto& state=trap.GetStateForAnalysis();
        state.pauseMilliseconds=0;
        wxArrowTrapMessageForAnalysis setup{0x1C}, update{0x1E}, unknown{0x99};
        trap.vfunc_0C(&setup);
        Check(state.pauseMilliseconds==1&&state.intervalSeconds==0.001f&&
            state.active==0&&incomplete.created==0&&incomplete.entityTime==17,
            "minimum pause and missing start node");
        trap.vfunc_0C(&update);trap.vfunc_0C(&unknown);
        Check(incomplete.notifications==0&&incomplete.transformUpdates==0,
            "inactive and unknown notification");
    }
    std::cout << "wxArrowTrap checks passed\n";
}
