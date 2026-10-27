#include "Code/Sparkplug/spGameLevelInitialization.h"
#include "Code/Sparkplug/spGameLevel.h"
#include "Code/Sparkplug/spEntity.h"
#include "Code/Sparkplug/spGUIObject.h"
#include "Code/Sparkplug/spWidget.h"
#include "Code/Sparkplug/spCollisionInfo.h"
#include "Code/Sparkplug/spOBBBV.h"
#include "Code/SparkBase/spNotification.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace sparkplug::reconstruction;
namespace
{
    unsigned checks = 0, comparisons = 0, nativeCases = 0;
    void Check(bool value, const char* reason)
    { ++checks; if (!value) throw std::runtime_error(reason); }
    void Match(std::size_t actual, std::size_t expected, const char* reason)
    {
        ++comparisons;
        if (actual != expected) std::cerr << reason << " actual " << actual << " expected " << expected << '\n';
        Check(actual == expected, reason);
    }
    struct Host final : spGUIHost
    {
        std::map<void*, std::uint16_t> references;
        unsigned unresolved = 0;
        void AddToEngineManagerForAnalysis(spEntity&) override {}
        void DestroyEntityForAnalysis(spEntity& entity) noexcept override
        { if (auto* node = entity.GetReferenceForAnalysis()) --references[node]; }
        std::uint16_t GetReferenceCountForAnalysis(void* p) override { return references[p]; }
        void SetReferenceCountForAnalysis(void* p, std::uint16_t value) override { references[p] = value; }
        void DeleteReferenceForAnalysis(void*) override { ++unresolved; }
        void SubscribeGUINodeForAnalysis(spNode&, spGUIObject&) override {}
        void UnsubscribeGUINodeForAnalysis(spNode&, spGUIObject&) noexcept override {}
        void DestroyGUIForAnalysis(spGUIObject&) noexcept override {}
        bool IsGUINodeEnabledForAnalysis(const spNode& node) override { return node.IsEnabledForAnalysis(); }
        void SetGUINodeEnabledForAnalysis(spNode& node, std::uint8_t value, bool recursive) override
        { node.SetEnabledForAnalysis(value != 0, recursive); }
        void* GetGUINotificationTargetForAnalysis(const spGUIObject&) override { return nullptr; }
        std::uint32_t ReadGUIInputWord18ForAnalysis(std::uintptr_t) override { ++unresolved; return 0; }
        void DispatchNotificationForAnalysis(void*, const spNotificationForAnalysis&) override { ++unresolved; }
        void UnrecoveredGUINotificationForAnalysis(spGUIObject&, const void*, const char*) noexcept override { ++unresolved; }
    };
    struct Event final { unsigned role; std::array<std::uint32_t, 8> words; };
    struct Context final
    {
        std::map<unsigned, unsigned> retryUntil;
        unsigned flag = 1;
        bool mutateWords = false, highOnly = false, valid = true;
        std::vector<unsigned> predicates;
        std::vector<Event> events;
        Context() { predicates.reserve(4096); events.reserve(4096); }
        void Predicate(unsigned role, spClassID candidate)
        { predicates.push_back(role); valid = valid && candidate == spEntity::ClassID; }
        void Observe(unsigned role, const void* value)
        {
            auto& n = *static_cast<const spNotificationForAnalysis*>(value);
            events.push_back({role, {n.code, n.word04, n.word08, n.word0C, n.sender ? 1u : 0u,
                n.word14, static_cast<std::uint32_t>(n.payload), static_cast<std::uint32_t>(n.tail & 0xFF)}});
            auto& writable = const_cast<spNotificationForAnalysis&>(n);
            const auto found = retryUntil.find(role);
            const bool retry = found != retryUntil.end() && n.payload < found->second;
            if (highOnly) writable.tail = retry ? 0x80000000 : 0;
            else if (retry) writable.tail = (writable.tail & ~std::uintptr_t{0xFF}) | flag;
            if (mutateWords)
            {
                writable.code = 0x99; writable.word04 = 0x12345678; writable.word08 = 0xFFFFFFFF;
                writable.word0C = 0x87654321; writable.sender = reinterpret_cast<const void*>(std::uintptr_t{0x100});
                writable.word14 = 0x200; writable.payload = 0x300;
            }
        }
    };
    // These are observation fixtures, not invented game classes or retry
    // policies. They record the actual helper's virtual boundary and then run
    // the real shared Entity/GUIObject/Widget notification implementation.
    template<class Base> class ObservedReceiver final : public Base
    {
        Context& context_;
        unsigned role_;
    public:
        ObservedReceiver(Host& host, Context& context, unsigned role) : Base(host), context_(context), role_(role) {}
        bool vfunc_20(spClassID candidate) const noexcept override
        { context_.Predicate(role_, candidate); return Base::vfunc_20(candidate); }
        void vfunc_0C(const void* notification) noexcept override
        { context_.Observe(role_, notification); Base::vfunc_0C(notification); }
    };
    class ObservedNode final : public spNode
    {
        Context& context_;
        unsigned role_;
    public:
        ObservedNode(Context& context, unsigned role) : context_(context), role_(role) {}
        bool vfunc_20(spClassID candidate) const noexcept override
        { context_.Predicate(role_, candidate); return spNode::vfunc_20(candidate); }
    };
    struct Reader final
    {
        std::ifstream input;
        explicit Reader(const char* path) : input(path, std::ios::binary) { Check(bool(input), "Open GameLevel initialization fixture"); }
        std::uint32_t U32()
        { std::uint32_t value = 0; input.read(reinterpret_cast<char*>(&value), 4); Check(bool(input), "Initialization fixture word extent"); return value; }
        std::string Text()
        {
            const auto size = U32(); Check(size <= 4096, "Initialization label bound");
            std::string result(size, '\0'); if (size) input.read(result.data(), size); Check(bool(input), "Initialization label extent"); return result;
        }
        std::vector<unsigned> Words()
        { const auto size = U32(); Check(size <= 4096, "Initialization word-vector bound"); std::vector<unsigned> words(size); for (auto& word : words) word = U32(); return words; }
    };
    void Basics()
    {
        using Status = spGameLevelInitializationStatusForAnalysis;
        spGameLevel empty; auto result = InitializeGameLevelForAnalysis(empty);
        Check(result.status == Status::Completed && result.notificationsSent == 0 && result.passesCompleted == 0, "Empty established level completes");
        Host host; Context context; auto receiver = std::make_shared<ObservedReceiver<spEntity>>(host, context, 1);
        auto node = std::make_shared<ObservedNode>(context, 2); auto instance = std::make_unique<spTemplateInstance>();
        instance->SetAttachedObjectsForFixture({receiver, node, receiver}); spGameLevel level;
        Check(level.AddInstanceForAnalysis(std::move(instance)), "Existing level ownership input");
        Check(level.GetInstancesForAnalysis().front()->GetAttachedObjectsForAnalysis().size() == 3, "Borrowed existing graph retains repeated entries");
        spGameLevelInitializationLimitsForAnalysis limits; limits.maximumEntities = 1;
        result = InitializeGameLevelForAnalysis(level, limits);
        Check(result.status == Status::EntityLimit && result.entitiesCollected == 1 && result.notificationsSent == 0, "Entity cap is an analytical pre-dispatch refusal");
        limits = {}; limits.maximumAttachedObjects = 1; result = InitializeGameLevelForAnalysis(level, limits);
        Check(result.status == Status::AttachedObjectLimit && result.attachedObjectsVisited == 1 && result.notificationsSent == 0, "Attached traversal cap is explicit");
        context.retryUntil[1] = 100; limits = {}; limits.maximumPasses = 2; result = InitializeGameLevelForAnalysis(level, limits);
        Check(result.status == Status::PassLimit && result.passesCompleted == 2 && result.notificationsSent == 4, "Nonterminating policy has a bounded analytical prefix");
        auto invalid = std::make_unique<spTemplateInstance>(); invalid->SetAttachedObjectsForFixture({nullptr}); spGameLevel nullGraph;
        Check(nullGraph.AddInstanceForAnalysis(std::move(invalid)), "Explicit null-entry guard fixture"); result = InitializeGameLevelForAnalysis(nullGraph);
        Check(result.status == Status::InvalidInputGraph && result.notificationsSent == 0, "Null native dereference input is refused analytically");
        context.retryUntil.clear();
        result = InitializeTemplateInstanceForAnalysis(*level.GetInstancesForAnalysis().front());
        Check(result.status == Status::Completed && result.entitiesCollected == 2 && result.notificationsSent == 2,
            "Per-instance wrapper reuses the shared collector/retry over its own list");
    }
    void Replay(const char* path)
    {
        Reader input(path); const auto magic = input.U32(); const bool singleInstance = magic == 0x31494954;
        Check(magic == 0x314C4947 || singleInstance, "GIL1 or TII1 fixture");
        const auto count = input.U32(); Check(count <= 128, "Native initialization case bound");
        for (unsigned caseIndex = 0; caseIndex != count; ++caseIndex)
        {
            const auto label = input.Text();
            const auto instanceIndex = singleInstance ? input.U32() : 0;
            try
            {
                const auto groupCount = input.U32(); Check(groupCount <= 64, "Instance fixture bound");
                std::vector<std::vector<unsigned>> groups; for (unsigned i = 0; i != groupCount; ++i) groups.push_back(input.Words());
                const auto objectCount = input.U32(); Check(objectCount <= 64, "Attached object fixture bound"); std::map<unsigned, unsigned> kinds;
                for (unsigned i = 0; i != objectCount; ++i) { const auto role = input.U32(), kind = input.U32(); kinds.emplace(role, kind); }
                Host host; Context context; const auto retryCount = input.U32(); Check(retryCount <= objectCount, "Retry policy fixture bound");
                for (unsigned i = 0; i != retryCount; ++i) { const auto role = input.U32(), until = input.U32(); context.retryUntil.emplace(role, until); }
                context.flag = input.U32(); context.mutateWords = input.U32() != 0; context.highOnly = input.U32() != 0;
                std::map<unsigned, std::shared_ptr<spNode>> roots; std::map<unsigned, std::shared_ptr<spCollisionInfo>> collisions;
                // Objects release their raw GUI root binding before the
                // external root owners leave this established input scope.
                std::map<unsigned, std::shared_ptr<spBaseObject>> objects;
                for (auto [role, kind] : kinds)
                {
                    std::shared_ptr<spBaseObject> object;
                    if (kind == 1) object = std::make_shared<ObservedReceiver<spEntity>>(host, context, role);
                    else if (kind == 2) object = std::make_shared<ObservedNode>(context, role);
                    else if (kind == 3) object = std::make_shared<ObservedReceiver<spGUIObject>>(host, context, role);
                    else if (kind == 4) object = std::make_shared<ObservedReceiver<spWidget>>(host, context, role);
                    else throw std::logic_error("Unknown original receiver fixture kind");
                    if (kind >= 3)
                    {
                        auto root = std::make_shared<spNode>(); root->SetName("GUICollision");
                        auto info = std::make_shared<spCollisionInfo>(); info->SetPrimitiveForAnalysis(std::make_shared<spOBBBV>());
                        info->SetGroupForAnalysis(0x100+role); Check(root->AttachCollisionForAnalysis(info), "Actual shared Node collision attachment");
                        host.references[root.get()] = 4; static_cast<spGUIObject*>(object.get())->SetReferenceForAnalysis(root.get());
                        // The native fixture already has a bound GUI root and
                        // explicit group values. Binding is not under study.
                        info->SetGroupForAnalysis(0x100+role); roots[role] = std::move(root); collisions[role] = std::move(info);
                    }
                    objects.emplace(role, std::move(object));
                }
                spGameLevel level; std::vector<spTemplateInstance*> instances;
                for (const auto& group : groups)
                {
                    auto instance = std::make_unique<spTemplateInstance>(); std::list<std::shared_ptr<spBaseObject>> attached;
                    for (auto role : group) attached.push_back(objects.at(role));
                    instance->SetAttachedObjectsForFixture(std::move(attached)); instances.push_back(instance.get());
                    Check(level.AddInstanceForAnalysis(std::move(instance)), "Established instance input order");
                }
                std::map<unsigned, long> ownerCounts; for (const auto& [role, object] : objects) ownerCounts[role] = object.use_count();
                const auto result = singleInstance ? InitializeTemplateInstanceForAnalysis(*instances.at(instanceIndex)) : InitializeGameLevelForAnalysis(level);
                Check(result.status == spGameLevelInitializationStatusForAnalysis::Completed && context.valid && host.unresolved == 0, "Actual shared initialization completed");
                const auto collection = input.Words(), predicates = input.Words();
                Match(result.entitiesCollected, collection.size(), "Original selected Entity count"); Match(result.attachedObjectsVisited, predicates.size(), "Original visited object count");
                Match(context.predicates.size(), predicates.size(), "Original actual IsKindOf call count");
                for (unsigned i = 0; i != predicates.size(); ++i) Match(context.predicates[i], predicates[i], "Original actual IsKindOf list order");
                std::vector<unsigned> firstPass; for (const auto& event : context.events) if (event.words[6] == 0) firstPass.push_back(event.role);
                Match(firstPass.size(), collection.size(), "Original borrowed collection preserves duplicate entries");
                for (unsigned i = 0; i != collection.size(); ++i) Match(firstPass[i], collection[i], "Original borrowed collection order");
                const auto events = input.U32(); Match(context.events.size(), events, "Original retry callback count"); Match(result.notificationsSent, events, "Original notifications sent");
                unsigned passes = 0;
                for (unsigned i = 0; i != events; ++i)
                {
                    Match(context.events[i].role, input.U32(), "Original retry compaction order");
                    for (auto word : context.events[i].words) Match(word, input.U32(), "Original fresh notification word");
                    passes = std::max(passes, context.events[i].words[6]+1);
                }
                Match(result.passesCompleted, passes, "Original zero-based retry pass count");
                const auto groupValues = input.U32(); Match(collisions.size(), groupValues, "Original actual GUI root count");
                for (unsigned i = 0; i != groupValues; ++i) { const auto role = input.U32(), value = input.U32(); Match(collisions.at(role)->GetGroupForAnalysis(), value, "Original actual GUI1C group effect"); }
                Check(input.U32() == 1 && input.U32() == 1 && input.U32() == 1, "Original input guards and temporary cleanup observations");
                const auto nativeReferencesUnchanged = input.U32(); bool sourceReferencesUnchanged = true;
                for (const auto& [role, object] : objects) sourceReferencesUnchanged = sourceReferencesUnchanged && ownerCounts[role] == object.use_count();
                Match(sourceReferencesUnchanged, nativeReferencesUnchanged, "Borrowed operation leaves ownership unchanged (portable and native representations remain distinct)");
                ++nativeCases;
            }
            catch (const std::exception& error) { throw std::runtime_error(label + ": " + error.what()); }
        }
        Check(input.input.peek() == std::char_traits<char>::eof(), "Complete initialization fixture read");
    }
}
int main(int argc, char** argv)
{
    try
    {
        Basics(); for (int i = 1; i < argc; ++i) Replay(argv[i]);
        std::cout << "GameLevelInitialization checks=" << checks << " nativeCases=" << nativeCases << " scalarComparisons=" << comparisons << '\n'; return 0;
    }
    catch (const std::exception& error) { std::cerr << "GameLevelInitialization failure: " << error.what() << '\n'; return 1; }
}
