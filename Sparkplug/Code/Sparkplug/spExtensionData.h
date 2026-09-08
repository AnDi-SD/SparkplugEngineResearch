#pragma once
// Inferred paths; PC6D4090/6D6550 identify both abstract classes. This is the
// stream/ownership slice used by face containers, not application callbacks.
#include "Code/SparkBase/spBaseObject.h"
#include "Code/SparkBase/spStream.h"
namespace sparkplug::reconstruction {
class spExtensionData : public spBaseObject {
public:
    static constexpr spClassID ClassID=0x6A1D0D2E;
    static const spRTTIRecord& StaticRTTI() noexcept;
    const spRTTIRecord& vfunc_18() const noexcept override;
    // Native owns the optional extension at +10 and deletes it in438230.
    virtual bool ReadForAnalysis(spStream&,std::uint32_t maximumBytes)=0;
    virtual bool WriteForAnalysis(spStream&) const=0;
protected:
    std::unique_ptr<spBaseObject> extension_;
};
class spCustomAppData : public spExtensionData {
public:
    static constexpr spClassID ClassID=0x76181F6B;
    static const spRTTIRecord& StaticRTTI() noexcept;
    const spRTTIRecord& vfunc_18() const noexcept override;
    bool vfunc_14(spBaseObject& destination,spCloneManager&) const override;
    virtual void CopyFromForAnalysis(const spCustomAppData& source)=0;
};
}
