#include "wxProjectile.h"
#include "wxEntity.h"

#include <cstring>
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;

    namespace
    {
        wxProjectileHost* factoryHost = nullptr;
        std::unique_ptr<spBaseObject> CreateProjectile()
        {
            if (!factoryHost) throw std::logic_error("wxProjectile requires a factory host");
            return std::make_unique<wxProjectile>(*factoryHost);
        }
        const spRTTIRecord record{wxProjectile::ClassID, wxEntity::ClassID,
            "wxProjectile", &wxEntity::StaticRTTI(), &CreateProjectile, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        constexpr std::array<std::size_t, 8> middleWords{
            0x34,0x38,0x40,0x44,0x48,0x4C,0x50,0x54};

        std::uint32_t ReadWord(const wxProjectile::BytesForAnalysis& bytes,
            const std::size_t offset) noexcept
        {
            std::uint32_t value;
            std::memcpy(&value, bytes.data() + offset, 4);
            return value;
        }
        void PutWord(wxProjectile::BytesForAnalysis& bytes,
            const std::size_t offset, const std::uint32_t value) noexcept
        { std::memcpy(bytes.data() + offset, &value, 4); }
    }

    wxProjectile::wxProjectile(wxProjectileHost& host) : host_(host)
    {
        PutWord(bytes_, 0x38, 0x3F800000);
        PutWord(bytes_, 0xD8, 0xC4750000);
    }

    wxProjectile::~wxProjectile()
    {
        host_.DestroyProjectileForAnalysis(*this);
    }

    void wxProjectile::SetFactoryHostForAnalysis(wxProjectileHost* host) noexcept
    { factoryHost = host; }

    const spRTTIRecord& wxProjectile::StaticRTTI() noexcept
    { (void)registered; return record; }

    const spRTTIRecord& wxProjectile::vfunc_18() const noexcept
    { return record; }

    std::unique_ptr<spBaseObject> wxProjectile::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxProjectile>(host_);
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }

    void wxProjectile::TransferReferenceWordForAnalysis(
        const std::uint32_t oldToken, const std::uint32_t newToken) const
    {
        if (oldToken)
        {
            const auto count = static_cast<std::uint16_t>(
                host_.ReferenceCountForAnalysis(oldToken) - 1u);
            host_.SetReferenceCountForAnalysis(oldToken, count);
            if (!count) host_.DeleteReferenceForAnalysis(oldToken);
        }
        if (newToken)
            host_.SetReferenceCountForAnalysis(newToken,
                static_cast<std::uint16_t>(
                    host_.ReferenceCountForAnalysis(newToken) + 1u));
    }

    void wxProjectile::TransferReferenceForAnalysis(wxProjectile& target,
        const std::size_t offset) const
    {
        const auto newToken = ReadWord(bytes_, offset);
        TransferReferenceWordForAnalysis(ReadWord(target.bytes_, offset), newToken);
        PutWord(target.bytes_, offset, newToken);
    }

    bool wxProjectile::vfunc_14(spBaseObject& destination,
        spCloneManager& manager) const
    {
        auto* target = dynamic_cast<wxProjectile*>(&destination);
        if (!target || !spNamedObject::vfunc_14(*target, manager)) return false;
        PutWord(target->bytes_, 0x10, ReadWord(bytes_, 0x10));
        TransferReferenceForAnalysis(*target, 0x14);
        PutWord(target->bytes_, 0x28, ReadWord(bytes_, 0x28));
        target->bytes_[0x30] = bytes_[0x30];
        for (const auto offset : middleWords)
            PutWord(target->bytes_, offset, ReadWord(bytes_, offset));
        TransferReferenceForAnalysis(*target, 0x58);
        TransferReferenceForAnalysis(*target, 0x5C);
        PutWord(target->bytes_, 0x2C, ReadWord(bytes_, 0x2C));
        TransferReferenceForAnalysis(*target, 0xD4);
        PutWord(target->bytes_, 0x9C, ReadWord(bytes_, 0x9C));
        target->bytes_[0xA0] = bytes_[0xA0];
        if (target->actor_) host_.DestroyActorForAnalysis(target->actor_);
        target->actor_ = nullptr;
        target->actor_ = host_.CreateFreshActorForAnalysis();
        host_.CompleteCopyForAnalysis(*this, *target, ClassID);
        return true;
    }
}
