#include "Analysis/PC/spEntityAbi.h"
#include "Analysis/PS2/spEntityAbi.h"
#include "Code/Sparkplug/spEntity.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace sparkplug::reconstruction;

namespace
{
    void Require(bool condition, const char* message)
    {
        if (!condition) { std::cerr << message << '\n'; std::exit(EXIT_FAILURE); }
    }
    struct Reference { std::uint16_t count = 0; bool deleted = false; };
    struct Host final : spEntityHost
    {
        std::vector<std::string> calls;
        spEntity* current = nullptr;
        void AddToEngineManagerForAnalysis(spEntity& entity) override
        { calls.emplace_back("add"); current = &entity; }
        void DestroyEntityForAnalysis(spEntity&) noexcept override
        { calls.emplace_back("destroy"); }
        std::uint16_t GetReferenceCountForAnalysis(void* object) override
        { return static_cast<Reference*>(object)->count; }
        void SetReferenceCountForAnalysis(void* object,
            std::uint16_t count) override
        {
            static_cast<Reference*>(object)->count = count;
            calls.emplace_back("count:" + std::to_string(count));
        }
        void DeleteReferenceForAnalysis(void* object) override
        {
            Require(current->GetReferenceForAnalysis() == object,
                "old reference remains visible during deleting destructor");
            static_cast<Reference*>(object)->deleted = true;
            calls.emplace_back("delete");
        }
    };
}

int main()
{
    static_assert(sizeof(sparkplug::evidence::pc::spEntityLayout) == 0x28);
    static_assert(sizeof(sparkplug::evidence::ps2::spEntityLayout) == 0x28);
    Host host;
    spEntity::SetFactoryHostForAnalysis(&host);
    auto factory = spRTTIManager::Instance().Create(spEntity::ClassID);
    auto* entity = dynamic_cast<spEntity*>(factory.get());
    Require(entity && entity->IsKindOf(spNamedObject::ClassID)
        && entity->GetField20ForAnalysis() == 3
        && entity->GetReferenceForAnalysis() == nullptr,
        "factory, ancestry and defaults");
    Require(host.calls == std::vector<std::string>{"add"}, "engine registration");
    auto clone = factory->Clone();
    Require(dynamic_cast<spEntity*>(clone.get()) != nullptr, "default clone");
    clone.reset();
    host.calls.clear();
    host.current = entity;

    Reference first{}, second{2}, wrap{0xFFFF};
    entity->SetReferenceForAnalysis(nullptr);
    Require(host.calls.empty(), "null to null is no-op");
    entity->SetReferenceForAnalysis(&first);
    Require(first.count == 1 && entity->GetReferenceForAnalysis() == &first,
        "increment before store");
    host.calls.clear();
    entity->SetReferenceForAnalysis(&first);
    Require(host.calls.empty(), "same reference is no-op");
    entity->SetReferenceForAnalysis(&second);
    Require(first.count == 0 && first.deleted && second.count == 3
        && entity->GetReferenceForAnalysis() == &second,
        "replace releases old then retains new");
    Require(host.calls == std::vector<std::string>{"count:0", "delete", "count:3"},
        "replacement order");
    host.calls.clear();
    entity->SetReferenceForAnalysis(&wrap);
    Require(second.count == 2 && wrap.count == 0 && !wrap.deleted,
        "16-bit increment wraps");
    host.calls.clear();
    entity->SetReferenceForAnalysis(nullptr);
    Require(wrap.count == 0xFFFF && !wrap.deleted
        && entity->GetReferenceForAnalysis() == nullptr,
        "16-bit decrement wraps");
    spEntity::SetFactoryHostForAnalysis(nullptr);
    std::cout << "spEntity checks passed\n";
}
