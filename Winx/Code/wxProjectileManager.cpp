#include "wxProjectileManager.h"

#include <cstring>
#include <exception>
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;

    namespace
    {
        wxProjectileManagerHost* factoryHost = nullptr;
        std::unique_ptr<spBaseObject> CreateProjectileManager()
        {
            if (!factoryHost) throw std::logic_error("wxProjectileManager requires a factory host");
            return std::make_unique<wxProjectileManager>(*factoryHost);
        }
        const spRTTIRecord record{wxProjectileManager::ClassID, wxEntity::ClassID,
            "wxProjectileManager", &wxEntity::StaticRTTI(),
            &CreateProjectileManager, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);

        constexpr std::array<std::size_t, 14> copyOffsets{
            0x128, 0x124, 0x134, 0x138, 0x13C, 0x140, 0x144,
            0x148, 0x150, 0x154, 0x158, 0x160, 0x164, 0x168};
        void PutWord(wxProjectileManager::OwnBytesForAnalysis& bytes,
            std::size_t offset, std::uint32_t value) noexcept
        {
            std::memcpy(bytes.data() + offset - 0x124, &value, sizeof(value));
        }
        std::uint32_t GetWord(const wxProjectileManager::OwnBytesForAnalysis& bytes,
            std::size_t offset) noexcept
        {
            std::uint32_t value;
            std::memcpy(&value, bytes.data() + offset - 0x124, sizeof(value));
            return value;
        }
    }

    wxProjectileManager::wxProjectileManager(wxProjectileManagerHost& host)
        : wxEntity(host), host_(host)
    {
        // Values visible after original PC factory/constructor. They are
        // stored in an offset view, independently of the native object ABI.
        PutWord(ownBytes_, 0x138, 0x3F000000);
        PutWord(ownBytes_, 0x13C, 0x00000BB8);
        PutWord(ownBytes_, 0x140, 0x19);
        PutWord(ownBytes_, 0x144, 0x0F);
        PutWord(ownBytes_, 0x148, 0x05);
        PutWord(ownBytes_, 0x14C, 0xFFFFFFE7);
        PutWord(ownBytes_, 0x150, 0x3E4CCCCD);
        PutWord(ownBytes_, 0x154, 0x3E99999A);
        PutWord(ownBytes_, 0x158, 0x3F000000);
        PutWord(ownBytes_, 0x15C, 0x3F59999A);
        PutWord(ownBytes_, 0x160, 0x40400000);
        PutWord(ownBytes_, 0x164, 0x40000000);
        PutWord(ownBytes_, 0x168, 0x3F800000);
        PutWord(ownBytes_, 0x16C, 0x3F800000);
    }

    wxProjectileManager::~wxProjectileManager()
    {
        // PC 505E2F / PS2 2C6654: external manager call before the pool.
        host_.BeforePoolTeardownForAnalysis(*this);
        PutWord(ownBytes_, 0x130, 0);
        // PC 505E50 / PS2 2C6678: row-major traversal of all 20 entries.
        for (auto& group : pool_)
            for (auto*& record : group)
                if (record)
                {
                    record->payload = nullptr;
                    host_.FreePoolRecordForAnalysis(*record);
                    record = nullptr;
                }
    }

    void wxProjectileManager::SetFactoryHostForAnalysis(wxProjectileManagerHost* host) noexcept
    { factoryHost = host; }

    const spRTTIRecord& wxProjectileManager::StaticRTTI() noexcept
    { (void)registered; return record; }

    const spRTTIRecord& wxProjectileManager::vfunc_18() const noexcept
    { return record; }

    std::unique_ptr<spBaseObject> wxProjectileManager::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxProjectileManager>(host_);
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }

    bool wxProjectileManager::vfunc_14(spBaseObject& destination,
        spCloneManager& manager) const
    {
        auto* target = dynamic_cast<wxProjectileManager*>(&destination);
        if (!target || !wxEntity::vfunc_14(*target, manager)) return false;
        for (const auto offset : copyOffsets)
            std::memcpy(target->ownBytes_.data() + offset - 0x124,
                ownBytes_.data() + offset - 0x124, sizeof(std::uint32_t));
        host_.CompleteCopyForAnalysis(*this, *target, manager, ClassID);
        return true;
    }

    void wxProjectileManager::vfunc_0C(const void* notification) noexcept
    {
        if (!notification) return;
        const auto& message = *static_cast<const wxProjectileManagerMessageForAnalysis*>(notification);
        switch (message.code)
        {
        case 0x1C: host_.SetupProjectileManagerForAnalysis(*this); break;
        case 0x1E: (void)TickForAnalysis(); break;
        case 0x273F:
            if (const auto slot = FindFreeSlotForAnalysis(message.group))
            {
                auto* const record = host_.AllocatePoolRecordForAnalysis(*this);
                if (!record) std::terminate();
                record->deadline = 0;
                record->active = 0;
                record->payload = message.payload;
                const auto scaleBits = GetWord(ownBytes_, 0x160 + 4*message.group);
                float scale;
                std::memcpy(&scale, &scaleBits, sizeof(scale));
                host_.InitializePoolPayloadForAnalysis(message.payload, scale);
                pool_[message.group][*slot] = record;
            }
            break;
        default: break;
        }
    }

    std::optional<std::size_t> wxProjectileManager::FindFreeSlotForAnalysis(
        const std::size_t group) const noexcept
    {
        if (group >= pool_.size()) return std::nullopt;
        for (std::size_t slot = 0; slot < 4; ++slot)
            if (!pool_[group][slot]) return slot;
        return std::nullopt;
    }

    bool wxProjectileManager::TickForAnalysis() noexcept
    {
        if (host_.IsGamePausedForAnalysis()) return false;
        if (enabled_)
        {
            for (std::size_t group = 0; group < pool_.size(); ++group)
                for (std::size_t slot = 0; slot < pool_[group].size(); ++slot)
                {
                    auto* entry = pool_[group][slot];
                    if (!entry || !entry->active) continue;
                    // The original reads time for each active entry, after
                    // callbacks for preceding slots, rather than once per Tick.
                    if (entry->deadline < host_.GameTimeForAnalysis())
                    {
                        entry->active = 0;
                        entry->deadline = 0;
                        host_.DisablePoolPayloadForAnalysis(entry->payload, 0, 1);
                        host_.ResetPoolPayloadPositionForAnalysis(
                            pool_[group][slot]->payload);
                    }
                    else host_.UpdatePoolRecordForAnalysis(*this, group, slot);
                }
        }
        return true;
    }
}
