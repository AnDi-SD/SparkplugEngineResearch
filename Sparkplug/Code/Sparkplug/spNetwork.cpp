#include "spNetwork.h"
namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord Record{spNetwork::ClassID,spCrossPlatform::ClassID,"spNetwork",&spCrossPlatform::StaticRTTI(),nullptr,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    spNetwork::spNetwork() noexcept:connected_(0),type_(1),address_(0xffffffff),port_(0xffff){}
    // Native004A1C00 restores both tables, then destroys the secondary
    // CrossPlatform subobject. This base owns no socket/platform service.
    spNetwork::~spNetwork()=default;
    const spRTTIRecord& spNetwork::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spNetwork::vfunc_18() const noexcept{return StaticRTTI();}
    std::unique_ptr<spBaseObject> spNetwork::vfunc_10(spCloneManager&) const{return nullptr;}
    bool spNetwork::vfunc_14(spBaseObject& target,spCloneManager&) const
    {
        // Secondary PC slot+0C inherits00413120: copy only the physical name.
        // Keep this common behavior here for all network implementations.
        auto* named=dynamic_cast<spNamedObject*>(&target);
        if(!named)return false; // Explicit host protection for an invalid destination.
        // Original clears/releases the destination before reading the source.
        // Self-copy consequently drops the name while leaving network fields.
        named->SetName(nullptr);
        CopyNameToForAnalysis(*named);return true;
    }
    // Primary slot+40 shares the native null-return entry with object Clone.
    const char* spNetwork::GetLocalAddressTextForAnalysis(){return nullptr;}
}
