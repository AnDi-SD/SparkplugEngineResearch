#include "wxBacoProjectileManager.h"
#include "wxProjectileManager.h"

#include <cmath>

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        std::unique_ptr<spBaseObject> CreateBacoProjectileManager()
        {
            return std::make_unique<wxBacoProjectileManager>();
        }
        const spRTTIRecord record{wxBacoProjectileManager::ClassID,
            wxProjectileManager::ClassID,
            "wxBacoProjectileManager", &wxProjectileManager::StaticRTTI(),
            &CreateBacoProjectileManager, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }

    wxBacoProjectileManager::~wxBacoProjectileManager()
    {
        if (host_)
            for (void* projectile : projectiles_)
                if (projectile) host_->DestroyProjectileForAnalysis(projectile);
    }

    const spRTTIRecord& wxBacoProjectileManager::StaticRTTI() noexcept
    {
        (void)registered;
        return record;
    }
    const spRTTIRecord& wxBacoProjectileManager::vfunc_18() const noexcept { return record; }

    std::unique_ptr<spBaseObject> wxBacoProjectileManager::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxBacoProjectileManager>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxBacoProjectileManager::vfunc_14(spBaseObject& destination, spCloneManager&) const
    {
        auto* other = dynamic_cast<wxBacoProjectileManager*>(&destination);
        // Native empty clone is measured. Copying live pool ownership is not.
        if (!other || setup_ || projectiles_[0] || projectiles_[1]) return false;
        CopyNameToForAnalysis(*other);
        other->host_ = host_;
        return true;
    }

    void wxBacoProjectileManager::vfunc_0C(const void* notification) noexcept
    {
        if (!notification) return;
        const auto code = static_cast<const wxBacoProjectileManagerMessageForAnalysis*>(notification)->code;
        if (code == 0x1C) (void)SetupForAnalysis();
        else if (code == 0x1E) (void)UpdateForAnalysis();
    }

    bool wxBacoProjectileManager::SetupForAnalysis() noexcept
    {
        if (!host_ || !host_->HasSceneForAnalysis(*this) || setup_) return false;
        host_->AttachSceneForAnalysis(*this);
        for (std::size_t i = 0; i < emitters_.size(); ++i)
            emitters_[i] = host_->FindEmitterForAnalysis(i);
        for (std::size_t i = 0; i < projectiles_.size(); ++i)
            projectiles_[i] = host_->CreateProjectileForAnalysis(i);
        host_->BaseSetupForAnalysis(*this);
        setup_ = true;
        return true;
    }

    bool wxBacoProjectileManager::UpdateForAnalysis() noexcept
    {
        if (!host_) return false;
        // Native slot 17 walks exactly two pointers, in order, then returns 1.
        for (void* projectile : projectiles_)
            if (projectile) host_->UpdateProjectileForAnalysis(projectile);
        return true;
    }

    void wxBacoProjectileManager::NormalizeForAnalysis(Position& direction) noexcept
    {
        const float length = std::sqrt(direction[0]*direction[0]
            + direction[1]*direction[1] + direction[2]*direction[2]);
        if (length <= 0.001f)
        {
            direction = {};
            return;
        }
        for (float& component : direction) component /= length;
    }

    bool wxBacoProjectileManager::FireDirectionForAnalysis(Position direction) noexcept
    {
        if (!host_ || !host_->HasSceneForAnalysis(*this)) return false;
        NormalizeForAnalysis(direction);
        for (void* projectile : projectiles_)
        {
            if (!projectile || host_->IsProjectileActiveForAnalysis(projectile)) continue;
            // Original 50FA20 dereferences the first emitter after selecting a
            // free projectile. Missing scene objects are an unresolved boundary.
            if (!emitters_[0]) return false;
            host_->LaunchProjectileForAnalysis(projectile,
                host_->GetEmitterPositionForAnalysis(emitters_[0]), direction);
            return true;
        }
        return false;
    }

    bool wxBacoProjectileManager::FireTowardPlayerForAnalysis() noexcept
    {
        if (!host_ || !emitters_[0]) return false;
        Position player{};
        if (!host_->TryGetPlayerPositionForAnalysis(player)) return false;
        const Position emitter = host_->GetEmitterPositionForAnalysis(emitters_[0]);
        Position direction{player[0] - emitter[0], player[1] - emitter[1],
            player[2] - emitter[2]};
        NormalizeForAnalysis(direction);
        direction[1] = 0.5f;
        NormalizeForAnalysis(direction);
        return FireDirectionForAnalysis(direction);
    }
}
