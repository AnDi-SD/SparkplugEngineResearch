#include "spQuad.h"
#include "spMaterial.h"
#include "spVertexBuffer.h"
namespace sparkplug::reconstruction {
namespace {
std::unique_ptr<spBaseObject> Create(){return std::make_unique<spQuad>();}
const spRTTIRecord Record{spQuad::ClassID,spBaseObject::ClassID,"spQuad",&spBaseObject::StaticRTTI(),&Create,nullptr};
const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
}
spQuad::~spQuad()=default;
const spRTTIRecord& spQuad::StaticRTTI() noexcept{(void)Registered;return Record;}
const spRTTIRecord& spQuad::vfunc_18() const noexcept{return Record;}
}
