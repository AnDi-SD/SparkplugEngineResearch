#pragma once
// Host envelope/ownership guards shared by actual spatial serializers below.
// These helpers are analytical names, not additional recovered game classes.
#include "spSectionCursor.h"
#include <array>
#include <vector>
namespace sparkplug::evidence::pc::serialization
{
    inline bool RemainingSection(reconstruction::spStream& source,std::uint32_t start,
        std::uint32_t size,std::uint32_t& remaining)
    {
        std::uint32_t position=0;
        if(!source.GetCurrentPosition(position)||position<start||position-start>=size)return false;
        remaining=size-(position-start);return true;
    }
    inline bool ReadSpatialPolygon(reconstruction::spStream& source,
        const reconstruction::spDataBlockHeaderForAnalysis& header,
        std::vector<std::array<float,3>>& points)
    {
        std::uint32_t count=0;
        if(header.payloadSize<4||!source.Read(count)||count>4096||header.payloadSize!=4+count*12)return false;
        points.resize(count);
        for(auto& point:points)if(!source.ReadData(point.data(),12))return false;
        return true;
    }
    template<class T> std::unique_ptr<T> TakeSpatialOwner(
        reconstruction::spSerializerReadContextForAnalysis& context,
        reconstruction::spBaseObject* object,const reconstruction::spBaseObject& owner,std::string* error)
    {
        auto* typed=dynamic_cast<T*>(object);
        if(!typed){context.failed=true;if(error)*error="Wrong type for direct spatial owner";return nullptr;}
        auto value=context.TakeDirectOwnerForAnalysis(object,&owner,error);
        if(!value)return nullptr;
        value.release();return std::unique_ptr<T>(typed);
    }
}
