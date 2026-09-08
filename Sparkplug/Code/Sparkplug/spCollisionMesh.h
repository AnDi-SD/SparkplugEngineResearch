#pragma once
#include "spIndexBuffer.h"
#include "spVertexBuffer.h"
#include "spFaceDataContainer.h"
namespace sparkplug::reconstruction {
// PC47D700 owns these three objects directly. The original name/RTTI comes
// from6D40F0; no native triangle query interface is exposed by this slice.
class spCollisionMesh final : public spBaseObject {
public:
    static constexpr spClassID ClassID=0x36432CFF;
    static const spRTTIRecord& StaticRTTI() noexcept;
    const spRTTIRecord& vfunc_18() const noexcept override;
    std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
    const spIndexBuffer* GetIndicesForAnalysis() const noexcept{return indices_.get();}
    const spVertexBuffer* GetVerticesForAnalysis() const noexcept{return vertices_.get();}
    const spFaceDataContainer* GetFacesForAnalysis() const noexcept{return faces_.get();}
private:
    friend class spMeshBVSerializer;
    std::unique_ptr<spIndexBuffer> indices_;
    std::unique_ptr<spVertexBuffer> vertices_;
    std::unique_ptr<spFaceDataContainer> faces_;
};
}
