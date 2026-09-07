#include "spRenderTargetManager.h"

#include "spCubeRenderTarget.h"
#include "spMaterialRenderTargetTexture.h"

#include <algorithm>

namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord RenderTargetManagerRecord{
            spRenderTargetManager::ClassID,
            spCrossPlatform::ClassID,
            "spRenderTargetManager",
            &spCrossPlatform::StaticRTTI(),
            nullptr,
            nullptr,
        };

        const bool RenderTargetManagerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(RenderTargetManagerRecord);
    }

    spRenderTargetManager* spRenderTargetManager::instance_ = nullptr;

    spRenderTargetManager::spRenderTargetManager() noexcept
    {
        instance_ = this;
    }

    spRenderTargetManager::~spRenderTargetManager()
    {
        ordinaryTargets_.clear();
        cubeTargets_.clear();
        layerTargets_.clear();
        if (instance_ == this)
        {
            instance_ = nullptr;
        }
    }

    const spRTTIRecord& spRenderTargetManager::StaticRTTI() noexcept
    {
        (void)RenderTargetManagerRegistered;
        return RenderTargetManagerRecord;
    }

    spRenderTargetManager* spRenderTargetManager::GetInstance() noexcept
    {
        return instance_;
    }

    std::unique_ptr<spBaseObject> spRenderTargetManager::vfunc_10(
        spCloneManager&) const
    {
        return nullptr;
    }

    const spRTTIRecord& spRenderTargetManager::vfunc_18() const noexcept
    {
        return RenderTargetManagerRecord;
    }

    bool spRenderTargetManager::RegisterTargetForAnalysis(spRenderTarget& target)
    {
        auto& entries = target.IsKindOf(spCubeRenderTarget::ClassID)
            ? cubeTargets_
            : ordinaryTargets_;
        const auto found = std::find_if(entries.begin(), entries.end(),
            [&target](const Entry& entry) { return entry.target == &target; });
        if (found != entries.end())
        {
            found->active = true;
            return false;
        }
        entries.push_back(Entry{&target, true});
        return true;
    }

    bool spRenderTargetManager::DeactivateTargetForAnalysis(
        spRenderTarget& target) noexcept
    {
        auto deactivate = [&target](std::vector<Entry>& entries)
        {
            const auto found = std::find_if(entries.begin(), entries.end(),
                [&target](const Entry& entry) { return entry.target == &target; });
            if (found == entries.end())
            {
                return false;
            }
            found->active = false;
            found->target = nullptr;
            return true;
        };
        return deactivate(ordinaryTargets_) || deactivate(cubeTargets_);
    }

    bool spRenderTargetManager::RegisterLayerTargetForAnalysis(
        spMaterialRenderTargetTexture& target)
    {
        const auto found = std::find_if(layerTargets_.begin(), layerTargets_.end(),
            [&target](const LayerEntry& entry) { return entry.target == &target; });
        if (found != layerTargets_.end())
        {
            found->active = true;
            return false;
        }
        layerTargets_.push_back(LayerEntry{&target, true});
        return true;
    }

    bool spRenderTargetManager::DeactivateLayerTargetForAnalysis(
        spMaterialRenderTargetTexture& target) noexcept
    {
        const auto found = std::find_if(layerTargets_.begin(), layerTargets_.end(),
            [&target](const LayerEntry& entry) { return entry.target == &target; });
        if (found == layerTargets_.end())
        {
            return false;
        }
        found->active = false;
        found->target = nullptr;
        return true;
    }

    void spRenderTargetManager::ReleaseTargetsForDeviceReset() noexcept
    {
        const auto release = [](std::vector<Entry>& entries)
        {
            for (auto& entry : entries)
            {
                if (entry.active && entry.target != nullptr)
                {
                    entry.target->ReleaseTargetsForDeviceReset();
                }
            }
        };
        release(ordinaryTargets_);
        release(cubeTargets_);
        for (auto& entry : layerTargets_)
        {
            if (entry.active && entry.target != nullptr)
            {
                entry.target->ReleaseRenderTargetsForDeviceResetForAnalysis();
            }
        }
    }

    bool spRenderTargetManager::ReinitTargetsForDeviceReset()
    {
        bool success = true;
        const auto reinit = [&success](std::vector<Entry>& entries)
        {
            for (auto& entry : entries)
            {
                if (entry.active && entry.target != nullptr)
                {
                    success = entry.target->ReinitTargetsForDeviceReset()
                        && success;
                }
            }
        };
        reinit(ordinaryTargets_);
        reinit(cubeTargets_);
        for (auto& entry : layerTargets_)
        {
            if (entry.active && entry.target != nullptr)
            {
                success = entry.target->ReinitRenderTargetsForDeviceResetForAnalysis()
                    && success;
            }
        }
        return success;
    }

    std::size_t spRenderTargetManager::GetOrdinaryTargetCountForAnalysis()
        const noexcept
    {
        return ordinaryTargets_.size();
    }

    std::size_t spRenderTargetManager::GetCubeTargetCountForAnalysis()
        const noexcept
    {
        return cubeTargets_.size();
    }

    std::size_t spRenderTargetManager::GetActiveOrdinaryTargetCountForAnalysis()
        const noexcept
    {
        return static_cast<std::size_t>(std::count_if(
            ordinaryTargets_.begin(), ordinaryTargets_.end(),
            [](const Entry& entry) { return entry.active; }));
    }

    std::size_t spRenderTargetManager::GetActiveCubeTargetCountForAnalysis()
        const noexcept
    {
        return static_cast<std::size_t>(std::count_if(
            cubeTargets_.begin(), cubeTargets_.end(),
            [](const Entry& entry) { return entry.active; }));
    }

    std::size_t spRenderTargetManager::GetLayerTargetCountForAnalysis()
        const noexcept
    {
        return layerTargets_.size();
    }

    std::size_t spRenderTargetManager::GetActiveLayerTargetCountForAnalysis()
        const noexcept
    {
        return static_cast<std::size_t>(std::count_if(
            layerTargets_.begin(), layerTargets_.end(),
            [](const LayerEntry& entry) { return entry.active; }));
    }

    std::size_t spRenderTargetManager::GetCurrentTargetIndexForAnalysis()
        const noexcept
    {
        return currentTargetIndex_;
    }

    std::size_t spRenderTargetManager::GetTargetSlotCountForAnalysis()
        const noexcept
    {
        return targetSlotCount_;
    }

    bool spRenderTargetManager::SetCurrentTargetIndexForAnalysis(
        const std::size_t index) noexcept
    {
        if (index > targetSlotCount_)
        {
            return false;
        }
        currentTargetIndex_ = index;
        return true;
    }
}
