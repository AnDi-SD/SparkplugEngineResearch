#pragma once
// HOST compatibility policy, not reconstructed source-selection behavior.
// A legacy direct cross-pixel section is put behind an explicit SourceNone
// envelope, then consumed by the ONE existing common TextureData reader.
// Original input bytes/FAT extents are never rewritten. Strict graph loading
// does not install this adapter. Every accepted resource is reported by ID.
#include "BorrowedInput.h"
#include "Code/Sparkplug/spTextureDataSerializer.h"
#include "Code/Sparkplug/spDataBlockSerializer.h"
#include <algorithm>
namespace spvhost {
class LegacyTextureAdapter final : public sparkplug::reconstruction::spTextureDataSerializer {
    std::vector<std::uint32_t>& adapted_;
public:
    explicit LegacyTextureAdapter(std::vector<std::uint32_t>& adapted):adapted_(adapted){}
    bool ReadPayloadForAnalysis(sparkplug::reconstruction::spSerializerReadContextForAnalysis& context,
        sparkplug::reconstruction::spStream& source,std::uint32_t size,
        sparkplug::reconstruction::spBaseObject& object,std::string* error) const override {
        using namespace sparkplug::reconstruction;
        std::uint32_t start=0;
        if(size<2||size>16u*1024u*1024u||!source.GetCurrentPosition(start))
            return spTextureDataSerializer::ReadPayloadForAnalysis(context,source,size,object,error);
        // At most one16MiB temporary per common texture, reused as prefixed input.
        std::vector<std::uint8_t> input(std::size_t(size)+3);
        const bool read=source.ReadData(input.data()+3,size);
        if(!source.Seek(spStream::SeekSource::essStart,static_cast<std::int32_t>(start)))
        {context.failed=true;if(error)*error="Cannot restore compatibility probe cursor";return false;}
        bool direct=false;
        if(read){
            BorrowedInput probe(input.data()+3,size);spDataBlockSerializer blocks;
            const auto* first=blocks.ReadHeaderForAnalysis(probe);
            if(first&&first->fieldID==0&&!first->IsTerminator()&&first->payloadSize
                &&first->dataStreamPosition<size&&first->payloadSize==size-first->dataStreamPosition-1
                &&probe.Seek(spStream::SeekSource::essStart,static_cast<std::int32_t>(size-1))){
                const auto* last=blocks.ReadHeaderForAnalysis(probe);std::uint32_t end=0;
                direct=last&&last->IsTerminator()&&probe.GetCurrentPosition(end)&&end==size;
            }
        }
        if(!direct)return spTextureDataSerializer::ReadPayloadForAnalysis(context,source,size,object,error);
        // DataBlock field2, byte0, terminator. Original field0 pixels follow.
        input[0]=0x22;input[1]=input[2]=0;BorrowedInput adapted(input.data(),size+3);
        if(!spTextureDataSerializer::ReadPayloadForAnalysis(context,adapted,size+3,object,error))return false;
        if(!source.Seek(spStream::SeekSource::essStart,static_cast<std::int32_t>(start+size)))
        {context.failed=true;if(error)*error="Cannot advance accepted compatibility extent";return false;}
        const auto id=context.currentFileReadObjectIdForAnalysis;
        if(std::find(adapted_.begin(),adapted_.end(),id)==adapted_.end())adapted_.push_back(id);
        return true;
    }
};
}
