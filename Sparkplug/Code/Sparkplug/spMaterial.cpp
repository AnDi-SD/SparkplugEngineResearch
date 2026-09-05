#include "spMaterial.h"

#include <algorithm>

namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord MaterialRecord{
            spMaterial::ClassID,
            spBaseObject::ClassID,
            "spMaterial",
            &spBaseObject::StaticRTTI(),
            nullptr,
            nullptr,
        };

        const bool MaterialRegistered =
            spRTTIManager::Instance().Register(MaterialRecord);
    }

    spMaterial::spMaterial() noexcept = default;

    spMaterial::~spMaterial() = default;

    const spRTTIRecord& spMaterial::StaticRTTI() noexcept
    {
        (void)MaterialRegistered;
        return MaterialRecord;
    }

    std::unique_ptr<spBaseObject> spMaterial::vfunc_10(
        spCloneManager&) const
    {
        return nullptr;
    }

    bool spMaterial::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        if (!destination.IsKindOf(ClassID)
            || !spBaseObject::vfunc_14(destination, manager))
        {
            return false;
        }

        auto& materialDestination = static_cast<spMaterial&>(destination);
        materialDestination.renderStates_ = renderStates_;
        materialDestination.passes_ = passes_;
        materialDestination.passCount_ = passCount_;
        materialDestination.renderOverrideFlag_ = renderOverrideFlag_;
        materialDestination.useVertexAlpha_ = useVertexAlpha_;
        materialDestination.opaqueRuntimeField_ = opaqueRuntimeField_;
        materialDestination.materialColorController_ =
            materialColorController_;
        return true;
    }

    const spRTTIRecord& spMaterial::vfunc_18() const noexcept
    {
        return MaterialRecord;
    }

    const spMaterial::RenderStates&
    spMaterial::GetRenderStatesForAnalysis() const noexcept
    {
        return renderStates_;
    }

    std::uint32_t spMaterial::GetRenderStateForAnalysis(
        const std::size_t index) const noexcept
    {
        return index < renderStates_.size() ? renderStates_[index] : 0u;
    }

    bool spMaterial::SetRenderStateForAnalysis(
        const std::size_t index,
        const std::uint32_t value) noexcept
    {
        if (index >= renderStates_.size())
        {
            return false;
        }
        renderStates_[index] = value;
        return true;
    }

    std::size_t spMaterial::GetPassCountForAnalysis() const noexcept
    {
        return passCount_;
    }

    spBaseObject* spMaterial::GetPassForAnalysis(
        const std::size_t index) const noexcept
    {
        return index < passCount_ ? passes_[index] : nullptr;
    }

    bool spMaterial::SetPassForAnalysis(
        const std::size_t index,
        spBaseObject* const pass) noexcept
    {
        if (index >= passes_.size())
        {
            return false;
        }
        passes_[index] = pass;
        if (pass != nullptr)
        {
            passCount_ = std::max(passCount_, index + 1);
        }
        else if (index + 1 == passCount_)
        {
            while (passCount_ != 0 && passes_[passCount_ - 1] == nullptr)
            {
                --passCount_;
            }
        }
        return true;
    }

    bool spMaterial::UsesVertexAlphaForAnalysis() const noexcept
    {
        return useVertexAlpha_;
    }

    void spMaterial::SetUsesVertexAlphaForAnalysis(const bool value) noexcept
    {
        useVertexAlpha_ = value;
    }

    bool spMaterial::GetRenderOverrideFlagForAnalysis() const noexcept
    {
        return renderOverrideFlag_;
    }

    void spMaterial::SetRenderOverrideFlagForAnalysis(const bool value) noexcept
    {
        renderOverrideFlag_ = value;
    }

    std::uint32_t spMaterial::GetOpaqueRuntimeFieldForAnalysis() const noexcept
    {
        return opaqueRuntimeField_;
    }

    void spMaterial::SetOpaqueRuntimeFieldForAnalysis(
        const std::uint32_t value) noexcept
    {
        opaqueRuntimeField_ = value;
    }

    spBaseObject* spMaterial::GetMaterialColorControllerForAnalysis()
        const noexcept
    {
        return materialColorController_;
    }

    void spMaterial::SetMaterialColorControllerForAnalysis(
        spBaseObject* const controller) noexcept
    {
        materialColorController_ = controller;
    }
}
