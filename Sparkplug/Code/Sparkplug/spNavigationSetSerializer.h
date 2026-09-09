#pragma once
#include "spNodeSerializer.h"
namespace sparkplug::reconstruction
{
    class spNavigationSet;
    class spNavigationSetSerializer : public spNodeSerializer
    {
    public:
        static constexpr spClassID ClassID=0x5C2C4113;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override{return nullptr;}
        [[nodiscard]] spClassID GetTargetClassIDForAnalysis() const noexcept override{return 0x74F9013E;}
        bool ReadPayloadForAnalysis(spSerializerReadContextForAnalysis&,spStream&,std::uint32_t,spBaseObject&,std::string*) const override;
        bool WritePayloadForAnalysis(spStream& stream,const spBaseObject& object,std::string* error) const override
        {return spSerializer::WritePayloadForAnalysis(stream,object,error);}
        bool WritePayloadWithContextForAnalysis(spSerializerManager& manager,spStream& stream,const spBaseObject& object,std::string* error) const override
        {return spSerializer::WritePayloadWithContextForAnalysis(manager,stream,object,error);}
        bool IndexRelationshipsWithContextForAnalysis(spSerializerManager&,spBaseObject&) const override{return false;}
    protected:
        bool ReadNavigationSetFieldsForAnalysis(spSerializerReadContextForAnalysis&,spStream&,std::uint32_t,spNavigationSet&,bool exact,std::string*) const;
    };
}
