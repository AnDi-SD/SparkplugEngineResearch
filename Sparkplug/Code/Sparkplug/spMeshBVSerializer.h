#pragma once
#include "spSerializer.h"
namespace sparkplug::reconstruction {
class spCollisionMesh;
class spIndexBuffer;
class spVertexBuffer;
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
    // The geometry branch of PC438490, shared with tools inspecting that field.
    // Optional position is a host observation, relative to the field payload.
    static std::unique_ptr<spCollisionMesh> ReadGeometryForAnalysis(
        spStream&,std::uint32_t,std::uint32_t* vertexPayloadOffset=nullptr,std::string* error=nullptr);
    // Fresh-owner assignment from PC438604/438607. Portable access facade,
    // not an additional original virtual method or collision query algorithm.
    static std::unique_ptr<spCollisionMesh> CreateGeometryForAnalysis(
        std::unique_ptr<spIndexBuffer>,std::unique_ptr<spVertexBuffer>);
    bool IndexRelationshipsWithContextForAnalysis(spSerializerManager&,spBaseObject& object) const override{return object.IsExactly(TargetClassID);}
};
}
