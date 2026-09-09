#pragma once
// Inferred path. PC465050 ->4742A0; PS21248E0. Metadata/ownership and
// represented bounds effects only; no collision-manager/query facade.
#include "spBoundingVolume.h"
#include <vector>
namespace sparkplug::reconstruction
{
    class spNode;
    class spPartitionNode;
    class spCollisionInfo final : public spBaseObject
    {
        friend class spNode;
        friend class spPartitionNode;
    public:
        static constexpr spClassID ClassID=0x47A97C0E;
        using Vector3=spBoundingVolume::Vector3;
        using Matrix3=spBoundingVolume::Matrix3;
        spCollisionInfo() noexcept=default;
        ~spCollisionInfo() override;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination,spCloneManager& manager) const override;
        [[nodiscard]] spNode* GetNodeForAnalysis() const noexcept{return node_;}
        [[nodiscard]] spBoundingVolume* GetPrimitiveForAnalysis() const noexcept{return primitive_.get();}
        void SetPrimitiveForAnalysis(std::shared_ptr<spBoundingVolume> value) noexcept{primitive_=std::move(value);}
        [[nodiscard]] std::uint32_t GetGroupForAnalysis() const noexcept{return group_;}
        void SetGroupForAnalysis(std::uint32_t value) noexcept{group_=value;}
        [[nodiscard]] const Vector3& GetPositionForAnalysis() const noexcept{return position_;}
        [[nodiscard]] const Matrix3& GetOrientationForAnalysis() const noexcept{return orientation_;}
        [[nodiscard]] const Vector3& GetScaleForAnalysis() const noexcept{return scale_;}
        [[nodiscard]] const Vector3& GetBoundingCenterForAnalysis() const noexcept{return boundingCenter_;}
        [[nodiscard]] float GetBoundingRadiusForAnalysis() const noexcept{return boundingRadius_;}
        void SetTransformForAnalysis(const Vector3& p,const Matrix3& r,const Vector3& s) noexcept
        {position_=p;orientation_=r;scale_=s;}
        // Native4651E0 dereferences a null primitive; the tools boundary rejects
        // it explicitly. Scene partition/query notifications are unrepresented.
        [[nodiscard]] bool UpdateWorldForAnalysis() noexcept;
        [[nodiscard]] const std::vector<spPartitionNode*>& GetPartitionsForAnalysis() const noexcept{return partitionRoots_;}
        void RemovePartitionForAnalysis(spPartitionNode* root,bool notify=true) noexcept;
    private:
        spNode* node_=nullptr;
        std::shared_ptr<spBoundingVolume> primitive_;
        std::uint32_t group_=1;
        Vector3 position_{};
        Matrix3 orientation_{1,0,0,0,1,0,0,0,1};
        Vector3 scale_{1,1,1};
        Vector3 boundingCenter_{};
        float boundingRadius_=0;
        std::vector<spPartitionNode*> partitionRoots_; // borrowed native6C vector
    };
}
