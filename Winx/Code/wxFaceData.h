#pragma once
// Inferred path. PC5A5320/5A4FE0, RTTI6D65B0; PS2 field diagnostics corroborate
// member names/widths. Numeric flags and IDs have no invented gameplay meaning.
#include "Code/Sparkplug/spExtensionData.h"
namespace winx::reconstruction {
class wxFaceData final : public sparkplug::reconstruction::spCustomAppData {
public:
    static constexpr sparkplug::reconstruction::spClassID ClassID=0x313C4C17;
    static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
    const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
    std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(sparkplug::reconstruction::spCloneManager&) const override;
    bool ReadForAnalysis(sparkplug::reconstruction::spStream&,std::uint32_t maximumBytes) override;
    bool WriteForAnalysis(sparkplug::reconstruction::spStream&) const override;
    void CopyFromForAnalysis(const spCustomAppData&) override;
    std::uint8_t GetSurfaceTypeForAnalysis() const noexcept{return surfaceType_;}
    std::uint16_t GetFlagsForAnalysis() const noexcept{return flags_;}
    std::uint8_t GetSurfaceIDForAnalysis() const noexcept{return surfaceID_;}
    // Inspector observation only; native CopyFrom copies just the three values.
    std::uint8_t GetSerializedFieldMaskForAnalysis() const noexcept{return serializedFieldMask_;}
private:
    std::uint8_t surfaceType_=0,surfaceID_=0;
    std::uint16_t flags_=0;
    std::uint8_t serializedFieldMask_=0;
};
}
