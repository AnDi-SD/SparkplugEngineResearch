#include "spTextNodeSerializer.h"
namespace sparkplug::reconstruction {
namespace {
std::unique_ptr<spBaseObject> Create(){return std::make_unique<spTextNodeSerializer>();}
const spRTTIRecord Record{spTextNodeSerializer::ClassID,spRenderNodeSerializer::ClassID,"spTextNodeSerializer",
    &spRenderNodeSerializer::StaticRTTI(),&Create,nullptr};
const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
}
const spRTTIRecord& spTextNodeSerializer::StaticRTTI() noexcept{(void)Registered;return Record;}
const spRTTIRecord& spTextNodeSerializer::vfunc_18() const noexcept{return Record;}
}
