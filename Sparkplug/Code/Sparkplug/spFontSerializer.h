#pragma once
#include "spSerializer.h"
#include "Analysis/PC/spReferenceInspection.h"
namespace sparkplug::reconstruction {
class spFont;
class spFontSerializer final : public spSerializer {
public:
    static constexpr spClassID ClassID=0x7BA30BCD;
    static const spRTTIRecord& StaticRTTI() noexcept;
    const spRTTIRecord& vfunc_18() const noexcept override;
    std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override { return nullptr; }
    bool vfunc_14(spBaseObject&,spCloneManager&) const override { return false; }
    bool ReadPayloadForAnalysis(spSerializerReadContextForAnalysis&,spStream&,
        std::uint32_t,spBaseObject&,std::string*) const override;
    struct InspectionForAnalysis {
        bool hasImage=false;
        evidence::pc::serialization::InspectedReference image;
    };
    // Same field reader. Atlas extent is observed without constructing a fake
    // texture; the partial Font carries actual metrics/glyph assignments only.
    bool InspectPayloadForAnalysis(spStream&,std::uint32_t,spFont&,
        InspectionForAnalysis&,std::string*) const;
private:
    bool ReadFieldsForAnalysis(spSerializerReadContextForAnalysis&,spStream&,
        std::uint32_t,spBaseObject&,std::string*,InspectionForAnalysis*) const;
};
}
