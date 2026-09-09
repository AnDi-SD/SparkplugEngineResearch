#include "spMeshNavigationSetSerializer.h"
#include "spMeshNavigationSet.h"
#include "Analysis/PC/spSpatialReadSupport.h"
namespace sparkplug::reconstruction
{
    using namespace evidence::pc::serialization;
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spMeshNavigationSetSerializer>();}
        const spRTTIRecord Record{spMeshNavigationSetSerializer::ClassID,spNavigationSetSerializer::ClassID,"spMeshNavigationSetSerializer",&spNavigationSetSerializer::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
    }
    const spRTTIRecord& spMeshNavigationSetSerializer::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spMeshNavigationSetSerializer::vfunc_18() const noexcept{return Record;}
    bool spMeshNavigationSetSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,spStream& source,std::uint32_t size,spBaseObject& object,std::string* error) const
    {
        auto* node=dynamic_cast<spMeshNavigationSet*>(&object);std::uint32_t start=0,remaining=0;
        if(!node||!object.IsExactly(spMeshNavigationSet::ClassID)||!source.GetCurrentPosition(start)||
            !ReadNavigationSetFieldsForAnalysis(context,source,size,*node,false,error)||!RemainingSection(source,start,size,remaining))
        {context.failed=true;if(error&&error->empty())*error="Missing MeshNavigationSet section";return false;}
        SectionCursor cursor(context,source,remaining,true,error);
        while(const auto* header=cursor.Next())
        {
            if(header->IsTerminator())return true;
            if(header->fieldID==0)
            {
                auto* mesh=ReadFieldReferenceForAnalysis(context,spMeshBV::ClassID,source,*header,error);
                if(context.failed||!node->SetMeshForAnalysis(std::dynamic_pointer_cast<spMeshBV>(context.ShareObjectForAnalysis(mesh))))
                    return cursor.Fail("Navigation mesh is null, unowned, or exceeds represented finite geometry");
            }
            else if(!cursor.Skip())return cursor.Fail("Cannot skip MeshNavigationSet field");
        }
        return false;
    }
}
