#include "spExtensionData.h"
namespace sparkplug::reconstruction {
namespace {
const spRTTIRecord Extension{spExtensionData::ClassID,spBaseObject::ClassID,"spExtensionData",
    &spBaseObject::StaticRTTI(),nullptr,nullptr};
const spRTTIRecord Custom{spCustomAppData::ClassID,spExtensionData::ClassID,"spCustomAppData",
    &spExtensionData::StaticRTTI(),nullptr,nullptr};
const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Extension)
    &&spRTTIManager::Instance().RegisterDeferredForAnalysis(Custom);
}
const spRTTIRecord& spExtensionData::StaticRTTI() noexcept{(void)Registered;return Extension;}
const spRTTIRecord& spExtensionData::vfunc_18() const noexcept{return Extension;}
const spRTTIRecord& spCustomAppData::StaticRTTI() noexcept{(void)Registered;return Custom;}
const spRTTIRecord& spCustomAppData::vfunc_18() const noexcept{return Custom;}
bool spCustomAppData::vfunc_14(spBaseObject& destination,spCloneManager&) const {
    // PC5A3700 invokes the destination's copy slot, then returns true.
    auto* target=dynamic_cast<spCustomAppData*>(&destination);
    if(!target)return false; // host type guard
    target->CopyFromForAnalysis(*this);return true;
}
}
