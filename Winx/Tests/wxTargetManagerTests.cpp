#include "Code/wxTargetManager.h"
#include "Code/wxAssetManager.h"
#include "Analysis/PC/wxTargetManagerAbi.h"
#include <cstring>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace sparkplug::reconstruction;
using namespace winx::reconstruction;
namespace
{
    int checks = 0;
    void Check(bool value, const char* label)
    { ++checks; if (!value) throw std::runtime_error(label); }
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
    struct Host final : wxTargetManagerHost
    {
        AssetHost assetHost;
        wxAssetManager asset{assetHost};
        spResourceManager manager;
        std::shared_ptr<spNode> root = std::make_shared<spNode>();
        std::vector<std::shared_ptr<wxTargetNodeBindingForAnalysis>> nodes;
        std::vector<std::weak_ptr<spNode>> weakNodes;
        std::vector<std::string> paths;
        std::string mode;
        bool publishedAtLoad = false;
        explicit Host(std::string value) : mode(std::move(value))
        {
            asset.InitializeForAnalysis();
            for (int i = 0; i < 3; ++i)
            {
                auto binding = std::make_shared<wxTargetNodeBindingForAnalysis>();
                binding->node = std::make_shared<spNode>(); weakNodes.push_back(binding->node); nodes.push_back(std::move(binding));
            }
            if (mode == "retained") nodes[0]->referenceCount8 = 1;
        }
        wxAssetManager* ResolveTargetAssetManagerForAnalysis() override { return &asset; }
        spResourceManager* ResolveTargetResourceManagerForAnalysis() override { return &manager; }
        std::shared_ptr<wxTargetNodeBindingForAnalysis> LoadTargetNodeForAnalysis(spResourceManager& service, const char* path) override
        {
            Check(&service == &manager, "Typed real ResourceManager receiver");
            Check(std::string(path) == "A/SFX\\arow.smo", "Actual shared BuildPath retains original key spelling");
            publishedAtLoad = wxTargetManager::GetInstanceForAnalysis() != nullptr;
            const auto index = (mode == "reinit" || mode == "clone") ? paths.size() : 0;
            paths.emplace_back(path); return nodes.at(index);
        }
        spNode* ResolveTargetSceneRootForAnalysis() override
        {
            auto* target = wxTargetManager::GetInstanceForAnalysis();
            Check(target && target->ResourceForAnalysis() && !target->ResourceForAnalysis()->node->IsEnabledForAnalysis(),
                "Scene root resolution follows direct resource Disable");
            return root.get();
        }
        int ID(const std::shared_ptr<wxTargetNodeBindingForAnalysis>& value) const
        {
            if (!value) return 0;
            for (std::size_t i = 0; i < nodes.size(); ++i) if (nodes[i] == value) return static_cast<int>(i + 1);
            throw std::runtime_error("unknown resource binding");
        }
    };
    template<class T> void Array(const T& values)
    {
        std::cout << '['; bool first = true;
        for (auto value : values) { if (!first) std::cout << ','; first = false; std::cout << +value; }
        std::cout << ']';
    }
    void State(const wxTargetManager& target, const Host& host)
    {
        std::cout << "{\"current\":"; Array(target.CurrentWordsForAnalysis());
        std::cout << ",\"bytes\":"; Array(target.DefaultFlagsForAnalysis());
        std::cout << ",\"node\":" << host.ID(target.ResourceForAnalysis()) << ",\"own\":";
        Array(target.OptionalDefaultWordsForAnalysis()); std::cout << ",\"listCount\":"
            << target.RegisteredCandidatesForAnalysis().size() << '}';
    }
    void Nodes(const Host& host, bool full)
    {
        std::cout << '[';
        for (std::size_t i = 0; i < host.nodes.size(); ++i)
        {
            if (i) std::cout << ',';
            auto node = host.weakNodes[i].lock();
            std::cout << "{\"refs\":" << host.nodes[i]->referenceCount8 << ",\"alive\":" << (node ? "true" : "false");
            if (full && node)
            {
                std::cout << ",\"scale\":[";
                const auto& scale = node->GetScaleForAnalysis();
                for (std::size_t j = 0; j < 3; ++j)
                {
                    std::uint32_t bits; std::memcpy(&bits, &scale[j], sizeof(bits));
                    if (j) std::cout << ','; std::cout << bits;
                }
                std::cout << "],\"flags\":" << node->GetFlagsForAnalysis() << ",\"parent\":"
                    << static_cast<int>(node->GetParentForAnalysis() == host.root.get());
            }
            std::cout << '}';
        }
        std::cout << ']';
    }
    void Replay(const std::string& mode)
    {
        Host host(mode);
        auto target = std::make_unique<wxTargetManager>(host);
        Check(host.publishedAtLoad && target->IsExactly(wxTargetManager::ClassID) && target->IsKindOf(spBaseObject::ClassID),
            "Published real derived identity before foreign Load");
        std::cout << "{\"mode\":\"" << mode << "\",\"constructor\":"; State(*target, host);
        std::cout << ",\"constructorNodes\":"; Nodes(host, true);
        std::unique_ptr<spBaseObject> clone;
        if (mode == "reinit") target->InitializeResourceForAnalysis();
        else if (mode == "reset") { target->ResetDefaultLifetimeForAnalysis(); target->ResetDefaultLifetimeForAnalysis(); }
        else if (mode == "clone") clone = target->Clone();
        else if (mode == "copy")
        {
            spBaseObject unrelated; spCloneManager manager;
            Check(target->vfunc_14(unrelated, manager), "Literal root Copy accepts unrelated destination and has no effects");
        }
        else if (mode == "notify-ignore")
        {
            std::uint32_t code = 0x12345678; target->vfunc_0C(&code);
            Check(target->LastNotificationResultForAnalysis() == wxTargetNotificationResultForAnalysis::Ignored, "Native default Notify ignore");
        }
        else Check(mode == "lifetime" || mode == "retained", "Known replay mode");
        std::cout << ",\"after\":"; State(*target, host); std::cout << ",\"afterNodes\":"; Nodes(host, true);
        std::cout << ",\"clone\":";
        if (clone) State(static_cast<const wxTargetManager&>(*clone), host); else std::cout << "null";
        clone.reset(); target.reset();
        std::cout << ",\"final\":"; Nodes(host, false);
        std::cout << ",\"loads\":" << host.paths.size() << ",\"rootChildren\":" << host.root->GetChildCountForAnalysis()
            << ",\"globalCleared\":" << (wxTargetManager::GetInstanceForAnalysis() ? "false" : "true") << '}';
    }
    void RegistryReplay(std::uint8_t flag, const std::array<unsigned, 3>& initial, bool mutate)
    {
        Host host("lifetime");
        std::array<wxTargetCandidateBindingForAnalysis, 3> members{{{flag},{0},{flag}}};
        wxTargetManager target(host);
        auto ptr = [&](unsigned i) -> const wxTargetCandidateBindingForAnalysis*
        { return i ? &members.at(i - 1) : nullptr; };
        target.SeedBorrowedSelectionsForAnalysis(ptr(initial[0]), ptr(initial[1]), ptr(initial[2]));
        auto id = [&](std::uintptr_t value)
        {
            if (!value) return 0u;
            for (unsigned i = 0; i < members.size(); ++i)
                if (value == reinterpret_cast<std::uintptr_t>(&members[i])) return i + 1;
            throw std::logic_error("Unknown borrowed registry identity");
        };
        auto state = [&]
        {
            std::array<unsigned,3> current{};
            for (unsigned i=0;i<3;++i) current[i]=id(target.CurrentWordsForAnalysis()[i]);
            std::cout << "{\"current\":"; Array(current);
            std::cout << ",\"flags\":"; Array(target.DefaultFlagsForAnalysis());
            std::cout << ",\"members\":[";
            bool first=true;
            for (auto* member : target.RegisteredCandidatesForAnalysis())
            { if (!first) std::cout << ','; first=false; std::cout << id(reinterpret_cast<std::uintptr_t>(member)); }
            std::cout << "],\"count\":" << target.RegisteredCandidatesForAnalysis().size() << '}';
        };
        constexpr std::array<std::array<unsigned,2>,6> sequence{{
            {{0x59DF70,0}},{{0x59DF70,1}},{{0x59DF70,0}},{{0x59DF20,0}},{{0x59DF20,2}},{{0x59DF70,2}}}};
        std::cout << "{\"rows\":["; bool first=true;
        unsigned step=0;
        for (auto [method,index] : sequence)
        {
            if (mutate && step==2) members[0].flag148=static_cast<std::uint8_t>(!flag);
            ++step;
            if (!first) std::cout << ','; first=false;
            std::cout << "{\"method\":" << method << ",\"argument\":" << index+1 << ",\"before\":"; state();
            if (method==0x59DF70) target.RegisterCandidateForAnalysis(members[index]);
            else target.UnregisterCandidateForAnalysis(&members[index]);
            std::cout << ",\"after\":"; state(); std::cout << '}';
        }
        target.ResetDefaultLifetimeForAnalysis();
        std::cout << "],\"afterReset\":"; state(); std::cout << '}';
    }
    void RegistryOwnership()
    {
        Host host("lifetime"); wxTargetManager target(host);
        wxTargetCandidateBindingForAnalysis a{},b{0xFF};
        target.RegisterCandidateForAnalysis(a); target.RegisterCandidateForAnalysis(a);
        target.RegisterCandidateForAnalysis(b);
        Check(target.RegisteredCandidatesForAnalysis()==std::vector<const wxTargetCandidateBindingForAnalysis*>{&a},
            "Zero-byte registry suppresses duplicate identities; nonzero byte bypasses list");
        target.SeedBorrowedSelectionsForAnalysis(&b,&b,&b);
        target.UnregisterCandidateForAnalysis(&b);
        Check(target.CurrentWordsForAnalysis()==std::array<std::uintptr_t,3>{0,0,reinterpret_cast<std::uintptr_t>(&b)},
            "Removal clears both aliases20/24 but preserves override28");
        a.flag148=1; target.RegisterCandidateForAnalysis(a);
        Check(target.RegisteredCandidatesForAnalysis().size()==1 &&
            target.CurrentWordsForAnalysis()[2]==reinterpret_cast<std::uintptr_t>(&a),
            "Registration rereads the live borrowed flag without removing an existing entry");
        target.ResetDefaultLifetimeForAnalysis();
        Check(target.RegisteredCandidatesForAnalysis().empty() && a.flag148==1 && b.flag148==0xFF,
            "Reset clears list/aliases without mutating or owning candidate bindings");
    }
    void Guards()
    {
        Host host("lifetime"); wxTargetManager target(host);
        auto* node = target.ResourceForAnalysis()->node.get();
        for (auto code : {0x2712u, 0x2713u})
        {
            target.vfunc_0C(&code);
            Check(target.LastNotificationResultForAnalysis() == (code == 0x2712
                ? wxTargetNotificationResultForAnalysis::UnqualifiedActivation
                : wxTargetNotificationResultForAnalysis::UnqualifiedDeactivation), "Owned active handlers explicitly refused");
            Check(target.ResourceForAnalysis()->node.get() == node && host.paths.size() == 1,
                "Unqualified notification cannot invoke a successful owned callback");
        }
        target.vfunc_0C(nullptr);
        Check(target.LastNotificationResultForAnalysis() == wxTargetNotificationResultForAnalysis::InvalidInput, "Null packet host refusal");
        target.InitializeResourceForAnalysis();
        Check(host.paths.size() == 2 && host.root->GetChildCountForAnalysis() == 1 && host.nodes[0]->referenceCount8 == 2,
            "Same loaded pointer avoids AddRef and actual same-parent Attach is a no-op");
        wxTargetManager::SetFactoryHostForAnalysis(&host);
        auto factory = wxTargetManager::StaticRTTI().factory();
        Check(factory && factory->IsExactly(wxTargetManager::ClassID), "Typed factory host");
        wxTargetManager::SetFactoryHostForAnalysis(nullptr); factory.reset();
        bool refused = false;
        try { (void)wxTargetManager::StaticRTTI().factory(); } catch (const std::logic_error&) { refused = true; }
        Check(refused, "Unbound factory refuses before construction");
    }
    void OwnershipGuards()
    {
        {
            Host host("lifetime"); auto retained = host.weakNodes[0];
            { wxTargetManager target(host); target.ResetDefaultLifetimeForAnalysis(); }
            Check(retained.expired() && !host.nodes[0]->node && host.nodes[0]->referenceCount8 == 0,
                "Plain source Node actually destructs when graph/manager edges release the final shadow references");
        }
        {
            Host host("lifetime"); host.nodes[0]->referenceCount8 = 65534;
            bool refused = false;
            try { wxTargetManager target(host); } catch (const std::domain_error&) { refused = true; }
            Check(refused && host.root->GetChildCountForAnalysis() == 0 && host.nodes[0]->referenceCount8 == 65534,
                "Unsupported intrusive AddRef wrap is refused before ownership/transform mutation");
            Check(host.nodes[0]->node->GetScaleForAnalysis() == spNode::Vector3{1,1,1}, "Refcount guard precedes local transform changes");
        }
    }
}
int main(int argc, char** argv)
{
    try
    {
        if (argc == 3 && std::string(argv[1]) == "--replay") { Replay(argv[2]); std::cout << '\n'; return 0; }
        if (argc == 6 && (std::string(argv[1]) == "--registry" || std::string(argv[1]) == "--registry-mutation"))
        {
            RegistryReplay(static_cast<std::uint8_t>(std::stoul(argv[2])),
                {static_cast<unsigned>(std::stoul(argv[3])),static_cast<unsigned>(std::stoul(argv[4])),static_cast<unsigned>(std::stoul(argv[5]))},
                std::string(argv[1]) == "--registry-mutation");
            std::cout << '\n'; return 0;
        }
        Guards(); OwnershipGuards(); RegistryOwnership();
        for (const auto* mode : {"lifetime", "reinit", "reset", "clone", "copy", "notify-ignore", "retained"})
        {
            std::ostringstream captured; auto* output = std::cout.rdbuf(captured.rdbuf());
            try { Replay(mode); } catch (...) { std::cout.rdbuf(output); throw; }
            std::cout.rdbuf(output);
        }
        std::cout << "PASS wxTargetManager " << checks << " checks\n"; return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
