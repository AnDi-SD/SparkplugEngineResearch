#include "wxStingerProjectile.h"

#include <cstring>
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;

    namespace
    {
        wxProjectileHost* factoryHost = nullptr;
        std::unique_ptr<spBaseObject> CreateStinger()
        {
            if (!factoryHost) throw std::logic_error("wxStingerProjectile requires a factory host");
            return std::make_unique<wxStingerProjectile>(*factoryHost);
        }
        const spRTTIRecord record{wxStingerProjectile::ClassID,
            wxProjectile::ClassID, "wxStingerProjectile",
            &wxProjectile::StaticRTTI(), &CreateStinger, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);

        std::uint32_t ReadWord(const wxStingerProjectile::TailForAnalysis& bytes,
            const std::size_t offset) noexcept
        {
            std::uint32_t value;
            std::memcpy(&value, bytes.data() + offset - 0xEC, 4);
            return value;
        }
        void PutWord(wxStingerProjectile::TailForAnalysis& bytes,
            const std::size_t offset, const std::uint32_t value) noexcept
        { std::memcpy(bytes.data() + offset - 0xEC, &value, 4); }
    }

    wxStingerProjectile::wxStingerProjectile(wxProjectileHost& host)
        : wxProjectile(host) {}

    wxStingerProjectile::~wxStingerProjectile()
    {
        // PC 501B60 releases +F0, then +EC before the base destructor.
        for (const auto offset : {0xF0u, 0xECu})
        {
            const auto oldToken = ReadWord(tail_, offset);
            if (!oldToken) continue;
            TransferReferenceWordForAnalysis(oldToken, 0);
            PutWord(tail_, offset, 0);
        }
    }

    void wxStingerProjectile::SetFactoryHostForAnalysis(wxProjectileHost* host) noexcept
    { factoryHost = host; }

    const spRTTIRecord& wxStingerProjectile::StaticRTTI() noexcept
    { (void)registered; return record; }

    const spRTTIRecord& wxStingerProjectile::vfunc_18() const noexcept
    { return record; }

    std::unique_ptr<spBaseObject> wxStingerProjectile::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxStingerProjectile>(GetHostForAnalysis());
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }

    bool wxStingerProjectile::vfunc_14(spBaseObject& destination,
        spCloneManager& manager) const
    {
        auto* target = dynamic_cast<wxStingerProjectile*>(&destination);
        // Both native versions call the named-object Copy directly here.
        if (!target || !spNamedObject::vfunc_14(*target, manager)) return false;
        for (const auto offset : {0xECu, 0xF0u})
        {
            const auto next = ReadWord(tail_, offset);
            TransferReferenceWordForAnalysis(ReadWord(target->tail_, offset), next);
            PutWord(target->tail_, offset, next);
        }
        PutWord(target->tail_, 0xF4, ReadWord(tail_, 0xF4));
        GetHostForAnalysis().CompleteCopyForAnalysis(*this, *target, ClassID);
        return true;
    }
}
