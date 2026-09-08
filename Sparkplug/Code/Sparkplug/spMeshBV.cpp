#include "spMeshBV.h"
#include "Analysis/PC/spVertexBounds.h"
#include <cmath>
#include <cstring>
namespace sparkplug::reconstruction {
namespace {
std::unique_ptr<spBaseObject> Create(){return std::make_unique<spMeshBV>();}
const spRTTIRecord Record{spMeshBV::ClassID,spBoundingVolume::ClassID,"spMeshBV",
    &spBoundingVolume::StaticRTTI(),&Create,nullptr};
const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
}
const spRTTIRecord& spMeshBV::StaticRTTI() noexcept{(void)Registered;return Record;}
const spRTTIRecord& spMeshBV::vfunc_18() const noexcept{return Record;}
std::unique_ptr<spBaseObject> spMeshBV::vfunc_10(spCloneManager& manager) const {
    auto clone=std::make_unique<spMeshBV>();manager.RegisterCloneForAnalysis(*this,*clone);
    return vfunc_14(*clone,manager)?std::move(clone):nullptr;
}
bool spMeshBV::SetDataAndBoundsForAnalysis(std::unique_ptr<spCollisionMesh> data) {
    data_=std::move(data); // native deletes the previous owner before validation
    if(!data_)return true; // native leaves the previous sphere intact
    const auto* ib=data_->GetIndicesForAnalysis();const auto* vb=data_->GetVerticesForAnalysis();
    if(!ib||!vb||(ib->GetFormatFlagsForAnalysis()&1)
        ||ib->GetTypeForAnalysis()!=spIndexBuffer::eIndexBufferType::Type2
        ||vb->GetComponentFlagsForAnalysis()!=0)return false;
    // Host guards keep corrupt geometry out of unbounded native query code.
    const auto count=vb->GetVertexCountForAnalysis();
    if(!count||!ib->GetIndexCountForAnalysis())return false;
    for(std::uint32_t i=0;i<ib->GetIndexCountForAnalysis();++i) {
        const auto index=ib->GetIndexForAnalysis(i);if(!index||*index>=count)return false;
    }
    const auto& bytes=vb->GetDataForAnalysis();
    for(std::size_t i=0;i+4<=bytes.size();i+=4) {
        float value=0;std::memcpy(&value,bytes.data()+i,4);if(!std::isfinite(value))return false;
    }
    std::array<float,4> sphere{};if(!evidence::pc::ComputeVertexSphere(*vb,sphere))return false;
    for(auto value:sphere)if(!std::isfinite(value))return false;
    boundingCenter_={sphere[0],sphere[1],sphere[2]};boundingRadius_=sphere[3];return true;
}
void spMeshBV::UpdateCollisionTransformForAnalysis(Vector3&,Matrix3&,const Vector3&) const noexcept {
    // Actual PC47A180 is exactly RET16: no parameter or instance mutation.
}
}
