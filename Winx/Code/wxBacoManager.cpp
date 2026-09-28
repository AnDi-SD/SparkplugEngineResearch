#include "wxBacoManager.h"

namespace winx::reconstruction
{
    using namespace sparkplug::reconstruction;
    namespace
    {
        const spRTTIRecord spEntityRecord{0x22875AA1, spNamedObject::ClassID,
            "spEntity", &spNamedObject::StaticRTTI(), nullptr, nullptr};
        const spRTTIRecord wxEntityRecord{0x796A1869, 0x22875AA1,
            "wxEntity", &spEntityRecord, nullptr, nullptr};
        std::unique_ptr<spBaseObject> CreateBacoManager()
        {
            return std::make_unique<wxBacoManager>();
        }
        const spRTTIRecord record{wxBacoManager::ClassID, 0x796A1869,
            "wxBacoManager", &wxEntityRecord, &CreateBacoManager, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);

        constexpr wxBacoManager::Position firstPoint{-4097.0f, 950.0f, -1111.0f};
        constexpr wxBacoManager::Position secondPoint{-2722.0f, 950.0f, -736.0f};
        bool WithinTrigger(const wxBacoManager::Position& player,
            const wxBacoManager::Position& point) noexcept
        {
            const double x = double(player[0]) - point[0];
            const double y = double(player[1]) - point[1];
            const double z = double(player[2]) - point[2];
            return x*x + y*y + z*z < 160000.0;
        }
    }

    const spRTTIRecord& wxBacoManager::StaticRTTI() noexcept
    {
        (void)registered;
        return record;
    }
    const spRTTIRecord& wxBacoManager::vfunc_18() const noexcept { return record; }

    std::unique_ptr<spBaseObject> wxBacoManager::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<wxBacoManager>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool wxBacoManager::vfunc_14(spBaseObject& destination, spCloneManager&) const
    {
        auto* other = dynamic_cast<wxBacoManager*>(&destination);
        if (!other || sceneSetup_ || !members_.empty() || state_.nextProximityCheck
            || state_.reachedFirst || state_.reachedSecond
            || state_.waveFirst || state_.waveSecond
            || state_.firstCount || state_.secondCount) return false;
        for (auto deadline : state_.deadlines)
            if (deadline) return false;
        CopyNameToForAnalysis(*other);
        return true;
    }

    void wxBacoManager::vfunc_0C(const void* notification) noexcept
    {
        if (!notification) return;
        const auto& message = *static_cast<const wxBacoManagerMessageForAnalysis*>(notification);
        switch (message.code)
        {
        case 0x1C: SetupForAnalysis(); break;
        case 0x1E: (void)UpdateForAnalysis(); break;
        case 0x273D: members_.push_back(message.payload); break;
        default: break;
        }
    }

    void wxBacoManager::SetupForAnalysis() noexcept
    {
        if (host_)
        {
            host_->SetupSceneForAnalysis(*this);
            state_.nextProximityCheck = host_->GetTickForAnalysis() + 500;
            sceneSetup_ = true;
        }
    }

    void wxBacoManager::CheckProximityForAnalysis(const Position& player) noexcept
    {
        if (!host_) return;
        if (!state_.reachedFirst && WithinTrigger(player, firstPoint))
        {
            state_.reachedFirst = state_.waveFirst = true;
            for (std::size_t i = 0; i < 3; ++i)
            {
                const auto tick = host_->GetTickForAnalysis();
                const auto random = host_->NextRandomForAnalysis();
                state_.deadlines[i] = tick + 3000 + random % 2001;
            }
        }
        if (!state_.reachedSecond && WithinTrigger(player, secondPoint))
        {
            state_.reachedSecond = state_.waveSecond = true;
            state_.secondCount = static_cast<std::uint32_t>(members_.size() / 2);
            for (std::size_t i = 3; i < 6; ++i)
            {
                const auto tick = host_->GetTickForAnalysis();
                const auto random = host_->NextRandomForAnalysis();
                state_.deadlines[i] = tick + 1000 + random % 2001;
            }
        }
        if (!state_.reachedFirst || !state_.reachedSecond)
            state_.nextProximityCheck = host_->GetTickForAnalysis() + 500;
    }

    bool wxBacoManager::UpdateForAnalysis() noexcept
    {
        if (!host_) return false;
        if ((!state_.reachedFirst || !state_.reachedSecond)
            && host_->GetTickForAnalysis() > state_.nextProximityCheck)
            CheckProximityForAnalysis(host_->GetPlayerPositionForAnalysis());
        if (state_.waveFirst) ProcessWaveForAnalysis(1);
        if (state_.waveSecond) ProcessWaveForAnalysis(2);
        return true;
    }

    void wxBacoManager::ProcessWaveForAnalysis(const std::uint32_t wave) noexcept
    {
        const auto half = static_cast<std::uint32_t>(members_.size() / 2);
        auto& count = wave == 1 ? state_.firstCount : state_.secondCount;
        auto& active = wave == 1 ? state_.waveFirst : state_.waveSecond;
        const std::size_t start = wave == 1 ? 0 : 3;
        const std::uint32_t limit = wave == 1 ? half
            : static_cast<std::uint32_t>(members_.size());
        for (std::size_t slot = start; slot < start + 3; ++slot)
        {
            if (state_.deadlines[slot] >= host_->GetTickForAnalysis()) continue;
            if (count >= limit)
            {
                active = false;
                continue;
            }
            // 573100 consumes the member at the current counter, then advances
            // it and samples a fresh independent RNG value for this deadline.
            host_->SpawnMemberForAnalysis(*this, members_[count],
                static_cast<std::uint32_t>(slot));
            ++count;
            const auto tick = host_->GetTickForAnalysis();
            const auto random = host_->NextRandomForAnalysis();
            state_.deadlines[slot] = tick + 1000 + random % 2001;
        }
    }
}
