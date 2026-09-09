#pragma once
// Original serializer class/RTTI; read slice only. Writer/index operations are
// explicitly unavailable until their own reconstruction is connected.
// TU path proven by PC executable source diagnostics.
#include "spSerializer.h"
namespace sparkplug::reconstruction
{
    class spPartitionNode;
    class spPartitionNodeSerializer : public spSerializer
    {
    public:
        static constexpr spClassID ClassID=0x452AB84A;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        bool ReadPayloadForAnalysis(spSerializerReadContextForAnalysis&,spStream&,std::uint32_t,spBaseObject&,std::string*) const override;
        bool WritePayloadForAnalysis(spStream& stream,const spBaseObject& object,std::string* error) const override
        {return spSerializer::WritePayloadForAnalysis(stream,object,error);}
        bool WritePayloadWithContextForAnalysis(spSerializerManager& manager,spStream& stream,const spBaseObject& object,std::string* error) const override
        {return spSerializer::WritePayloadWithContextForAnalysis(manager,stream,object,error);}
        bool IndexRelationshipsWithContextForAnalysis(spSerializerManager&,spBaseObject&) const override{return false;}
    protected:
        bool ReadPartitionFieldsForAnalysis(spSerializerReadContextForAnalysis&,spStream&,
            std::uint32_t,spPartitionNode&,bool requireExactEnd,std::string*) const;
    };
}
