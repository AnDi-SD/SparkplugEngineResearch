#include "spNavigationSetSerializer.h"
#include "spNavigationSet.h"
#include "spNavigationPortal.h"
#include "Analysis/PC/spSpatialReadSupport.h"
namespace sparkplug::reconstruction
{
    using namespace evidence::pc::serialization;
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spNavigationSetSerializer>();}
        const spRTTIRecord Record{spNavigationSetSerializer::ClassID,spNodeSerializer::ClassID,"spNavigationSetSerializer",&spNodeSerializer::StaticRTTI(),&Create,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
        bool Matrix(spStream& source,const spDataBlockHeaderForAnalysis& header,spNavigationSet::Table& table)
        {
            std::uint32_t rows=0,columns=0;
            if(header.payloadSize<8||!source.Read(rows)||!source.Read(columns)||
                std::uint64_t(rows)*columns*4+8!=header.payloadSize||!table.Resize(rows,columns))return false;
            for(std::uint32_t row=0;row<rows;++row)for(std::uint32_t col=0;col<columns;++col)
            {std::uint32_t value=0;if(!source.Read(value))return false;table.Set(row,col,value);}
            return true;
        }
    }
    const spRTTIRecord& spNavigationSetSerializer::StaticRTTI() noexcept{(void)Registered;return Record;}
    const spRTTIRecord& spNavigationSetSerializer::vfunc_18() const noexcept{return Record;}
    bool spNavigationSetSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,spStream& source,std::uint32_t size,spBaseObject& object,std::string* error) const
    {
        auto* node=dynamic_cast<spNavigationSet*>(&object);
        if(!node||!object.IsExactly(spNavigationSet::ClassID)){context.failed=true;if(error)*error="NavigationSet target mismatch";return false;}
        return ReadNavigationSetFieldsForAnalysis(context,source,size,*node,true,error);
    }
    bool spNavigationSetSerializer::ReadNavigationSetFieldsForAnalysis(spSerializerReadContextForAnalysis& context,spStream& source,std::uint32_t size,spNavigationSet& node,bool exact,std::string* error) const
    {
        std::uint32_t start=0,remaining=0;
        if(!source.GetCurrentPosition(start)||!ReadNodeFieldsForAnalysis(context,source,size,node,false,error)||!RemainingSection(source,start,size,remaining))
        {context.failed=true;if(error&&error->empty())*error="Missing NavigationSet section";return false;}
        SectionCursor cursor(context,source,remaining,exact,error);
        while(const auto* header=cursor.Next())
        {
            if(header->IsTerminator())return true;
            if(header->fieldID==0)
            {std::uint32_t count=0;if(!cursor.Read(count)||!node.SetNodeCountForAnalysis(count))return cursor.Fail("Navigation node count exceeds host allocation limit");}
            else if(header->fieldID==1||header->fieldID==2)
            {
                auto table=std::make_unique<spNavigationSet::Table>();
                if(!Matrix(source,*header,*table))return cursor.Fail("Invalid bounded navigation transition matrix");
                (header->fieldID==1?node.transitions_:node.portalTransitions_)=std::move(table);
            }
            else if(header->fieldID==3)
            {
                std::uint32_t records=0,position=0;const auto end=header->dataStreamPosition+header->payloadSize;
                if(header->payloadSize<4||!source.Read(records)||records>65536)return cursor.Fail("Invalid navigation link record count");
                for(std::uint32_t i=0;i<records;++i)
                {
                    std::uint8_t id=0;std::uint32_t count=0;
                    if(!source.GetCurrentPosition(position)||position>end||end-position<5||!source.Read(id)||!source.Read(count)||count>end-position-5)
                        return cursor.Fail("Truncated navigation link record");
                    for(std::uint32_t j=0;j<count;++j)
                    {std::uint8_t neighbour=0;if(!source.Read(neighbour)||!node.AppendNeighbourForAnalysis(id,neighbour))return cursor.Fail("Navigation link source/degree exceeds safe native byte storage");}
                }
            }
            else if(header->fieldID==4)
            {
                auto* portal=dynamic_cast<spNavigationPortal*>(ReadFieldReferenceForAnalysis(context,spNavigationPortal::ClassID,source,*header,error));
                if(context.failed||!node.AppendPortalForAnalysis(portal))return cursor.Fail("NavigationSet portal is null or wrong type");
            }
            else if(header->fieldID==5)
            {if(!cursor.Read(node.enabled_))return cursor.Fail("Invalid navigation enabled byte");}
            else if(!cursor.Skip())return cursor.Fail("Cannot skip NavigationSet field");
        }
        return false;
    }
}
