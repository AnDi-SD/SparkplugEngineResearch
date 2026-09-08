#include "spFaceDataContainer.h"
namespace sparkplug::reconstruction {
namespace {
std::unique_ptr<spBaseObject> Create(){return std::make_unique<spFaceDataContainer>();}
const spRTTIRecord Record{spFaceDataContainer::ClassID,spExtensionData::ClassID,"spFaceDataContainer",
    &spExtensionData::StaticRTTI(),&Create,nullptr};
const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
}
const spRTTIRecord& spFaceDataContainer::StaticRTTI() noexcept{(void)Registered;return Record;}
const spRTTIRecord& spFaceDataContainer::vfunc_18() const noexcept{return Record;}
std::unique_ptr<spBaseObject> spFaceDataContainer::vfunc_10(spCloneManager& manager) const {
    auto clone=std::make_unique<spFaceDataContainer>();manager.RegisterCloneForAnalysis(*this,*clone);
    return vfunc_14(*clone,manager)?std::move(clone):nullptr;
}
bool spFaceDataContainer::ReadForAnalysis(spStream& stream,std::uint32_t maximumBytes) {
    std::uint32_t start=0,count=0,position=0;
    if(maximumBytes<8||!stream.GetCurrentPosition(start)||!stream.Read(elementClass_)||!stream.Read(count)
        ||count>65536||count>maximumBytes-8)return false; // host count/byte cap
    // Read is exposed for fresh/empty containers; native re-reading populated
    // containers overwrites raw owners. Reject this host lifetime hazard.
    if(!elements_.empty())return false;
    elements_.resize(count);
    for(auto& target:elements_) {
        auto object=spRTTIManager::Instance().Create(elementClass_);
        auto* typed=dynamic_cast<spExtensionData*>(object.get());
        if(!typed||!stream.GetCurrentPosition(position)||position<start||position-start>=maximumBytes)return false;
        if(!typed->ReadForAnalysis(stream,maximumBytes-(position-start)))return false;
        object.release();target.reset(typed);
    }
    return true;
}
bool spFaceDataContainer::WriteForAnalysis(spStream& stream) const {
    if(elements_.size()>65536||!stream.Write(elementClass_)||!stream.Write(static_cast<std::uint32_t>(elements_.size())))return false;
    for(const auto& item:elements_)if(!item||!item->WriteForAnalysis(stream))return false;
    return true;
}
}
