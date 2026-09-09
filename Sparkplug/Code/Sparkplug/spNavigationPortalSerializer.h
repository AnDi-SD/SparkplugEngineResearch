#pragma once
#include "spNodeSerializer.h"
namespace sparkplug::reconstruction
{
    class spNavigationSet;
    class spNavigationPortalSerializer : public spNodeSerializer
    {
    public:
        static constexpr spClassID ClassID=0x33695A39;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override{return nullptr;}
        [[nodiscard]] spClassID GetTargetClassIDForAnalysis() const noexcept override{return 0x385662AA;}
        bool ReadPayloadForAnalysis(spSerializerReadContextForAnalysis&,spStream&,std::uint32_t,spBaseObject&,std::string*) const override;
        bool WritePayloadForAnalysis(spStream& stream,const spBaseObject& object,std::string* error) const override
        {return spSerializer::WritePayloadForAnalysis(stream,object,error);}
        bool WritePayloadWithContextForAnalysis(spSerializerManager& manager,spStream& stream,const spBaseObject& object,std::string* error) const override
        {return spSerializer::WritePayloadWithContextForAnalysis(manager,stream,object,error);}
        bool IndexRelationshipsWithContextForAnalysis(spSerializerManager&,spBaseObject&) const override{return false;}
    };
}
