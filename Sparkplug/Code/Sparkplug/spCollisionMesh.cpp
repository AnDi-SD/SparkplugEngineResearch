#include "spCollisionMesh.h"
namespace sparkplug::reconstruction {
namespace {
std::unique_ptr<spBaseObject> Create(){return std::make_unique<spCollisionMesh>();}
const spRTTIRecord Record{spCollisionMesh::ClassID,spBaseObject::ClassID,"spCollisionMesh",
    &spBaseObject::StaticRTTI(),&Create,nullptr};
const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
}
const spRTTIRecord& spCollisionMesh::StaticRTTI() noexcept{(void)Registered;return Record;}
const spRTTIRecord& spCollisionMesh::vfunc_18() const noexcept{return Record;}
std::unique_ptr<spBaseObject> spCollisionMesh::vfunc_10(spCloneManager& manager) const {
    auto clone=std::make_unique<spCollisionMesh>();manager.RegisterCloneForAnalysis(*this,*clone);
    return vfunc_14(*clone,manager)?std::move(clone):nullptr;
}
}
