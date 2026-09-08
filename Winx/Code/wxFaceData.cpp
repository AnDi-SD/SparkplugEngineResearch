#include "wxFaceData.h"
namespace winx::reconstruction {
using namespace sparkplug::reconstruction;
namespace {
std::unique_ptr<spBaseObject> Create(){return std::make_unique<wxFaceData>();}
const spRTTIRecord Record{wxFaceData::ClassID,spCustomAppData::ClassID,"wxFaceData",
    &spCustomAppData::StaticRTTI(),&Create,nullptr};
const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
}
const spRTTIRecord& wxFaceData::StaticRTTI() noexcept{(void)Registered;return Record;}
const spRTTIRecord& wxFaceData::vfunc_18() const noexcept{return Record;}
std::unique_ptr<spBaseObject> wxFaceData::vfunc_10(spCloneManager& manager) const {
    auto clone=std::make_unique<wxFaceData>();manager.RegisterCloneForAnalysis(*this,*clone);
    return vfunc_14(*clone,manager)?std::move(clone):nullptr;
}
void wxFaceData::CopyFromForAnalysis(const spCustomAppData& source) {
    // PC5A4DE0 dispatches only for wxFaceData;5A4E10 copies exactly these fields.
    if(const auto* face=dynamic_cast<const wxFaceData*>(&source)) {
        surfaceType_=face->surfaceType_;flags_=face->flags_;surfaceID_=face->surfaceID_;
    }
}
bool wxFaceData::ReadForAnalysis(spStream& stream,std::uint32_t maximumBytes) {
    surfaceType_=surfaceID_=0;flags_=0;serializedFieldMask_=0;
    std::uint32_t consumed=0;
    const auto read=[&](void* out,std::uint32_t size) {
        if(size>maximumBytes-consumed||!stream.ReadData(out,size))return false;
        consumed+=size;return true;
    };
    const auto small=[&](std::uint32_t& value) {
        value=0;
        for(unsigned shift=0;shift<35;shift+=7) {
            std::uint8_t byte=0;if(!read(&byte,1))return false;
            if(shift==28&&(byte&0xf0))return false; // bounded host UInt32 guard
            value|=std::uint32_t(byte&127)<<shift;
            if(!(byte&128))return true;
        }
        return false;
    };
    for(std::uint32_t fields=0;fields<65536;++fields) {
        std::uint32_t id=0,size=0;
        if(!small(id))return false;if(!id)return true;
        if(!small(size)||size>maximumBytes-consumed)return false;
        // Native known members read their fixed width regardless of advertised
        // size. Preserve that behavior while keeping the enclosing byte bound.
        if(id==1){if(!read(&surfaceType_,1))return false;serializedFieldMask_|=1;}
        else if(id==2){if(!read(&flags_,2))return false;serializedFieldMask_|=2;}
        else if(id==3){if(!read(&surfaceID_,1))return false;serializedFieldMask_|=4;}
        else {
            if(size>0x7fffffff||!stream.Seek(spStream::SeekSource::essCurrent,static_cast<std::int32_t>(size)))return false;
            consumed+=size;
        }
    }
    return false;
}
bool wxFaceData::WriteForAnalysis(spStream& stream) const {
    // Native writer independently suppresses each zero member, then terminates.
    if(surfaceType_&&(!stream.Write(std::uint8_t{1})||!stream.Write(std::uint8_t{1})||!stream.Write(surfaceType_)))return false;
    if(flags_&&(!stream.Write(std::uint8_t{2})||!stream.Write(std::uint8_t{2})||!stream.Write(flags_)))return false;
    if(surfaceID_&&(!stream.Write(std::uint8_t{3})||!stream.Write(std::uint8_t{1})||!stream.Write(surfaceID_)))return false;
    return stream.Write(std::uint8_t{0});
}
}
