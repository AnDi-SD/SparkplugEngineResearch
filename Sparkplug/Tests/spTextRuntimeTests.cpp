#include "Code/Sparkplug/spTextNodeSerializer.h"
#include "Code/Sparkplug/spTextRenderable.h"
#include "Code/Sparkplug/spTextRenderableSerializer.h"
#include "Code/Sparkplug/spFont.h"
#include "Code/Sparkplug/spFontSerializer.h"
#include "Code/Sparkplug/spFontManager.h"
#include "Code/SparkplugDX/spDXTexture.h"
#include "Code/Sparkplug/spTextureData.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/Sparkplug/spResourceFATSerializer.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace {
using Bytes=std::vector<std::uint8_t>;
unsigned checks=0,originalWords=0;
void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
std::uint32_t Bits(float value){std::uint32_t result;std::memcpy(&result,&value,4);return result;}
struct Reader {
    std::ifstream file;
    explicit Reader(const std::filesystem::path& path):file(path,std::ios::binary){Check(bool(file),"Open qualified fixture");}
    std::uint32_t U32(){std::uint32_t value=0;file.read(reinterpret_cast<char*>(&value),4);Check(bool(file),"Fixture uint32 extent");return value;}
    Bytes Blob(){const auto size=U32();Check(size<=65536,"Fixture byte limit");Bytes result(size);if(size)file.read(reinterpret_cast<char*>(result.data()),size);Check(bool(file),"Fixture byte extent");return result;}
};
void Open(spMemoryStream& stream,const Bytes& bytes){
    Check(stream.ResizeAndSetSize(static_cast<std::uint32_t>(bytes.size())),"Memory extent");
    if(!bytes.empty())std::memcpy(stream.GetBuffer(),bytes.data(),bytes.size());
    Check(stream.Seek(spStream::SeekSource::essStart,0),"Memory rewind");
}
template<class T>void Add(Bytes& bytes,T value){const auto* begin=reinterpret_cast<const std::uint8_t*>(&value);bytes.insert(bytes.end(),begin,begin+sizeof(value));}
void Match(std::uint32_t actual,std::uint32_t expected,const char* label){
    if(actual!=expected)std::cerr<<label<<" actual "<<std::hex<<actual<<" expected "<<expected<<std::dec<<'\n';
    Check(actual==expected,label);++originalWords;
}
void State(Reader& capture,const spTextRenderable& text,const spFontManager& manager){
    std::vector<std::uint32_t> words{text.GetColorForAnalysis(),text.GetWrapWidthForAnalysis(),
        text.GetAlignmentForAnalysis(),text.GetMeasuredWidthForAnalysis()};
    for(float value:text.GetBoundingSphereForAnalysis())words.push_back(Bits(value));
    Check(text.GetMinimumForAnalysis().has_value()&&text.GetMaximumForAnalysis().has_value(),"Captured bounds have defined native assignments");
    for(float value:*text.GetMinimumForAnalysis())words.push_back(Bits(value));
    for(float value:*text.GetMaximumForAnalysis())words.push_back(Bits(value));
    words.push_back(manager.GetSelectedColorForAnalysis());
    for(auto value:words)Match(value,capture.U32(),"Original text word");
}
void Run(const std::filesystem::path& path){
    Reader capture(path);Check(capture.U32()==0x31545854,"TXT1 fixture");
    spSerializerManager serializers;spResourceManager resources;spFontManager layout;
    spSerializerReadContextForAnalysis context(serializers,resources);context.fontManager=&layout;
    auto font=std::make_shared<spFont>();spMemoryStream input;std::string error;
    auto bytes=capture.Blob();Open(input,bytes);
    Check(spFontSerializer{}.ReadPayloadForAnalysis(context,input,static_cast<unsigned>(bytes.size()),*font,&error),"Actual shared Font reader");
    const auto measures=capture.U32();Check(measures==9,"Nine original measure paths");
    for(unsigned i=0;i<measures;++i){
        const auto present=capture.U32(),wrap=capture.U32(),width=capture.U32(),expectedHeight=capture.U32();
        bytes=capture.Blob();bytes.push_back(0);std::uint32_t height=0xdeadbeef;
        Match(font->MeasureTextForAnalysis(present?reinterpret_cast<const char*>(bytes.data()):nullptr,wrap,&height),width,"Original measured width");
        Match(height,expectedHeight,"Original measured height/unwritten sentinel");
    }
    layout.SetLayoutDefaultFontForAnalysis(font.get());
    // Explicit pre-existing FAT object, matching the original probe's reference
    // leaf. Common ReadReference resolves/owns it; no native startup claim.
    Bytes directory;Add(directory,1u);Add(directory,0x1234u);Add(directory,std::uint16_t(0));
    Add(directory,spFont::ClassID);Add(directory,0u);Add(directory,0u);Open(input,directory);
    Check(serializers.GetFATForAnalysis()->LoadIndexForAnalysis(input),"Fixture FAT directory");
    serializers.GetFATForAnalysis()->FindByIDForAnalysis(0x1234)->object=font.get();context.externalOwners.push_back(font);
    auto text=std::make_shared<spTextRenderable>();
    Check(!text->GetTextForAnalysis()&&!text->GetMinimumForAnalysis()&&!text->GetMaximumForAnalysis(),"Original factory string/bounds absence");
    const auto reads=capture.U32();Check(reads==4,"Four sequential original reader states");
    for(unsigned i=0;i<reads;++i){
        bytes=capture.Blob();Open(input,bytes);
        if(!spTextRenderableSerializer{}.ReadPayloadForAnalysis(context,input,static_cast<unsigned>(bytes.size()),*text,&error))
            throw std::runtime_error(error);
        std::uint32_t position=0;Check(input.GetCurrentPosition(position)&&position==bytes.size(),"Text reader exact extent");
        Check(text->GetFontForAnalysis()==font,"Canonical Font ownership");State(capture,*text,layout);
    }
    const auto layouts=capture.U32();Check(layouts==8,"Eight original final layout states");
    for(unsigned i=0;i<layouts;++i){
        const auto wrap=capture.U32(),alignment=capture.U32();bytes=capture.Blob();bytes.push_back(0);
        // Capture seeds wrap/alignment before SetText; public setters reproduce
        // final state. SetText first also preserves original empty max bounds.
        Check(text->SetTextForAnalysis(reinterpret_cast<const char*>(bytes.data()),&layout,&error)
            &&text->SetWrapWidthForAnalysis(wrap,&layout,&error)
            &&text->SetAlignmentForAnalysis(alignment,&layout,&error),"Shared text layout calls");
        State(capture,*text,layout);
    }
    spTextNode node;spRenderNode& dispatch=node;auto second=std::make_shared<spTextRenderable>();
    Check(!node.GetTextRenderableForAnalysis()&&node.GetRenderableCountForAnalysis()==0,"TextNode own defaults");
    Check(text->SetTextForAnalysis("AB",&layout,&error),"Text sphere before node insertion");
    const auto nodes=capture.U32();Check(nodes==6,"Six original TextNode ownership transitions");
    for(unsigned i=0;i<nodes;++i){
        if(i<2)Check(dispatch.AttachRenderableForAnalysis(text),"Polymorphic duplicate Text attach");
        else if(i==2)Check(dispatch.AttachRenderableForAnalysis(second),"Polymorphic replacement Text cache");
        else if(i==3)Check(dispatch.DetachRenderableForAnalysis(*text)==text,"Detach one old slot");
        else if(i==4)Check(dispatch.DetachRenderableForAnalysis(*second)==second,"Detach cached Text");
        else dispatch.ClearRenderablesForAnalysis();
        Match(static_cast<unsigned>(node.GetRenderableCountForAnalysis()),capture.U32(),"Original node slot count");
        Match(!node.GetTextRenderableForAnalysis()?0:node.GetTextRenderableForAnalysis()==text.get()?1:2,capture.U32(),"Original independent cached Text identity");
        Match(static_cast<unsigned>(text.use_count()),capture.U32(),"Original Text reference multiplicity");
        Match(static_cast<unsigned>(second.use_count()),capture.U32(),"Original second Text multiplicity");
        for(float value:node.GetLocalBoundingSphereForAnalysis())Match(Bits(value),capture.U32(),"Original node aggregate sphere");
    }
    Check(capture.file.peek()==std::char_traits<char>::eof(),"Fixture exact end");
    Check(!text->SetTextForAnalysis(nullptr,&layout,&error)&&!error.empty(),"Explicit guard for original NULL dereference");
    spTextRenderable cold;Check(!cold.SetTextForAnalysis("A",nullptr,&error),"No invented manager");
    spFontManager emptyManager;Check(!cold.RebuildLayoutForAnalysis(&emptyManager,&error),"No invented default Font");
    Check(cold.SetTextForAnalysis("",nullptr,&error)&&cold.GetMinimumForAnalysis()&&!cold.GetMaximumForAnalysis(),"Empty layout preserves unknown maximum");
    spTextNodeSerializer textNodeSerializer;
    Check(textNodeSerializer.IsExactly(0x46253465)&&textNodeSerializer.GetTargetClassIDForAnalysis()==spTextNode::ClassID,"Original TextNode serializer identity");
    spMemoryStream out;Open(out,{});
    if(!textNodeSerializer.WritePayloadForAnalysis(out,node,&error))throw std::runtime_error(error.empty()?"TextNode inherited writer refused":error);
    std::uint32_t size=0;Check(out.GetSize(&size)&&out.Seek(spStream::SeekSource::essStart,0),"TextNode writer output");
    spTextNode restored;Check(textNodeSerializer.ReadPayloadForAnalysis(context,out,size,restored,&error),"TextNode shared inherited reader");
    {
        // Original atlas-original-run2: actual runtime DXTexture, owning Font
        // reference0->1, Font deletion releases that exact atlas object.
        spSerializerManager atlasManager;spResourceManager atlasResources;
        (void)spTextureData::StaticRTTI(); // retain the wire-class TU in this static-library test
        spSerializerReadContextForAnalysis atlasContext(atlasManager,atlasResources);
        auto atlas=std::make_shared<spDXTexture>();std::weak_ptr<spDXTexture> weak=atlas;
        Bytes fat;Add(fat,1u);Add(fat,0x2345u);Add(fat,std::uint16_t(0));
        Add(fat,0x78ea082bu);Add(fat,0u);Add(fat,0u);Open(input,fat);
        Check(atlasManager.GetFATForAnalysis()->LoadIndexForAnalysis(input),"Atlas fixture FAT");
        atlasManager.GetFATForAnalysis()->FindByIDForAnalysis(0x2345)->object=atlas.get();
        atlasContext.externalOwners.push_back(atlas);
        Bytes payload;Add(payload,0x2345u);Add(payload,0u);Add(payload,12u);Add(payload,3u);payload.resize(payload.size()+224*17);
        Bytes wire{0xe0};Add(wire,static_cast<unsigned>(payload.size()));wire.insert(wire.end(),payload.begin(),payload.end());wire.push_back(0);Open(input,wire);
        auto atlasFont=std::make_unique<spFont>();
        Check(spFontSerializer{}.ReadPayloadForAnalysis(atlasContext,input,static_cast<unsigned>(wire.size()),*atlasFont,&error),"Font accepts canonical runtime DXTexture atlas");
        Check(atlasFont->GetImageForAnalysis()==atlas.get(),"Original atlas pointer assignment");
        atlasContext.externalOwners.clear();atlas.reset();Check(!weak.expired()&&weak.use_count()==1,"Font alone owns atlas");
        atlasFont.reset();Check(weak.expired(),"Original Font releases runtime atlas");
    }
}
}
int main(int argc,char** argv){try{Run(argc==2?std::filesystem::path(argv[1]):std::filesystem::path(__FILE__).parent_path()/"Fixtures/text-runtime-pc.dat");
    std::cout<<"PASS "<<checks<<" checks, "<<originalWords<<" original words: Font/Text/TextNode CPU runtime\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL after "<<checks<<": "<<error.what()<<'\n';return 1;}}
