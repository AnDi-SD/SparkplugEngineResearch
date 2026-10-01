#include "Analysis/PC/wxProjectileManagerAbi.h"
#include "Analysis/PS2/wxProjectileManagerAbi.h"
#include "Code/wxProjectileManager.h"

#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <memory>
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

    void PutWord(wxProjectileManager& object, std::size_t offset, std::uint32_t value)
    {
        std::memcpy(object.GetOwnBytesForAnalysis().data() + offset - 0x124,
            &value, sizeof(value));
    }

    struct Host final : wxProjectileManagerHost
    {
        mutable std::vector<std::string> calls;
        bool paused = false;
        std::uint32_t now = 100;
        bool changeTimeOnDisable = false;
        void* registeredPayload = nullptr;
        std::vector<std::unique_ptr<wxProjectileManagerRecordForAnalysis>> records;
        void AddToEngineManagerForAnalysis(spEntity&) override
        { calls.emplace_back("engine-add"); }
        void MoveFromEngineToGameForAnalysis(wxEntity&) override
        { calls.emplace_back("game-move"); }
        void DestroyWxEntityForAnalysis(wxEntity&) noexcept override
        { calls.emplace_back("game-remove"); }
        void DestroyEntityForAnalysis(spEntity&) noexcept override
        { calls.emplace_back("engine-remove"); }
        std::uint16_t GetReferenceCountForAnalysis(void*) override { return 0; }
        void SetReferenceCountForAnalysis(void*, std::uint16_t) override {}
        void DeleteReferenceForAnalysis(void*) override {}
        void CompleteCopyForAnalysis(const wxEntity&, wxEntity&,
            spCloneManager&, std::uint32_t classID) const override
        { calls.emplace_back(classID == wxEntity::ClassID ? "entity-copy" :
            classID == wxProjectileManager::ClassID ? "manager-copy" : "wrong-copy"); }
        std::size_t CachedPredicateCountForAnalysis(const wxEntity&) const override
        { return 0; }
        bool CachedPredicateForAnalysis(const wxEntity&, std::size_t) const override
        { return true; }
        bool TimerByteForAnalysis(const wxEntity&) const override { return false; }
        float SquaredDistanceForAnalysis(const wxEntity&) const override { return 0; }
        void WriteComputedFlagsForAnalysis(wxEntity&, std::uint32_t) override {}
        bool IsGamePausedForAnalysis() const noexcept override
        { calls.emplace_back("pause"); return paused; }
        std::uint32_t GameTimeForAnalysis() const noexcept override
        { calls.emplace_back("time"); return now; }
        void BeforePoolTeardownForAnalysis(wxProjectileManager&) noexcept override
        { calls.emplace_back("before-pool-teardown"); }
        void SetupProjectileManagerForAnalysis(wxProjectileManager&) noexcept override
        { calls.emplace_back("setup"); }
        wxProjectileManagerRecordForAnalysis* AllocatePoolRecordForAnalysis(
            wxProjectileManager&) noexcept override
        {
            calls.emplace_back("allocate");
            records.push_back(std::make_unique<wxProjectileManagerRecordForAnalysis>());
            return records.back().get();
        }
        void InitializePoolPayloadForAnalysis(void* payload,
            float scale) noexcept override
        {
            Require(scale == 1.0f, "group-three scale");
            calls.emplace_back("initialize");
            registeredPayload = payload;
        }
        void FreePoolRecordForAnalysis(
            wxProjectileManagerRecordForAnalysis& record) noexcept override
        {
            Require(!record.payload, "record payload cleared before free");
            calls.emplace_back("free");
            const auto found = std::find_if(records.begin(), records.end(),
                [&record](const auto& owned) { return owned.get() == &record; });
            Require(found != records.end(), "free allocated record");
            records.erase(found);
        }
        void UpdatePoolRecordForAnalysis(wxProjectileManager&,
            std::size_t group, std::size_t slot) noexcept override
        { calls.emplace_back("update" + std::to_string(group) + ":" +
            std::to_string(slot)); }
        void DisablePoolPayloadForAnalysis(void* payload,
            std::uint32_t enabled, std::uint32_t immediate) noexcept override
        {
            Require(payload == registeredPayload && enabled == 0 && immediate == 1,
                "expired payload callback args");
            calls.emplace_back("disable");
            if (changeTimeOnDisable) now = 200;
        }
        void ResetPoolPayloadPositionForAnalysis(void* payload) noexcept override
        {
            Require(payload == registeredPayload, "reset reloaded payload");
            calls.emplace_back("reset-position");
        }
    };
}

