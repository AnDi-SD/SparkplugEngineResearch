#include "spZonePortalSerializer.h"
#include "spZonePortal.h"
#include "spZone.h"
#include "Analysis/PC/spSpatialReadSupport.h"
namespace sparkplug::reconstruction
{
    using namespace evidence::pc::serialization;
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spZonePortalSerializer>();}
        const spRTTIRecord Record{spZonePortalSerializer::ClassID,spSerializer::ClassID,"spZonePortalSerializer",&spSerializer::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spZonePortalSerializer::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spZonePortalSerializer::vfunc_18() const noexcept{return Record;}
    std::unique_ptr<spBaseObject> spZonePortalSerializer::vfunc_10(spCloneManager&) const
    {
        // Serializer cloning is outside this read slice; no guessed clone body.
        return nullptr;
    }

bool spZonePortalSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
    spStream& source,std::uint32_t size,spBaseObject& object,std::string* error) const
{
    auto* portal=dynamic_cast<spZonePortal*>(&object);
    if(!portal||!object.IsExactly(spZonePortal::ClassID))
    {context.failed=true;if(error)*error="ZonePortal target mismatch";return false;}
    SectionCursor cursor(context,source,size,true,error);
    while(const auto* header=cursor.Next())
    {
        if(header->IsTerminator())return true;
        switch(header->fieldID)
        {
        case 0:
        {
            auto* value=ReadFieldReferenceForAnalysis(context,spZone::ClassID,source,*header,error);
            if(context.failed)return false;
            auto* zone=dynamic_cast<spZone*>(value);
            if(value&&!zone)return cursor.Fail("Wrong ZonePortal destination type");
            portal->SetDestinationForAnalysis(zone);break; // NULL is allowed by actual44DF2F
        }
        case 1:
        {
            std::vector<spZonePortal::Vector3> points;
            if(!ReadSpatialPolygon(source,*header,points)||!portal->SetPolygonForAnalysis(points))
                return cursor.Fail("Portal polygon exceeds represented finite geometry slice");
            break;
        }
        case 2:
        {
            std::uint8_t open=0;
            if(!cursor.Read(open))return cursor.Fail("Invalid ZonePortal Open byte");
            portal->SetOpenByteForAnalysis(open);break;
        }
        default:if(!cursor.Skip())return cursor.Fail("Cannot skip ZonePortal field");
        }
    }
    return false;
}
}
