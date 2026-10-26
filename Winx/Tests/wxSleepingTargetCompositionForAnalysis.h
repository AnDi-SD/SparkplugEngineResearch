#pragma once
#include "Code/wxIceGargoyleSleepingAIAction.h"
#include "Code/wxGameTimer.h"
#include "Code/wxAssetManager.h"
#include <optional>
#include <iostream>
#include <stdexcept>

namespace sleeping_target_tests
{
    using namespace winx::reconstruction;
    using namespace sparkplug::reconstruction;
    inline unsigned checks = 0;
    inline void Require(bool value, const char* text) { ++checks; if (!value) throw std::runtime_error(text); }
    struct AssetHost final : wxAssetManagerHost
    {
        bool IsPs2ForAnalysis() const noexcept override { return false; }
        std::uint32_t AppLanguageForAnalysis() const noexcept override { return 0; }
        void SetAppLanguageForAnalysis(std::uint32_t) noexcept override {}
        std::optional<std::string> ReadPcRegistryValueForAnalysis(std::string_view, std::string_view key) override
        { return key == "MediaPath" ? std::optional<std::string>{"A/"} : std::nullopt; }
        std::unique_ptr<wxAssetFileStreamForAnalysis> CreateFileStreamForAnalysis() override { return {}; }
        void ReportOpenFailureForAnalysis(std::string_view) noexcept override {}
    };
    struct LoadView { std::uint32_t stamp; unsigned command; bool published, resourceAbsent; };
    struct World final : wxIceGargoyleSleepingAIActionHost, wxTargetManagerHost
    {
        AssetHost assetHost;
        wxAssetManager asset{assetHost};
        spResourceManager resource;
        std::shared_ptr<spNode> root = std::make_shared<spNode>();
        std::vector<std::shared_ptr<wxTargetNodeBindingForAnalysis>> nodes;
        std::vector<std::weak_ptr<spNode>> weak;
        spTimer clock;
        wxGameTimer timer{clock};
        std::vector<std::unique_ptr<wxTargetManager>> owners;
        wxIceGargoyleSleepingAIAction* action = nullptr;
        int owner = 1;
        std::array<std::uint8_t, 0x80> command;
        std::string mode;
        unsigned times = 0, factories = 0, nestedTicks = 0;
        std::vector<LoadView> loads;
        wxSleepingTargetPlatformForAnalysis platform = wxSleepingTargetPlatformForAnalysis::PC;
        bool bound = true;
        explicit World(std::string value) : mode(std::move(value))
        {
            command.fill(0xA5); asset.InitializeForAnalysis();
            auto state = timer.GetStateForAnalysis(); state.currentMilliseconds = mode == "wrapped-repeat" ? 0xFFFFFF9C : 17;
            timer.SetStateForAnalysis(state);
            for (int i = 0; i < 2; ++i)
            {
                auto binding = std::make_shared<wxTargetNodeBindingForAnalysis>(); binding->node = std::make_shared<spNode>();
                weak.push_back(binding->node); nodes.push_back(std::move(binding));
            }
            if (mode == "retained") nodes[0]->referenceCount8 = 1;
        }
        void* ReadCommandForAnalysis(void* value) noexcept override
        { if (value != &owner) std::terminate(); return command.data(); }
        void WriteCommandByteForAnalysis(void* value, std::uint32_t offset, std::uint8_t byte) noexcept override
        { if (value != command.data() || offset != 0x1B) std::terminate(); command[offset] = byte; }
        std::uint32_t ReadCharacterStateWordForAnalysis(void*) noexcept override { return 1; }
        spTaskTimer& GameTimerPrefixForAnalysis() noexcept override { ++times; return timer; }
        bool HasExistingTargetManagerForAnalysis() noexcept override { return false; }
        wxSleepingTargetManagerBindingForAnalysis TargetManagerBindingForAnalysis() noexcept override
        { return {platform, bound ? this : nullptr, bound ? &owners : nullptr}; }
        void SelectActionForAnalysis(void*, std::uint32_t, std::uint32_t) noexcept override { std::terminate(); }
        wxAssetManager* ResolveTargetAssetManagerForAnalysis() override { ++factories; return &asset; }
        spResourceManager* ResolveTargetResourceManagerForAnalysis() override { return &resource; }
        std::shared_ptr<wxTargetNodeBindingForAnalysis> LoadTargetNodeForAnalysis(spResourceManager& service, const char* path) override
        {
            Require(&service == &resource && std::string(path) == "A/SFX\\arow.smo", "Real typed service/path composition");
            auto* current = wxTargetManager::GetInstanceForAnalysis(); Require(current != nullptr, "Real ctor publishes before loader");
            const auto index = loads.size(); Require(index < 2, "Bounded loader recursion");
            loads.push_back({action->StampForAnalysis(), command[0x1B], true, !current->ResourceForAnalysis()});
            if (mode == "mutate" && index == 0)
            { action->SetStateForAnalysis(1, 0x12345678); command[0x1B] = 0x7E; }
            if (mode == "nested-tick" && index == 0)
            {
                action->SetStateForAnalysis(1, 0); ++nestedTicks;
                Require(action->TickForAnalysis() == wxSleepingTickResultForAnalysis::Completed, "Nested own stage observes constructing singleton");
                Require(owners.empty() && factories == 1, "Pending real manager is published before app retains it");
            }
            if (mode == "republish" && index == 0)
            {
                auto nested = std::make_unique<wxTargetManager>(*this);
                Require(wxTargetManager::GetInstanceForAnalysis() == nested.get(), "Nested ctor changes singleton");
                owners.push_back(std::move(nested));
            }
            return nodes[index];
        }
        spNode* ResolveTargetSceneRootForAnalysis() override { return root.get(); }
    };
    inline void UInts(const std::vector<unsigned>& values)
    { std::cout << '['; for (std::size_t i = 0; i < values.size(); ++i) { if (i) std::cout << ','; std::cout << values[i]; } std::cout << ']'; }
    inline void Run(const std::string& mode, bool print)
    {
        World world(mode);
        auto action = std::make_unique<wxIceGargoyleSleepingAIAction>(world, wxSleepingTargetProfileForAnalysis::BoundedPcAcquisition);
        world.action = action.get(); action->SetOwnerForAnalysis(&world.owner); action->SetStateForAnalysis(1, 0);
        Require(action->TickForAnalysis() == wxSleepingTickResultForAnalysis::Completed, "Whole admitted cold composition returns");
        if (mode == "wrapped-repeat") Require(action->TickForAnalysis() == wxSleepingTickResultForAnalysis::Completed, "Wrapped stamp repeats against existing real manager");
        auto* captured = world.owners.back().get(); Require(wxTargetManager::GetInstanceForAnalysis() == captured, "Caller republishes captured outer result after nested constructor");
        const auto phase = action->PhaseForAnalysis(), stamp = action->StampForAnalysis(); const auto command = unsigned(world.command[0x1B]);
        std::vector<unsigned> refs;
        for (std::size_t i = 0; i < world.loads.size(); ++i)
        {
            const auto& node = world.nodes[i]; refs.push_back(node->referenceCount8);
            Require(node->node && !node->node->IsEnabledForAnalysis() && node->node->GetParentForAnalysis() == world.root.get()
                && node->node->GetScaleForAnalysis() == std::array<float,3>{2,2,2}, "Real Target body scales/disables/attaches actual Node");
        }
        const auto children = world.root->GetChildCountForAnalysis();
        const auto owners = world.owners.size(); action.reset(); world.action = nullptr;
        Require(wxTargetManager::GetInstanceForAnalysis() == captured && world.owners.size() == owners
            && world.root->GetChildCountForAnalysis() == children, "Action destructor does not own managers/resources");
        for (std::size_t i = 0; i < refs.size(); ++i) Require(world.nodes[i]->referenceCount8 == refs[i], "No target release on action destruction");
        world.owners.clear(); std::vector<unsigned> finalRefs; std::vector<bool> alive;
        for (std::size_t i = 0; i < refs.size(); ++i) { finalRefs.push_back(world.nodes[i]->referenceCount8); alive.push_back(!world.weak[i].expired()); }
        Require(world.root->GetChildCountForAnalysis() == 0 && !wxTargetManager::GetInstanceForAnalysis(), "Separate app release detaches/deletes/clears singleton");
        Require(finalRefs == (mode == "retained" ? std::vector<unsigned>{1} : std::vector<unsigned>(refs.size(),0)), "Actual reference lifetime");
        if (!print) return;
        std::cout << "{\"mode\":\"" << mode << "\",\"phase\":" << phase << ",\"stamp\":" << stamp << ",\"command\":" << command
            << ",\"timeReads\":" << world.times << ",\"factories\":" << world.factories << ",\"loads\":[";
        for (std::size_t i = 0; i < world.loads.size(); ++i)
        {
            if (i) std::cout << ','; const auto& load = world.loads[i];
            std::cout << "{\"stamp\":" << load.stamp << ",\"command\":" << load.command << ",\"published\":true,\"resourceAbsent\":" << (load.resourceAbsent ? "true" : "false") << '}';
        }
        std::cout << "],\"nestedTicks\":" << world.nestedTicks << ",\"owners\":" << owners << ",\"refsBefore\":"; UInts(refs);
        std::cout << ",\"childrenBefore\":" << children << ",\"afterAction\":{\"globalPreserved\":true,\"refs\":"; UInts(refs);
        std::cout << ",\"children\":" << children << "},\"finalRefs\":"; UInts(finalRefs);
        std::cout << ",\"finalAlive\":["; for (std::size_t i = 0; i < alive.size(); ++i) { if (i) std::cout << ','; std::cout << (alive[i] ? "true" : "false"); }
        std::cout << "],\"finalChildren\":0,\"globalCleared\":true,\"capturedFactoryResultPublished\":true}";
    }
    inline unsigned UnitTests()
    {
        checks = 0;
        for (auto mode : {"cold","wrapped-repeat","retained","mutate","nested-tick","republish"}) Run(mode, false);
        for (bool ps2 : {false,true})
        {
            World world("cold"); world.bound = false; if (ps2) world.platform = wxSleepingTargetPlatformForAnalysis::PS2;
            wxIceGargoyleSleepingAIAction action(world, wxSleepingTargetProfileForAnalysis::BoundedPcAcquisition);
            action.SetOwnerForAnalysis(&world.owner); action.SetStateForAnalysis(1,0);
            Require(action.TickForAnalysis() == wxSleepingTickResultForAnalysis::UnqualifiedTargetManagerAcquisition
                && action.StampForAnalysis() == 117 && world.factories == 0 && world.owners.empty(), "Missing/PS2 binding guard preserves known prefix");
        }
        return checks;
    }
}
