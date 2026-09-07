#include "spRenderable.h"

#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord RenderableRecord{
            spRenderable::ClassID,
            spNamedObject::ClassID,
            "spRenderable",
            &spNamedObject::StaticRTTI(),
            nullptr,
            nullptr,
        };

        const bool RenderableRegistered = spRTTIManager::Instance().RegisterDeferredForAnalysis(RenderableRecord);
    } // namespace

    spRenderable::~spRenderable() = default;

    const spRTTIRecord& spRenderable::StaticRTTI() noexcept
    {
        (void)RenderableRegistered;
        return RenderableRecord;
    }

    std::unique_ptr<spBaseObject> spRenderable::vfunc_10(spCloneManager&) const
    {
        return nullptr;
    }

    bool spRenderable::vfunc_14(spBaseObject& destination, spCloneManager& manager) const
    {
        auto* renderable = dynamic_cast<spRenderable*>(&destination);
        if (renderable == nullptr || !spNamedObject::vfunc_14(destination, manager))
        {
            return false;
        }

        renderable->field28_ = field28_;
        renderable->alphaSortEnabled_ = alphaSortEnabled_;
        renderable->priority_ = priority_;
        renderable->material_ = material_;
        renderable->InvalidateRuntimeModeForAnalysis();
        renderable->fog_ = fog_;
        return true;
    }

    const spRTTIRecord& spRenderable::vfunc_18() const noexcept
    {
        return RenderableRecord;
    }

    void spRenderable::SetMaterialForAnalysis(std::shared_ptr<spBaseObject> material) noexcept
    {
        material_ = std::move(material);
        InvalidateRuntimeModeForAnalysis();
    }

    void spRenderable::SetFogForAnalysis(std::shared_ptr<spBaseObject> fog) noexcept
    {
        fog_ = std::move(fog);
    }

    const std::shared_ptr<spBaseObject>& spRenderable::GetMaterialForAnalysis() const noexcept
    {
        return material_;
    }

    const std::shared_ptr<spBaseObject>& spRenderable::GetFogForAnalysis() const noexcept
    {
        return fog_;
    }

    void spRenderable::SetAlphaSortEnabledForAnalysis(const bool enabled) noexcept
    {
        alphaSortEnabled_ = enabled;
    }

    bool spRenderable::IsAlphaSortEnabledForAnalysis() const noexcept
    {
        return alphaSortEnabled_;
    }

    void spRenderable::SetPriorityForAnalysis(const std::uint32_t priority) noexcept
    {
        priority_ = priority;
    }

    std::uint32_t spRenderable::GetPriorityForAnalysis() const noexcept
    {
        return priority_;
    }

    void spRenderable::SetField28ForAnalysis(const std::uint32_t value) noexcept
    {
        field28_ = value;
    }

    std::uint32_t spRenderable::GetField28ForAnalysis() const noexcept
    {
        return field28_;
    }

    std::size_t spRenderable::CallbackIndex(const CallbackPhaseForAnalysis phase) noexcept
    {
        return phase == CallbackPhaseForAnalysis::Pre ? 0u : 1u;
    }

    bool spRenderable::SetDirectCallbackForAnalysis(
        const CallbackPhaseForAnalysis phase, const DirectCallbackForAnalysis callback) noexcept
    {
        if (dispatchActive_)
            return false;
        callbackGroups_[CallbackIndex(phase)].direct = callback;
        return true;
    }

    bool spRenderable::AddGroupCallbackForAnalysis(const CallbackPhaseForAnalysis phase,
                                                   const GroupCallbackForAnalysis callback,
                                                   void* const user)
    {
        auto& records = callbackGroups_[CallbackIndex(phase)].records;
        if (dispatchActive_ || callback == nullptr || records.size() >= 4096)
            return false; // explicit host bounds/null/reentry guard, not native API proof
        records.push_back({callback, user});
        return true;
    }

    bool spRenderable::ClearGroupCallbacksForAnalysis(const CallbackPhaseForAnalysis phase) noexcept
    {
        if (dispatchActive_)
            return false;
        callbackGroups_[CallbackIndex(phase)].records.clear();
        return true;
    }

    void spRenderable::SetGroupCallbacksEnabledForAnalysis(const CallbackPhaseForAnalysis phase,
                                                           const bool enabled) noexcept
    {
        callbackGroups_[CallbackIndex(phase)].enabled = enabled;
    }

    std::size_t spRenderable::GetGroupCallbackCountForAnalysis(
        const CallbackPhaseForAnalysis phase) const noexcept
    {
        return callbackGroups_[CallbackIndex(phase)].records.size();
    }

    bool spRenderable::DispatchCallbackPhaseForAnalysis(const CallbackPhaseForAnalysis phase,
                                                        spCamera* const camera, void* const support)
    {
        if (dispatchActive_)
            return false;
        struct DispatchGuard
        {
            bool& active;
            ~DispatchGuard()
            {
                active = false;
            }
        } guard{dispatchActive_};
        dispatchActive_ = true;
        auto& group = callbackGroups_[CallbackIndex(phase)];
        if (group.enabled)
        {
            std::size_t position = 0;
            std::uint32_t ordinal = 0;
            while (position < group.records.size())
            {
                const auto record = group.records[position];
                const auto result = record.callback(this, camera, support, ordinal, record.user);
                if (result == -1)
                    group.records.erase(group.records.begin() +
                                        static_cast<std::ptrdiff_t>(position));
                else if (result == 0)
                    break;
                else
                    ++position;
                ++ordinal;
            }
        }
        return group.direct == nullptr ||
               (static_cast<std::uint32_t>(group.direct(this, camera, support)) & 0xffu) != 0;
    }

    const spRenderable::BoundingSphere& spRenderable::GetBoundingSphereForAnalysis() const noexcept
    {
        static const BoundingSphere zero{};
        return zero;
    }

    void spRenderable::GetBoundsForAnalysis(BoundsPosition& minimum,
                                            BoundsPosition& maximum) const noexcept
    {
        const auto& sphere = GetBoundingSphereForAnalysis();
        for (std::size_t i = 0; i < 3; ++i)
        {
            minimum[i] = sphere[i] - sphere[3];
            maximum[i] = sphere[i] + sphere[3];
        }
    }

    void spRenderable::InvalidateRuntimeModeForAnalysis() noexcept
    {
        runtimeMode_ = 0;
    }

    std::uint32_t spRenderable::GetRuntimeModeForAnalysis() const noexcept
    {
        return runtimeMode_;
    }
} // namespace sparkplug::reconstruction
