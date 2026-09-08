#pragma once
#include "spSerializer.h"
namespace sparkplug::reconstruction {
class spMeshBVSerializer final : public spSerializer {
public:
    static constexpr spClassID ClassID=0x6C662708;
    static constexpr spClassID TargetClassID=0x3F453DE7;
    static const spRTTIRecord& StaticRTTI() noexcept;
    const spRTTIRecord& vfunc_18() const noexcept override;
    std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
    spClassID GetTargetClassIDForAnalysis() const noexcept{return TargetClassID;}
    bool ReadPayloadForAnalysis(spSerializerReadContextForAnalysis&,spStream&,std::uint32_t,spBaseObject&,std::string*) const override;
    bool WritePayloadForAnalysis(spStream&,const spBaseObject&,std::string*) const override;
    bool IndexRelationshipsWithContextForAnalysis(spSerializerManager&,spBaseObject& object) const override{return object.IsExactly(TargetClassID);}
};
}
