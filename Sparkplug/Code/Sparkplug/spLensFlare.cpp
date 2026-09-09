#include "spLensFlare.h"
#include <cstring>
namespace sparkplug::reconstruction {
namespace {
std::unique_ptr<spBaseObject> Create(){return std::make_unique<spLensFlare>();}
const spRTTIRecord Record{spLensFlare::ClassID,spRenderable::ClassID,"spLensFlare",&spRenderable::StaticRTTI(),&Create,nullptr};
const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
}
const spRTTIRecord& spLensFlare::StaticRTTI() noexcept{(void)Registered;return Record;}
const spRTTIRecord& spLensFlare::vfunc_18() const noexcept{return Record;}
const spRenderable::BoundingSphere& spLensFlare::GetBoundingSphereForAnalysis() const noexcept {
    const std::uint32_t bits=0x3fb504f3;float factor;std::memcpy(&factor,&bits,4);
    sphere_={0,0,0,radius_*factor};return sphere_; // actual PC4D74B0
}
}
