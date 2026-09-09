// Font data slice: original PC442660 and factory462EC0. Input-capture mode
// compares the actual reader to a bounded original guest, including raw UV bits.
#include "Code/Sparkplug/spFont.h"
#include "Code/Sparkplug/spFontSerializer.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/Sparkplug/spDataBlockSerializer.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace {
using Bytes=std::vector<std::uint8_t>;
int checks=0;
void Check(bool ok,const char* text) { ++checks;if(!ok)throw std::runtime_error(text); }
void Open(spMemoryStream& stream,const Bytes& bytes) {
    Check(stream.ResizeAndSetSize(static_cast<std::uint32_t>(bytes.size())),"memory stream");
    if(!bytes.empty())std::memcpy(stream.GetBuffer(),bytes.data(),bytes.size());
    Check(stream.Seek(spStream::SeekSource::essStart,0),"rewind");
}
bool Read(const Bytes& bytes,spFont& font) {
    spMemoryStream stream;Open(stream,bytes);spSerializerManager manager;spResourceManager resources;
    spSerializerReadContextForAnalysis context(manager,resources);std::string error;
    const auto ok=spFontSerializer{}.ReadPayloadForAnalysis(context,stream,static_cast<std::uint32_t>(bytes.size()),font,&error);
    Check(ok!=context.failed,"reader/context status agree");return ok;
}
Bytes Field(std::uint32_t field,const Bytes& payload) {
    spMemoryStream stream;Open(stream,{});spDataBlockSerializer blocks;
    Check(blocks.BeginObjectForAnalysis(stream,nullptr)&&blocks.WriteFieldForAnalysis(stream,field,payload.data(),
        static_cast<std::uint32_t>(payload.size()))&&blocks.FinalizeObjectForAnalysis(),"field fixture");
    std::uint32_t size=0;Check(stream.GetSize(&size),"size");
    const auto* bytes=static_cast<const std::uint8_t*>(stream.GetBuffer());return {bytes,bytes+size};
}
Bytes GlyphPayload() {
    Bytes bytes(4+8+spFont::GlyphCount*17);std::uint32_t metrics[]={0xFFFFFFFFu,0xFFFFFFFEu};
    std::memcpy(bytes.data()+4,metrics,8);
    const std::uint32_t uv[]={0x7FC12345u,0xFF800000u,0x80000000u,0x7F800000u};
    for(std::size_t i=0;i<spFont::GlyphCount;++i) {bytes[12+i*17]=static_cast<std::uint8_t>(255-i);std::memcpy(bytes.data()+13+i*17,uv,16);}
    return bytes;
}
void WriteState(const spFont& font,const char* file) {
    Check(font.GetBaselineForAnalysis().has_value(),"captured baseline assigned");
    std::ofstream output(file,std::ios::binary);const auto h=font.GetHeightForAnalysis(),b=*font.GetBaselineForAnalysis();
    output.write(reinterpret_cast<const char*>(&h),4);output.write(reinterpret_cast<const char*>(&b),4);
    for(const auto& glyph:font.GetGlyphsForAnalysis()) {
        output.write(reinterpret_cast<const char*>(&glyph.width),1);
        output.write(reinterpret_cast<const char*>(glyph.uv0.data()),8);output.write(reinterpret_cast<const char*>(glyph.uv1.data()),8);
    }
    Check(bool(output),"capture output");
}
}
int main(int argc,char** argv) {
    try {
        if(argc==3) {
            std::ifstream input(argv[1],std::ios::binary);Check(bool(input),"capture input");
            Bytes bytes{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
            Check(bytes.size()<=16384,"capture bound");spFont font;Check(Read(bytes,font),"captured reader");
            WriteState(font,argv[2]);std::cout<<"Font capture passed\n";return 0;
        }
        spFont font;
        Check(font.GetHeightForAnalysis()==0&&!font.GetBaselineForAnalysis()&&!font.GetImageForAnalysis(),"factory metrics/image");
        Check(Read({0},font)&&!font.GetBaselineForAnalysis(),"omitted baseline remains unknown");
        Check(Read(Field(7,{11,22,33}),font)&&!font.GetBaselineForAnalysis(),"unknown field skipped");
        const auto payload=GlyphPayload();const auto field=Field(0,payload);
        Check(Read(field,font)&&font.GetHeightForAnalysis()==0xFFFFFFFFu&&font.GetBaselineForAnalysis()==0xFFFFFFFEu,"raw UInt32 metrics");
        const auto& glyph=font.GetGlyphsForAnalysis().front();
        Check(glyph.width==255&&std::memcmp(glyph.uv0.data(),payload.data()+13,8)==0&&
            std::memcmp(glyph.uv1.data(),payload.data()+21,8)==0,"raw nonfinite UV bits");
        Bytes repeated=Field(0,Bytes(payload.size(),0));repeated.pop_back();repeated.insert(repeated.end(),field.begin(),field.end());
        Check(Read(repeated,font)&&font.GetHeightForAnalysis()==0xFFFFFFFFu&&font.GetGlyphsForAnalysis().back().width==32,"repeated assignment");
        auto truncated=field;truncated.erase(truncated.end()-2);
        Check(!Read(truncated,font),"truncated glyph/terminator rejected");
        spMemoryStream stream;Open(stream,repeated);spFont partial;spFontSerializer::InspectionForAnalysis observed;std::string error;
        Check(spFontSerializer{}.InspectPayloadForAnalysis(stream,static_cast<std::uint32_t>(repeated.size()),partial,observed,&error),"shared inspection");
        Check(observed.hasImage&&observed.image.id==0&&observed.image.size==4&&partial.GetHeightForAnalysis()==0xFFFFFFFFu,"NULL reference observed");
        std::cout<<"Font serialization: "<<checks<<" checks passed\n";return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
