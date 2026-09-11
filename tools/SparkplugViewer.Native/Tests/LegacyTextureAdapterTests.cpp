#include "../LegacyTextureAdapter.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/SparkplugPC/spPCRenderer.h"
#include "Code/SparkplugDX/spDXTexture.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace {
using Bytes=std::vector<std::uint8_t>;unsigned checks=0;
void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
void U32(Bytes& data,std::uint32_t value){const auto* bytes=reinterpret_cast<const std::uint8_t*>(&value);data.insert(data.end(),bytes,bytes+4);}
void Field(Bytes& data,std::uint8_t id,const Bytes& value){data.push_back(0xe0|id);U32(data,static_cast<unsigned>(value.size()));data.insert(data.end(),value.begin(),value.end());}
Bytes Legacy(std::uint32_t format=0){
    Bytes pixels;for(auto word:{2u,2u,format,4u})U32(pixels,word);
    for(std::uint8_t value=1;value<=16;++value)pixels.push_back(value);
    Bytes cross;Field(cross,5,pixels);cross.push_back(0);Bytes legacy;Field(legacy,0,cross);legacy.push_back(0);return legacy;
}
Bytes Run(const Bytes& payload,bool useAdapter,bool expected,bool compatibility){
    Bytes source(12,0x5a);source.insert(source.end(),payload.begin(),payload.end());source.insert(source.end(),8,0xa5);
    const auto original=source;spvhost::BorrowedInput stream(source.data(),static_cast<unsigned>(source.size()));
    stream.SetLogicalOriginForAnalysis(7);Check(stream.Seek(spStream::SeekSource::essStart,5),"Logical-origin fixture");
    spSerializerManager serializers;spResourceManager resources;spPCRenderer renderer;
    spSerializerReadContextForAnalysis context(serializers,resources);context.pcRenderer=&renderer;context.currentFileReadObjectIdForAnalysis=73;
    std::vector<std::uint32_t> adapted;spvhost::LegacyTextureAdapter adapter(adapted);spTextureDataSerializer strict;spDXTexture texture;std::string error;
    const auto& reader=useAdapter?static_cast<const spTextureDataSerializer&>(adapter):strict;
    const bool ok=reader.ReadPayloadForAnalysis(context,stream,static_cast<unsigned>(payload.size()),texture,&error);
    if(ok!=expected)std::cerr<<"Unexpected adapter result: "<<error<<'\n';
    Check(ok==expected,"Declared compatibility/strict outcome");Check(source==original,"Parent file bytes unchanged");
    Check(adapted==(compatibility?std::vector<std::uint32_t>{73}:std::vector<std::uint32_t>{}),"Only successfully adapted object IDs reported");
    if(!ok){Check(!texture.IsInitializedForAnalysis()&&!error.empty(),"Failed input is not an invented texture");return {};}
    std::uint32_t position=0;Check(stream.GetCurrentPosition(position)&&position==5+payload.size(),"Parent logical cursor consumes exactly original extent");
    Check(texture.IsInitializedForAnalysis()&&texture.GetWidthForAnalysis()==2&&texture.GetHeightForAnalysis()==2
        &&texture.GetMipsForAnalysis().size()==2,"Original common initializer builds complete CPU runtime mips");
    const auto& base=texture.GetMipsForAnalysis()[0].packedBytes;const std::uint8_t expectedPixels[]{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
    Check(base.size()==16&&std::memcmp(base.data(),expectedPixels,16)==0,"Stored base pixels preserved");
    Bytes result;for(const auto& mip:texture.GetMipsForAnalysis()){
        const auto* first=reinterpret_cast<const std::uint8_t*>(mip.packedBytes.data());result.insert(result.end(),first,first+mip.packedBytes.size());}return result;
}
}
int main(){try{
    const auto legacy=Legacy();const auto accepted=Run(legacy,true,true,true);Run(legacy,false,false,false);
    Bytes wrapped{0x22,0,0};wrapped.insert(wrapped.end(),legacy.begin(),legacy.end());
    Check(Run(wrapped,true,true,false)==accepted&&Run(wrapped,false,true,false)==accepted,"Explicit wrapper and tool adapter share one pixel/mip implementation");
    auto bad=legacy;bad.pop_back();Run(bad,true,false,false);
    bad=legacy;bad.push_back(0);Run(bad,true,false,false);
    bad=legacy;bad[1]=0xff;bad[2]=bad[3]=bad[4]=0xff;Run(bad,true,false,false);
    Run(Legacy(99),true,false,false);Run({0},true,false,false);
    std::cout<<"PASS "<<checks<<" checks: explicit legacy texture compatibility adapter\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL after "<<checks<<": "<<error.what()<<'\n';return 1;}}
