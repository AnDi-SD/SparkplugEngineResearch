#include "wxEntity.h"

#include <cstring>
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;

    namespace
    {
        wxEntityHost* factoryHost = nullptr;
        std::unique_ptr<spBaseObject> CreateEntity()
        {
            if (!factoryHost) throw std::logic_error("wxEntity requires a factory host");
            return std::make_unique<wxEntity>(*factoryHost);
        }
        const spRTTIRecord record{wxEntity::ClassID, 0x22875AA1,
            "wxEntity", &spEntity::StaticRTTI(), &CreateEntity, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);

        std::uint32_t ReadWord(const wxEntity::OwnBytesForAnalysis& bytes,
            const std::size_t offset) noexcept
        {
            std::uint32_t value;
            std::memcpy(&value, bytes.data() + offset - 0x28, sizeof(value));
            return value;
        }
        void WriteWord(wxEntity::OwnBytesForAnalysis& bytes,
            const std::size_t offset, const std::uint32_t value) noexcept
        {
            std::memcpy(bytes.data() + offset - 0x28, &value, sizeof(value));
        }
    }

    wxEntity::wxEntity(wxEntityHost& host) : spEntity(host), host_(host)
    {
        ownBytes_[0x38 - 0x28] = 1;
        ownBytes_[0x39 - 0x28] = 1;
        WriteWord(ownBytes_, 0x3C, 0x4B095440);
        WriteWord(ownBytes_, 0x40, 0x00000500);
        WriteWord(ownBytes_, 0x44, 0x43160000);
        ownBytes_[0x88 - 0x28] = 1;
        for (std::size_t i = 0; i < 16; ++i)
        {
            WriteWord(ownBytes_, 0x8C + 4*i, 0x7F7FFFFF);
            WriteWord(ownBytes_, 0xCC + 4*i, 0x7F7FFFFF);
        }
        host_.MoveFromEngineToGameForAnalysis(*this);
    }

    wxEntity::~wxEntity()
    {
        host_.DestroyWxEntityForAnalysis(*this);
    }

    void wxEntity::SetFactoryHostForAnalysis(wxEntityHost* host) noexcept
    {
        factoryHost = host;
    }

    const spRTTIRecord& wxEntity::StaticRTTI() noexcept
    {
        (void)registered;
        return record;
    }

    const spRTTIRecord& wxEntity::vfunc_18() const noexcept
    {
        return record;
    }

    std::unique_ptr<spBaseObject> wxEntity::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxEntity>(host_);
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }

    bool wxEntity::vfunc_14(spBaseObject& destination, spCloneManager& manager) const
    {
        auto* target = dynamic_cast<wxEntity*>(&destination);
        if (!target || !spEntity::vfunc_14(*target, manager))
            return false;
        target->ownBytes_[0x28 - 0x28] = ownBytes_[0x28 - 0x28];
        target->ownBytes_[0x38 - 0x28] = ownBytes_[0x38 - 0x28];
        target->ownBytes_[0x39 - 0x28] = ownBytes_[0x39 - 0x28];
        WriteWord(target->ownBytes_, 0x3C, ReadWord(ownBytes_, 0x3C));
        host_.CompleteCopyForAnalysis(*this, *target, manager, ClassID);
        return true;
    }

    std::uint32_t wxEntity::ComputeFlagsForAnalysis() const
    {
        std::uint32_t flags = 0xFFFFFF00;
        if (ownBytes_[0x38 - 0x28])
        {
            bool all = true;
            const auto count = host_.CachedPredicateCountForAnalysis(*this);
            for (std::size_t i = 0; i < count; ++i)
                if (!host_.CachedPredicateForAnalysis(*this, i))
                { all = false; break; }
            if (all) flags |= 0x02;
        }
        if (ownBytes_[0x39 - 0x28] && host_.TimerByteForAnalysis(*this))
            flags |= 0x08;
        const auto thresholdBits = ReadWord(ownBytes_, 0x3C);
        if (thresholdBits != 0x7F7FFFFF)
        {
            float threshold;
            std::memcpy(&threshold, &thresholdBits, sizeof(threshold));
            if (host_.SquaredDistanceForAnalysis(*this) > threshold)
                flags |= 0x10;
        }
        return flags;
    }

    void wxEntity::UpdateFlagsForAnalysis()
    {
        host_.WriteComputedFlagsForAnalysis(*this, ComputeFlagsForAnalysis());
    }
}