int main()
{
    static_assert(sizeof(winx::evidence::pc::wxProjectileManagerLayout) == 0x1D0);
    static_assert(sizeof(winx::evidence::ps2::wxProjectileManagerLayout) == 0x1E0);
    Host host;
    wxProjectileManager::SetFactoryHostForAnalysis(&host);
    auto created = spRTTIManager::Instance().Create(wxProjectileManager::ClassID);
    auto* object = dynamic_cast<wxProjectileManager*>(created.get());
    Require(object && object->IsKindOf(wxEntity::ClassID)
        && object->IsKindOf(spEntity::ClassID), "factory and physical ancestry");
    Require(host.calls == std::vector<std::string>{"engine-add", "game-move"},
        "native manager handoff");
    const auto& defaults = object->GetOwnBytesForAnalysis();
    std::uint32_t word = 0;
    std::memcpy(&word, defaults.data() + 0x13C - 0x124, 4);
    Require(word == 3000 && !object->GetEnabledForAnalysis(),
        "constructor defaults");

    wxProjectileManager destination(host);
    auto before = destination.GetOwnBytesForAnalysis();
    constexpr std::size_t offsets[]{0x128,0x124,0x134,0x138,0x13C,0x140,0x144,
        0x148,0x150,0x154,0x158,0x160,0x164,0x168};
    for (std::size_t i = 0; i < 14; ++i)
        PutWord(*object, offsets[i], static_cast<std::uint32_t>((i+1)*0x1111111));
    PutWord(*object, 0x14C, 0x7FC12345);
    PutWord(*object, 0x15C, 0x87654321);
    spCloneManager manager;
    host.calls.clear();
    Require(object->vfunc_14(destination, manager), "copy succeeds");
    Require(host.calls == std::vector<std::string>{"entity-copy", "manager-copy"},
        "parent and own copy callbacks");
    for (std::size_t i = 0; i < before.size(); ++i)
    {
        bool copied = false;
        for (const auto offset : offsets)
            if (i + 0x124 >= offset && i + 0x124 < offset + 4) copied = true;
        Require(destination.GetOwnBytesForAnalysis()[i] ==
            (copied ? object->GetOwnBytesForAnalysis()[i] : before[i]),
            "only fourteen own words copied");
    }

    wxProjectileManagerRecordForAnalysis occupied{};
    auto& pool = object->GetPoolForAnalysis();
    for (std::size_t slot = 0; slot < 5; ++slot) pool[3][slot] = &occupied;
    pool[3][1] = nullptr;
    Require(object->FindFreeSlotForAnalysis(3) == 1, "first of four free slots");
    pool[3][1] = &occupied;
    pool[3][4] = nullptr;
    Require(!object->FindFreeSlotForAnalysis(3), "fifth slot ignored on registration");
    Require(!object->FindFreeSlotForAnalysis(4), "invalid group has no portable slot");
    host.calls.clear();
    wxProjectileManagerMessageForAnalysis message{0x273F, 3, &occupied};
    object->vfunc_0C(&message);
    Require(host.calls.empty(), "full registration returns before allocation");
    pool[3][0] = nullptr;
    object->vfunc_0C(&message);
    Require(host.calls == std::vector<std::string>{"allocate", "initialize"}
        && host.registeredPayload == &occupied && pool[3][0]
        && pool[3][0]->payload == &occupied && !pool[3][0]->active
        && !pool[3][0]->deadline, "registration initializes and inserts record");
    auto* registered = pool[3][0];
    pool[3][0] = nullptr;
    registered->payload = nullptr;
    host.FreePoolRecordForAnalysis(*registered);
    host.calls.clear();
    message.code = 0x1C;
    object->vfunc_0C(&message);
    message.code = 0x1D;
    object->vfunc_0C(&message);
    Require(host.calls == std::vector<std::string>{"setup"}, "notify dispatch");

    wxProjectileManagerRecordForAnalysis expired{99, 1, &occupied};
    wxProjectileManagerRecordForAnalysis equal{100, 255, &occupied};
    wxProjectileManagerRecordForAnalysis inactive{2, 0, &occupied};
    pool = {};
    pool[0][0] = &inactive;
    pool[0][1] = &expired;
    pool[3][4] = &equal;
    object->SetEnabledForAnalysis(1);
    host.paused = true;
    host.calls.clear();
    Require(!object->TickForAnalysis() && host.calls == std::vector<std::string>{"pause"},
        "pause returns false without scan");
    host.paused = false;
    host.calls.clear();
    Require(object->TickForAnalysis(), "unpaused returns true");
    Require(expired.deadline == 0 && expired.active == 0
        && equal.deadline == 100 && equal.active == 255,
        "strict unsigned expiration and mutation");
    Require(host.calls == std::vector<std::string>{
        "pause", "time", "disable", "reset-position", "time", "update3:4"},
        "scan, per-entry time read and expiration callback order");
    pool = {};
    host.calls.clear();
    Require(object->TickForAnalysis()
        && host.calls == std::vector<std::string>{"pause"},
        "empty enabled pool avoids timer read");
    pool[0][0] = &inactive;
    host.calls.clear();
    Require(object->TickForAnalysis()
        && host.calls == std::vector<std::string>{"pause"},
        "inactive records avoid timer read");
    expired = {99, 1, &occupied};
    equal = {100, 1, &occupied};
    pool[0][1] = &expired;
    pool[3][4] = &equal;
    host.changeTimeOnDisable = true;
    host.calls.clear();
    Require(object->TickForAnalysis() && !equal.active && !equal.deadline,
        "later slot sees time changed during earlier callback");
    Require(host.calls == std::vector<std::string>{"pause", "time", "disable",
        "reset-position", "time", "disable", "reset-position"},
        "both expirations complete in row-major order");
    host.changeTimeOnDisable = false;
    object->SetEnabledForAnalysis(0);
    host.calls.clear();
    Require(object->TickForAnalysis()
        && host.calls == std::vector<std::string>{"pause"},
        "disabled returns true without timer read");
    pool = {};
    host.calls.clear();
    {
        wxProjectileManager disposable(host);
        wxProjectileManagerMessageForAnalysis ownedMessage{0x273F, 3, &occupied};
        disposable.vfunc_0C(&ownedMessage);
        Require(disposable.GetPoolForAnalysis()[3][0] != nullptr,
            "registration is visible before manager destruction");
        host.calls.clear();
    }
    Require(host.calls == std::vector<std::string>{
        "before-pool-teardown", "free", "game-remove", "engine-remove"},
        "own manager call precedes record free and inherited teardown");
    wxProjectileManager::SetFactoryHostForAnalysis(nullptr);
    std::cout << "wxProjectileManager checks passed\n";
}
