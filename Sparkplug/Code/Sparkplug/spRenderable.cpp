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

        const bool RenderableRegistered =
            spRTTIManager::Instance().Register(RenderableRecord);
    }

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

    bool spRenderable::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        auto* renderable = dynamic_cast<spRenderable*>(&destination);
        if (renderable == nullptr
            || !spNamedObject::vfunc_14(destination, manager))
        {
            return false;
        }

        renderable->runtimeMode_ = 0;
        renderable->alphaSortEnabled_ = alphaSortEnabled_;
        renderable->priority_ = priority_;
        renderable->material_ = material_;
        renderable->fog_ = fog_;
        return true;
    }

    const spRTTIRecord& spRenderable::vfunc_18() const noexcept
    {
        return RenderableRecord;
    }

    void spRenderable::SetMaterialForAnalysis(
        std::shared_ptr<spBaseObject> material) noexcept
    {
        material_ = std::move(material);
        InvalidateRuntimeModeForAnalysis();
    }

    void spRenderable::SetFogForAnalysis(
        std::shared_ptr<spBaseObject> fog) noexcept
    {
        fog_ = std::move(fog);
    }

    const std::shared_ptr<spBaseObject>&
        spRenderable::GetMaterialForAnalysis() const noexcept
    {
        return material_;
    }

    const std::shared_ptr<spBaseObject>&
        spRenderable::GetFogForAnalysis() const noexcept
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

    void spRenderable::InvalidateRuntimeModeForAnalysis() noexcept
    {
        runtimeMode_ = 0;
    }

    std::uint32_t spRenderable::GetRuntimeModeForAnalysis() const noexcept
    {
        return runtimeMode_;
    }
}
