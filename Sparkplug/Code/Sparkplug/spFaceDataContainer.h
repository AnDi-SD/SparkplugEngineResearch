#pragma once
#include "spExtensionData.h"
#include <vector>
namespace sparkplug::reconstruction {
// PC47D5E0/47D530 and RTTI6D40C0. Host unique_ptrs replace native direct owners.
class spFaceDataContainer final : public spExtensionData {
public:
    static constexpr spClassID ClassID=0x6E954A6A;
    static const spRTTIRecord& StaticRTTI() noexcept;
    const spRTTIRecord& vfunc_18() const noexcept override;
    std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
    bool ReadForAnalysis(spStream&,std::uint32_t maximumBytes) override;
    bool WriteForAnalysis(spStream&) const override;
    spClassID GetElementClassForAnalysis() const noexcept{return elementClass_;}
    const auto& GetElementsForAnalysis() const noexcept{return elements_;}
private:
    spClassID elementClass_=0;
    std::vector<std::unique_ptr<spExtensionData>> elements_;
};
}
