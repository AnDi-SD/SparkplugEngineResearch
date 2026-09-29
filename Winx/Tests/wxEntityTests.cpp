#include "Analysis/PC/wxEntityAbi.h"
#include "Analysis/PS2/wxEntityAbi.h"
#include "Code/wxEntity.h"

#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;

namespace
{
    void Require(bool condition, const char* message)
    {
        if (!condition) { std::cerr << message << '\n'; std::exit(EXIT_FAILURE); }
    }
    void PutWord(wxEntity& entity, std::size_t offset, std::uint32_t value)
    {
        std::memcpy(entity.GetOwnBytesForAnalysis().data() + offset - 0x28,
            &value, sizeof(value));
    }
    std::uint32_t Word(const wxEntity& entity, std::size_t offset)
    {
        std::uint32_t value;
        std::memcpy(&value, entity.GetOwnBytesForAnalysis().data() + offset - 0x28,
            sizeof(value));
        return value;
    }

    struct Host final : wxEntityHost
    {
        mutable std::vector<std::string> calls;
        std::vector<bool> predicates;
        bool timer = false;
        float distanceSquared = 0.0f;
        std::uint32_t writtenFlags = 0;
        void AddToEngineManagerForAnalysis(spEntity&) override
        { calls.emplace_back("engine-add"); }
        void MoveFromEngineToGameForAnalysis(wxEntity&) override
        { calls.emplace_back("engine-remove"); calls.emplace_back("game-add"); }
        void DestroyWxEntityForAnalysis(wxEntity&) noexcept override
        { calls.emplace_back("game-remove"); }
        void DestroyEntityForAnalysis(spEntity&) noexcept override
        { calls.emplace_back("engine-remove"); }
        std::uint16_t GetReferenceCountForAnalysis(void*) override { return 0; }
        void SetReferenceCountForAnalysis(void*, std::uint16_t) override {}
        void DeleteReferenceForAnalysis(void*) override {}
        void CompleteCopyForAnalysis(const wxEntity&, wxEntity&,
            spCloneManager&, std::uint32_t classID) const override
        {
            Require(classID == wxEntity::ClassID, "class hash for final copy hook");
            calls.emplace_back("copy-hook");
        }
        std::size_t CachedPredicateCountForAnalysis(const wxEntity&) const override
        { return predicates.size(); }
        bool CachedPredicateForAnalysis(const wxEntity&, std::size_t index) const override
        { calls.emplace_back("predicate" + std::to_string(index)); return predicates.at(index); }
        bool TimerByteForAnalysis(const wxEntity&) const override
        { calls.emplace_back("timer"); return timer; }
        float SquaredDistanceForAnalysis(const wxEntity&) const override
        { calls.emplace_back("distance"); return distanceSquared; }
        void WriteComputedFlagsForAnalysis(wxEntity&, std::uint32_t flags) override
        { writtenFlags = flags; calls.emplace_back("write"); }
    };
    struct RejectedEntity final : wxEntity
    {
        using wxEntity::wxEntity;
        const spRTTIRecord& vfunc_18() const noexcept override
        { return spNamedObject::StaticRTTI(); }
    };
}

int main()
{
    static_assert(sizeof(winx::evidence::pc::wxEntityLayout) == 0x124);
    static_assert(sizeof(winx::evidence::ps2::wxEntityLayout) == 0x130);
    Host host;
    wxEntity::SetFactoryHostForAnalysis(&host);
    auto factory = spRTTIManager::Instance().Create(wxEntity::ClassID);
    auto* entity = dynamic_cast<wxEntity*>(factory.get());
    Require(entity && entity->IsKindOf(0x22875AA1), "factory and native ancestry");
    Require(host.calls == std::vector<std::string>{
        "engine-add", "engine-remove", "game-add"}, "manager handoff order");
    const auto& bytes = entity->GetOwnBytesForAnalysis();
    Require(bytes[0] == 0 && bytes[0x38-0x28] == 1
        && bytes[0x39-0x28] == 1 && bytes[0x88-0x28] == 1,
        "constructor bytes");
    Require(Word(*entity, 0x3C) == 0x4B095440
        && Word(*entity, 0x40) == 0x500
        && Word(*entity, 0x44) == 0x43160000,
        "constructor words");
    for (std::size_t i = 0; i < 16; ++i)
        Require(Word(*entity, 0x8C + 4*i) == 0x7F7FFFFF
            && Word(*entity, 0xCC + 4*i) == 0x7F7FFFFF,
            "two sentinel blocks");

    wxEntity destination(host);
    spCloneManager manager;
    auto& sourceBytes = entity->GetOwnBytesForAnalysis();
    sourceBytes[0] = 0xA1;
    sourceBytes[0x38-0x28] = 0xB2;
    sourceBytes[0x39-0x28] = 0xC3;
    PutWord(*entity, 0x3C, 0x7FC12345);
    PutWord(*entity, 0x40, 0x12345678);
    const auto before = destination.GetOwnBytesForAnalysis();
    host.calls.clear();
    RejectedEntity rejected(host);
    const auto rejectedBefore = rejected.GetOwnBytesForAnalysis();
    host.calls.clear();
    Require(!entity->vfunc_14(rejected, manager)
        && rejected.GetOwnBytesForAnalysis() == rejectedBefore,
        "failed parent preserves own fields");
    host.calls.clear();
    Require(entity->vfunc_14(destination, manager), "successful parent copy");
    Require(host.calls == std::vector<std::string>{"copy-hook"},
        "copy hook follows own fields");
    for (std::size_t i = 0; i < sourceBytes.size(); ++i)
    {
        const auto offset = i + 0x28;
        const bool copied = offset == 0x28 || offset == 0x38 || offset == 0x39
            || (offset >= 0x3C && offset < 0x40);
        Require(destination.GetOwnBytesForAnalysis()[i]
            == (copied ? sourceBytes[i] : before[i]),
            "only four own fields transferred");
    }

    sourceBytes[0x38-0x28] = 1;
    sourceBytes[0x39-0x28] = 1;
    PutWord(*entity, 0x3C, 0x41C00000); // 24.0f
    host.predicates = {true, false, true};
    host.timer = true;
    host.distanceSquared = 25.0f;
    host.calls.clear();
    entity->UpdateFlagsForAnalysis();
    Require(host.writtenFlags == 0xFFFFFF18,
        "failed predicate, timer and distance flags");
    Require(host.calls == std::vector<std::string>{
        "predicate0", "predicate1", "timer", "distance", "write"},
        "predicate short circuit and write order");
    host.predicates.clear();
    PutWord(*entity, 0x3C, 0x41C80000); // exact 25.0f
    Require(entity->ComputeFlagsForAnalysis() == 0xFFFFFF0A,
        "empty predicate list and equal threshold");
    PutWord(*entity, 0x3C, 0x7F7FFFFF);
    host.calls.clear();
    Require(entity->ComputeFlagsForAnalysis() == 0xFFFFFF0A
        && host.calls == std::vector<std::string>{"timer"},
        "maximum-float threshold skips distance access");
    sourceBytes[0x38-0x28] = 0;
    sourceBytes[0x39-0x28] = 0;
    Require(entity->ComputeFlagsForAnalysis() == 0xFFFFFF00,
        "disabled predicate and timer gates");
    wxEntity::SetFactoryHostForAnalysis(nullptr);
    std::cout << "wxEntity checks passed\n";
}
