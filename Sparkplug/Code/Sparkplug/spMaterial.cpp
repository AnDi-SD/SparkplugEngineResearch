#include "spMaterial.h"
#include "spMaterialColorController.h"

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
            spRTTIManager::Instance().RegisterDeferredForAnalysis(MaterialRecord);
    }

    spMaterial::spMaterial() noexcept = default;

    spMaterial::~spMaterial()
    {
        // Host context may retain canonical controller beyond holder lifetime.
        // Base destruction must not call a derived color-interface setter.
        auto* color=dynamic_cast<spMaterialColorController*>(materialColorController_.get());
        if(color&&color->GetMaterialForAnalysis()==this)color->DetachMaterialForAnalysis();
    }

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
        if (dynamic_cast<spMaterialColorController*>(materialColorController_.get())
            || !destination.IsKindOf(ClassID)
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
        return index < passCount_ ? passes_[index].get() : nullptr;
    }

    bool spMaterial::SetPassForAnalysis(
        const std::size_t index,
        std::shared_ptr<spBaseObject> pass) noexcept
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
        else if (index < passCount_)
        {
            // PC423960 removes a slot and shifts the remaining seven-slot
            // tail, not merely trims trailing null entries.
            --passCount_;
            for (auto next=index;next+1<passes_.size();++next)
                passes_[next]=std::move(passes_[next+1]);
            passes_.back().reset();
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
        return materialColorController_.get();
    }

    void spMaterial::SetMaterialColorControllerForAnalysis(
        std::shared_ptr<spBaseObject> controller) noexcept
    {
        // Actual423A50 also invokes the binder on alias; NULL only releases.
        // Host-only pinning guard when a canonical former controller survives.
        if(materialColorController_.get()!=controller.get())
        {auto* old=dynamic_cast<spMaterialColorController*>(materialColorController_.get());if(old&&old->GetMaterialForAnalysis()==this)old->DetachMaterialForAnalysis();}
        materialColorController_ = std::move(controller);
        if(auto* color=dynamic_cast<spMaterialColorController*>(materialColorController_.get()))color->BindMaterialForAnalysis(this);
    }
}
