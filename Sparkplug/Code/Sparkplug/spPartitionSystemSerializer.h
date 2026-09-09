#pragma once
// Original serializer class/RTTI; read slice only. Writer/index operations are
// explicitly unavailable until their own reconstruction is connected.
// TU path proven by PC executable source diagnostics.
#include "spRenderNodeSerializer.h"
namespace sparkplug::reconstruction
{
    class spPartitionSystem;
    class spPartitionSystemSerializer : public spRenderNodeSerializer
    {
    public:
        static constexpr spClassID ClassID=0x73CA603A;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        [[nodiscard]] spClassID GetTargetClassIDForAnalysis() const noexcept override;
        bool ReadPayloadForAnalysis(spSerializerReadContextForAnalysis&,spStream&,std::uint32_t,spBaseObject&,std::string*) const override;
        bool WritePayloadForAnalysis(spStream& stream,const spBaseObject& object,std::string* error) const override
        {return spSerializer::WritePayloadForAnalysis(stream,object,error);}
        bool WritePayloadWithContextForAnalysis(spSerializerManager& manager,spStream& stream,const spBaseObject& object,std::string* error) const override
        {return spSerializer::WritePayloadWithContextForAnalysis(manager,stream,object,error);}
        bool IndexRelationshipsWithContextForAnalysis(spSerializerManager&,spBaseObject&) const override{return false;}
    };
}
