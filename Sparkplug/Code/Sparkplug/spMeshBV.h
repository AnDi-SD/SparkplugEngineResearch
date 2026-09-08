#pragma once
#include "spBoundingVolume.h"
#include "spCollisionMesh.h"
namespace sparkplug::reconstruction {
// PC47AFF0 factory,47A210 ownership/format checks. Only serialized geometry,
// bounds and CollisionInfo transform effects are exposed. OPCODE query-tree
// construction/query APIs remain outside this tools slice; no success stub.
class spMeshBV final : public spBoundingVolume {
public:
    static constexpr spClassID ClassID=0x3F453DE7;
    static const spRTTIRecord& StaticRTTI() noexcept;
    const spRTTIRecord& vfunc_18() const noexcept override;
    std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
    const spCollisionMesh* GetDataForAnalysis() const noexcept{return data_.get();}
    // Explicitly the ownership/bounds portion of SetData, not its OPCODE result.
    bool SetDataAndBoundsForAnalysis(std::unique_ptr<spCollisionMesh> data);
    void UpdateCollisionTransformForAnalysis(Vector3&,Matrix3&,const Vector3&) const noexcept override;
private:
    friend class spMeshBVSerializer;
    std::unique_ptr<spCollisionMesh> data_;
};
}
