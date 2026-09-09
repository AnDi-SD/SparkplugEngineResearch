#pragma once
// Original RTTI and PC44C8E0 read slice. TU path inferred; no original path
// string was found. Writer/index/clone remain outside this reconstruction.
#include "spPartitionNodeSerializer.h"
namespace sparkplug::reconstruction
{
    class spOctreeNode;
    class spOctreeNodeSerializer : public spPartitionNodeSerializer
    {
    public:
        static constexpr spClassID ClassID=0x05BC2A91;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        bool ReadPayloadForAnalysis(spSerializerReadContextForAnalysis&,spStream&,std::uint32_t,spBaseObject&,std::string*) const override;
        bool ReadOctreeFieldsForAnalysis(spSerializerReadContextForAnalysis&,spStream&,std::uint32_t,spOctreeNode&,std::string*) const;
        bool WritePayloadForAnalysis(spStream& stream,const spBaseObject& object,std::string* error) const override
        {return spSerializer::WritePayloadForAnalysis(stream,object,error);}
        bool WritePayloadWithContextForAnalysis(spSerializerManager& manager,spStream& stream,const spBaseObject& object,std::string* error) const override
        {return spSerializer::WritePayloadWithContextForAnalysis(manager,stream,object,error);}
        bool IndexRelationshipsWithContextForAnalysis(spSerializerManager&,spBaseObject&) const override{return false;}
    };
}
