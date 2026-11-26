#include "Code/wxArrowTrap.h"
#include "Analysis/PC/wxArrowTrapAbi.h"
#include "Analysis/PS2/wxArrowTrapAbi.h"
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
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

    struct PcRangeCase
    {
        std::array<std::uint32_t, 3> node;
        std::array<std::uint32_t, 3> point;
        std::uint8_t initialFlag;
        std::uint8_t finalFlag;
    };
    constexpr PcRangeCase pcCases[]{
        {{{0x00000000u, 0x80000000u, 0x00000000u}}, {{0x80000000u, 0x00000000u, 0x00000000u}}, 0x01, 1},
        {{{0x4479c000u, 0x447a0000u, 0x00000000u}}, {{0x00000000u, 0x00000000u, 0x00000000u}}, 0x00, 1},
        {{{0xc47a0000u, 0xc47a0000u, 0x00000000u}}, {{0x00000000u, 0x00000000u, 0x00000000u}}, 0xa5, 1},
        {{{0x447a0000u, 0x00000000u, 0x447a0000u}}, {{0x00000000u, 0x00000000u, 0x00000000u}}, 0x01, 1},
        {{{0x447a4000u, 0x447a0000u, 0x00000000u}}, {{0x00000000u, 0x00000000u, 0x00000000u}}, 0x00, 0},
        {{{0x447a0000u, 0x3f800000u, 0x447a0000u}}, {{0x00000000u, 0x00000000u, 0x00000000u}}, 0x01, 0},
        {{{0x447a0000u, 0x00000000u, 0x447a0000u}}, {{0xa6800000u, 0x00000000u, 0x00000000u}}, 0xff, 0},
        {{{0x447a0000u, 0x00000000u, 0x447a0000u}}, {{0x26800000u, 0x00000000u, 0x00000000u}}, 0x01, 1},
        {{{0x447a0000u, 0x447a0000u, 0x34900000u}}, {{0x00000000u, 0x00000000u, 0x00000000u}}, 0xa5, 1},
        {{{0x46800000u, 0x44800000u, 0xc6800000u}}, {{0x46706000u, 0x447fc000u, 0xc6706000u}}, 0x55, 0},
        {{{0x46800000u, 0x447fe000u, 0xc6800000u}}, {{0x46706400u, 0x44800000u, 0xc6706000u}}, 0x7f, 1},
        {{{0x00800000u, 0x80800000u, 0x80000000u}}, {{0x00000000u, 0x00000000u, 0x00000000u}}, 0x80, 1},
    };

    wxArrowPosition DecodePosition(const std::array<std::uint32_t, 3>& words) noexcept
    {
        wxArrowPosition point;
        for (std::size_t axis = 0; axis < point.size(); ++axis)
            std::memcpy(&point[axis], &words[axis], sizeof(point[axis]));
        return point;
    }

    void CheckPcRange()
    {
        Host host;
        wxArrowTrap trap(host);
        auto& state = trap.GetStateForAnalysis();
        state.active = 0x93;
        state.phase[1] = 0xA7;
        state.timer[1] = 0.625f;
        for (const auto& test : pcCases)
        {
            const auto node = DecodePosition(test.node), point = DecodePosition(test.point);
            state.inRange = test.initialFlag;
            const auto result = trap.TryUpdatePcRangeForAnalysis(node, point);
            Check(result && *result == 1, "PC range AL");
            Check(state.inRange == test.finalFlag, "PC range final byte");
            Check(state.active == 0x93 && state.phase[1] == 0xA7 && state.timer[1] == 0.625f,
                "PC range preserves adjacent state");
        }

        constexpr std::uint32_t refused[]{
            0x00000001u, 0x807FFFFFu, 0x7F800000u, 0xFFC00000u, 0x46800001u,
        };
        for (const auto bits : refused)
        {
            const auto invalid = DecodePosition({0, bits, 0});
            const wxArrowPosition zero{};
            state.inRange = 0xA5;
            Check(!trap.TryUpdatePcRangeForAnalysis(invalid, zero) && state.inRange == 0xA5,
                "PC range rejects root coordinates outside its domain");
            Check(!trap.TryUpdatePcRangeForAnalysis(zero, invalid) && state.inRange == 0xA5,
                "PC range rejects argument coordinates outside its domain");
        }
        std::cout << "ArrowTrapRangePrecision: 46 contracts passed\n";
    }

    bool ReadHex(std::istream& stream, std::size_t width, std::uint32_t& value)
    {
        std::string word;
        if (!(stream >> word) || word.size() != width ||
            word.find_first_not_of("0123456789abcdef") != std::string::npos)
            return false;
        const auto end = word.data() + word.size();
        const auto converted = std::from_chars(word.data(), end, value, 16);
        return converted.ptr == end && converted.ec == std::errc{};
    }

    int ReplayPcRange()
    {
        Host host;
        wxArrowTrap trap(host);
        std::cout << std::hex << std::setfill('0');
        std::string line;
        while (std::getline(std::cin, line))
        {
            std::istringstream row(line);
            std::array<std::uint32_t, 3> node, point;
            for (auto* words : {&node, &point})
                for (auto& coordinate : *words)
                    if (!ReadHex(row, 8, coordinate))
                        return 2;
            std::uint32_t flag;
            std::string extra;
            if (!ReadHex(row, 2, flag) || row >> extra)
                return 2;

            auto& state = trap.GetStateForAnalysis();
            state.inRange = static_cast<std::uint8_t>(flag);
            const auto result = trap.TryUpdatePcRangeForAnalysis(DecodePosition(node), DecodePosition(point));
            if (!result)
                return 2;
            std::cout << std::setw(2) << unsigned(*result) << ' '
                      << std::setw(2) << unsigned(state.inRange) << '\n';
        }
        return std::cin.eof() ? 0 : 2;
    }
}
int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "--replay")
        return ReplayPcRange();
    if (argc != 1)
        return 2;

    CheckPcRange();
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
