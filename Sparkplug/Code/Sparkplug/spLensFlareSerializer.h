#pragma once
#include "spRenderableSerializer.h"
namespace sparkplug::reconstruction {
class spLensFlareSerializer : public spRenderableSerializer {
public:
    static constexpr spClassID ClassID=0x72350266;
    [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
    [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
    [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override{return nullptr;}
    [[nodiscard]] spClassID GetTargetClassIDForAnalysis() const noexcept override{return 0x435370B5;}
    bool ReadPayloadForAnalysis(spSerializerReadContextForAnalysis&,spStream&,std::uint32_t,spBaseObject&,std::string*) const override;
    bool WritePayloadForAnalysis(spStream&,const spBaseObject&,std::string*) const override;
    bool WritePayloadWithContextForAnalysis(spSerializerManager&,spStream&,const spBaseObject&,std::string*) const override;
    bool IndexRelationshipsWithContextForAnalysis(spSerializerManager&,spBaseObject&) const override{return false;}
};
}
