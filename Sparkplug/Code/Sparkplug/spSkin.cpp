#include "spSkin.h"

#include "spNode.h"

#include <algorithm>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateSkin()
        {
            return std::make_unique<spSkin>();
        }

        const spRTTIRecord SkinRecord{
            spSkin::ClassID,
            spModel::ClassID,
            "spSkin",
            &spModel::StaticRTTI(),
            &CreateSkin,
            nullptr,
        };

        const bool SkinRegistered =
            spRTTIManager::Instance().Register(SkinRecord);
    }

    bool spSkin::BoneBinding::operator==(const BoneBinding& other) const noexcept
    {
        return bone == other.bone && inverseBindMatrix == other.inverseBindMatrix;
    }

    spSkin::~spSkin() = default;

    const spRTTIRecord& spSkin::StaticRTTI() noexcept
    {
        (void)SkinRegistered;
        return SkinRecord;
    }

    std::unique_ptr<spBaseObject> spSkin::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spSkin>();
        manager.RegisterClone(*this, *clone);
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }

    bool spSkin::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        auto* skin = dynamic_cast<spSkin*>(&destination);
        if (skin == nullptr || !spModel::vfunc_14(destination, manager))
        {
            return false;
        }

        skin->weightCount_ = weightCount_;
        skin->boneBindings_ = boneBindings_;

        // Native PC 0x0046A650 asks the clone manager to remap every bone.
        // Reuse a mapped node when the surrounding node graph has already
        // registered one; otherwise retain the original relationship.
        for (auto& binding : skin->boneBindings_)
        {
            if (binding.bone == nullptr)
            {
                continue;
            }
            if (auto* mapped = dynamic_cast<spNode*>(manager.FindClone(*binding.bone)))
            {
                binding.bone = std::shared_ptr<spNode>(binding.bone, mapped);
            }
        }
        return true;
    }

    const spRTTIRecord& spSkin::vfunc_18() const noexcept
    {
        return SkinRecord;
    }

    bool spSkin::SetPaletteForAnalysis(
        const std::uint32_t weightCount,
        std::vector<BoneBinding> bindings)
    {
        if (std::any_of(bindings.begin(), bindings.end(),
                [](const BoneBinding& binding) { return binding.bone == nullptr; }))
        {
            return false;
        }
        weightCount_ = weightCount;
        boneBindings_ = std::move(bindings);
        return true;
    }

    std::uint32_t spSkin::GetWeightCountForAnalysis() const noexcept
    {
        return weightCount_;
    }

    std::size_t spSkin::GetBoneCountForAnalysis() const noexcept
    {
        return boneBindings_.size();
    }

    const std::vector<spSkin::BoneBinding>&
        spSkin::GetBoneBindingsForAnalysis() const noexcept
    {
        return boneBindings_;
    }
}
