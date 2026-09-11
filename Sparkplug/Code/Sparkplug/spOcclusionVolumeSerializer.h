#pragma once
// Original PC class7F3030B8 -> spNodeSerializer; factory44F320, reader44F400.
#include "spNodeSerializer.h"
#include "spOcclusionVolume.h"
namespace sparkplug::reconstruction {
class spOcclusionVolumeSerializer final:public spNodeSerializer {
public:
    static constexpr spClassID ClassID=0x7F3030B8,TargetClassID=spOcclusionVolume::ClassID;
    // Host dependency injection; no implicit assumption about the game's CRT.
    explicit spOcclusionVolumeSerializer(spOcclusionVolume::SortDispatchForAnalysis sort={}) noexcept:sort_(sort){}
    [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
    [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
    // Serializer clone/copy is outside this reader slice (host refusal).
    [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override{return nullptr;}
    [[nodiscard]] spClassID GetTargetClassIDForAnalysis() const noexcept override{return TargetClassID;}
    [[nodiscard]] bool ReadPayloadForAnalysis(spSerializerReadContextForAnalysis&,spStream&,std::uint32_t,spBaseObject&,std::string*) const override;
    [[nodiscard]] bool WritePayloadForAnalysis(spStream&,const spBaseObject&,std::string*) const override;
    [[nodiscard]] bool WritePayloadWithContextForAnalysis(spSerializerManager&,spStream&,const spBaseObject&,std::string*) const override;
private:
    spOcclusionVolume::SortDispatchForAnalysis sort_;
};
}
