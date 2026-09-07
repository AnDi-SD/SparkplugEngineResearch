#include "spModel.h"

#include "spMesh.h"

#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateModel()
        {
            return std::make_unique<spModel>();
        }

        const spRTTIRecord ModelRecord{
            spModel::ClassID,
            spRenderable::ClassID,
            "spModel",
            &spRenderable::StaticRTTI(),
            &CreateModel,
            nullptr,
        };

        const bool ModelRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(ModelRecord);
    }

    spModel::~spModel() = default;

    const spRTTIRecord& spModel::StaticRTTI() noexcept
    {
        (void)ModelRegistered;
        return ModelRecord;
    }

    std::unique_ptr<spBaseObject> spModel::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spModel>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    bool spModel::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        auto* model = dynamic_cast<spModel*>(&destination);
        if (model == nullptr || !spRenderable::vfunc_14(destination, manager))
        {
            return false;
        }
        model->baseMesh_ = baseMesh_;
        model->projectionGroup_ = projectionGroup_;
        return true;
    }

    const spRTTIRecord& spModel::vfunc_18() const noexcept
    {
        return ModelRecord;
    }

    void spModel::SetBaseMeshForAnalysis(
        std::shared_ptr<spMesh> mesh) noexcept
    {
        baseMesh_ = std::move(mesh);
        InvalidateRuntimeModeForAnalysis();
    }

    const std::shared_ptr<spMesh>&
        spModel::GetBaseMeshForAnalysis() const noexcept
    {
        return baseMesh_;
    }

    void spModel::SetProjectionGroupForAnalysis(const std::uint32_t group) noexcept
    {
        projectionGroup_ = group;
    }

    std::uint32_t spModel::GetProjectionGroupForAnalysis() const noexcept
    {
        return projectionGroup_;
    }

    const spModel::BoundingSphere& spModel::GetBoundingSphereForAnalysis() const noexcept
    {
        return baseMesh_ ? baseMesh_->GetBoundingSphereForAnalysis()
                         : spRenderable::GetBoundingSphereForAnalysis();
    }

    void spModel::GetBoundsForAnalysis(BoundsPosition& minimum,
                                       BoundsPosition& maximum) const noexcept
    {
        minimum = baseMesh_ ? baseMesh_->GetMinimumForAnalysis() : BoundsPosition{};
        maximum = baseMesh_ ? baseMesh_->GetMaximumForAnalysis() : BoundsPosition{};
    }

    bool spModel::HasBoundsForAnalysis() const noexcept
    {
        return baseMesh_ && baseMesh_->HasBoundsForAnalysis();
    }

    void spModel::InvalidateRuntimeModeForAnalysis() noexcept
    {
        // Exact PC vslot34 =48EAA0. The PS2 classifier is a different body;
        // neither its categories nor a forced zero are a PC implementation.
    }
}
