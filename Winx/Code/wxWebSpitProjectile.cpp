#include "wxWebSpitProjectile.h"

#include <cstring>
#include <stdexcept>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;

    namespace
    {
        wxProjectileHost* factoryHost = nullptr;
        std::unique_ptr<spBaseObject> CreateWebSpit()
        {
            if (!factoryHost) throw std::logic_error("wxWebSpitProjectile requires a factory host");
            return std::make_unique<wxWebSpitProjectile>(*factoryHost);
        }
        const spRTTIRecord record{wxWebSpitProjectile::ClassID,
            wxProjectile::ClassID, "wxWebSpitProjectile",
            &wxProjectile::StaticRTTI(), &CreateWebSpit, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);

        std::uint32_t ReadWord(const wxWebSpitProjectile::TailForAnalysis& bytes,
            const std::size_t offset) noexcept
        {
            std::uint32_t value;
            std::memcpy(&value, bytes.data() + offset - 0xEC, 4);
            return value;
        }
        void PutWord(wxWebSpitProjectile::TailForAnalysis& bytes,
            const std::size_t offset, const std::uint32_t value) noexcept
        { std::memcpy(bytes.data() + offset - 0xEC, &value, 4); }
    }

    wxWebSpitProjectile::wxWebSpitProjectile(wxProjectileHost& host)
        : wxProjectile(host) {}

    wxWebSpitProjectile::~wxWebSpitProjectile()
    {
        // PC 501170 releases +F4 before +F8, then calls the base destructor.
        for (const auto offset : {0xF4u, 0xF8u})
        {
            const auto oldToken = ReadWord(tail_, offset);
            if (!oldToken) continue;
            TransferReferenceWordForAnalysis(oldToken, 0);
            PutWord(tail_, offset, 0);
        }
    }

    void wxWebSpitProjectile::SetFactoryHostForAnalysis(wxProjectileHost* host) noexcept
    { factoryHost = host; }

    const spRTTIRecord& wxWebSpitProjectile::StaticRTTI() noexcept
    { (void)registered; return record; }

    const spRTTIRecord& wxWebSpitProjectile::vfunc_18() const noexcept
    { return record; }

    std::unique_ptr<spBaseObject> wxWebSpitProjectile::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxWebSpitProjectile>(GetHostForAnalysis());
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }

    bool wxWebSpitProjectile::vfunc_14(spBaseObject& destination,
        spCloneManager& manager) const
    {
        auto* target = dynamic_cast<wxWebSpitProjectile*>(&destination);
        if (!target || !spNamedObject::vfunc_14(*target, manager)) return false;
        target->tail_[0] = tail_[0]; // +EC byte only
        PutWord(target->tail_, 0xF0, ReadWord(tail_, 0xF0));
        for (const auto offset : {0xF4u, 0xF8u})
        {
            const auto next = ReadWord(tail_, offset);
            TransferReferenceWordForAnalysis(ReadWord(target->tail_, offset), next);
            PutWord(target->tail_, offset, next);
        }
        PutWord(target->tail_, 0xFC, ReadWord(tail_, 0xFC));
        GetHostForAnalysis().CompleteCopyForAnalysis(*this, *target, ClassID);
        return true;
    }
}
