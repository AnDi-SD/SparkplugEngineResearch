#include "Code/Sparkplug/spFontManager.h"
#include "Code/Sparkplug/spFont.h"
#include "Code/Sparkplug/spFontSerializer.h"
#include "Code/Sparkplug/spTextRenderable.h"
#include "Code/Sparkplug/spMaterialPassLayer.h"
#include "Code/Sparkplug/spMaterialTextureLayer.h"
#include "Code/Sparkplug/spMaterialTexture.h"
#include "Code/SparkplugDX/spDXMaterial.h"
#include "Code/SparkplugDX/spDXTexture.h"
#include "Code/Sparkplug/spResourceFATSerializer.h"
#include "Code/SparkplugPC/spPCFontManager.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace {
using Bytes=std::vector<std::uint8_t>;unsigned checks=0,originalBytes=0;
void Check(bool ok,const char* label){++checks;if(!ok)throw std::runtime_error(label);}
struct Reader {
    std::ifstream file;
    explicit Reader(const std::filesystem::path& path):file(path,std::ios::binary){Check(bool(file),"Open FTG1 fixture");}
    std::uint32_t U32(){std::uint32_t v=0;file.read(reinterpret_cast<char*>(&v),4);Check(bool(file),"Fixture uint extent");return v;}
    float Float(){const auto bits=U32();float value;std::memcpy(&value,&bits,4);return value;}
    Bytes Blob(){const auto n=U32();Check(n<=65536,"Fixture blob bound");Bytes value(n);if(n)file.read(reinterpret_cast<char*>(value.data()),n);Check(bool(file),"Fixture blob extent");return value;}
};
void Match(const void* actual,std::size_t size,const Bytes& expected,const char* message){
    Check(size==expected.size()&&(!size||std::memcmp(actual,expected.data(),size)==0),message);originalBytes+=static_cast<unsigned>(size);
}
void CustomMaterial(){
    Reader capture(std::filesystem::path(__FILE__).parent_path()/"Fixtures/text-material-pc.dat");
    Check(capture.U32()==0x314d5446,"FTM1 signature");
    spSerializerManager serializers;spResourceManager resources;spFontManager manager,custom;
    spSerializerReadContextForAnalysis context(serializers,resources);spMemoryStream stream;std::string error;
    auto atlas=std::make_shared<spDXTexture>();auto font=std::make_shared<spFont>();context.externalOwners.push_back(atlas);
    Bytes directory;auto append=[&](const auto value){const auto* first=reinterpret_cast<const std::uint8_t*>(&value);directory.insert(directory.end(),first,first+sizeof(value));};
    append(std::uint32_t(1));append(std::uint32_t(0x2345));append(std::uint16_t(0));append(spTexture::ClassID);append(std::uint32_t(0));append(std::uint32_t(0));
    auto open=[&](const Bytes& data){Check(stream.ResizeAndSetSize(static_cast<std::uint32_t>(data.size())),"Bounded input");
        std::memcpy(stream.GetBuffer(),data.data(),data.size());Check(stream.Seek(spStream::SeekSource::essStart,0),"Input rewind");};
    open(directory);Check(serializers.GetFATForAnalysis()->LoadIndexForAnalysis(stream),"Actual declared atlas FAT");
    serializers.GetFATForAnalysis()->FindByIDForAnalysis(0x2345)->object=atlas.get();
    auto wire=capture.Blob();open(wire);
    Check(spFontSerializer{}.ReadPayloadForAnalysis(context,stream,static_cast<std::uint32_t>(wire.size()),*font,&error)&&font->GetImageForAnalysis()==atlas.get(),"Actual Font atlas owner");
    manager.SetLayoutDefaultFontForAnalysis(font.get());
    Check(manager.InitializePCMaterialForAnalysis()&&custom.InitializePCMaterialForAnalysis(),"Actual primary and custom materials");
    auto material=custom.GetCurrentMaterialForAnalysis();auto* pass=dynamic_cast<spMaterialPassLayer*>(material->GetPassForAnalysis(0));
    const auto& holder=pass->GetLayerForAnalysis(0)->GetMaterialTextureForAnalysis();
    auto oldAtlas=std::make_shared<spDXTexture>();std::weak_ptr<spTexture> oldOwner=oldAtlas;
    holder->SetOwnedFallBackTextureForAnalysis(oldAtlas);oldAtlas.reset();
    spTextRenderable text;text.SetMaterialForAnalysis(material);
    const auto count=capture.U32();Check(count==4,"Four actual custom Text alignments");
    for(std::uint32_t i=0;i<count;++i){
        const auto alignment=capture.U32(),color=capture.U32();auto chars=capture.Blob();chars.push_back(0);
        const auto vb=capture.Blob(),ib=capture.Blob();
        Check(text.SetTextForAnalysis(reinterpret_cast<const char*>(chars.data()),&manager,&error)&&text.SetAlignmentForAnalysis(alignment,&manager,&error),"Shared custom Text layout");
        text.SetColorForAnalysis(color);
        Check(text.SelectPCDrawResourcesForAnalysis(manager,&error)&&manager.GetPrimaryMaterialForAnalysis()==material.get(),"Actual Text chooses its material");
        Check(manager.BindPCTextAtlasForAnalysis(&error)&&holder->GetTextureForAnalysis()==atlas.get()&&oldOwner.expired(),"Actual atlas replacement releases old owner");
        spFontManager::Text3DGeometryForAnalysis geometry;Check(text.BuildPCGeometryForAnalysis(manager,geometry,&error),"Custom Text geometry");
        Match(geometry.vertices.data(),geometry.vertices.size()*24,vb,"Original custom Text vertices");
        Match(geometry.indices.data(),geometry.indices.size()*2,ib,"Original custom Text triangles");
    }
    Check(capture.file.peek()==std::char_traits<char>::eof(),"FTM1 exact end");
    text.SetMaterialForAnalysis(nullptr);
    Check(text.SelectPCDrawResourcesForAnalysis(manager,&error)&&!manager.GetPrimaryMaterialForAnalysis()&&
        manager.GetCurrentMaterialForAnalysis().get()==manager.GetFallbackMaterialForAnalysis(),"NULL Text material selects actual fallback");
    Check(manager.BindPCTextAtlasForAnalysis(&error),"Fallback gets actual atlas too");
    manager.SetPrimaryMaterialForAnalysis(std::make_shared<spDXMaterial>());
    Check(!manager.BindPCTextAtlasForAnalysis(&error),"Missing first StdLayer is an explicit host guard");
}
void Run(){
    Reader capture(std::filesystem::path(__FILE__).parent_path()/"Fixtures/text-geometry-pc.dat");Check(capture.U32()==0x31475446,"FTG1 signature");
    auto font=std::make_shared<spFont>();spSerializerManager serializers;spResourceManager resources;spFontManager manager;
    spSerializerReadContextForAnalysis context(serializers,resources);spMemoryStream stream;auto wire=capture.Blob();std::string error;
    Check(stream.ResizeAndSetSize(static_cast<unsigned>(wire.size())),"Font stream allocation");std::memcpy(stream.GetBuffer(),wire.data(),wire.size());
    Check(spFontSerializer{}.ReadPayloadForAnalysis(context,stream,static_cast<unsigned>(wire.size()),*font,&error),"Actual shared Font reader");
    manager.SetLayoutDefaultFontForAnalysis(font.get());
    Check(manager.InitializePCMaterialForAnalysis(),"Original PC Font material initializer");
    auto* material=dynamic_cast<spDXMaterial*>(manager.GetFallbackMaterialForAnalysis());
    Check(material&&material==manager.GetPrimaryMaterialForAnalysis()&&manager.GetCurrentMaterialForAnalysis().use_count()==2,"Two canonical material owners");
    Check(capture.U32()==41,"Original material words");
    for(auto state:material->GetRenderStatesForAnalysis())Check(state==capture.U32(),"Original material state");
    for(const auto* color:{&material->GetDiffuseColorForAnalysis(),&material->GetAmbientColorForAnalysis(),&material->GetSpecularColorForAnalysis(),&material->GetEmissiveColorForAnalysis()})
        for(float channel:*color){std::uint32_t bits;std::memcpy(&bits,&channel,4);Check(bits==capture.U32(),"Original material color bits");}
    Check(capture.U32()==0xcccccccc&&!material->HasInitializedSpecularPowerForAnalysis(),"Power remains unassigned; allocator poison is not a default");
    Check(material->GetRenderOverrideByteForAnalysis()==capture.U32()&&material->GetVertexAlphaByteForAnalysis()==capture.U32(),"Original material flags");
    auto* pass=dynamic_cast<spMaterialPassLayer*>(material->GetPassForAnalysis(0));
    Check(pass&&pass->GetFinalBlendOperationForAnalysis()==capture.U32()&&pass->GetLayerCountForAnalysis()==capture.U32(),"Original default font pass");
    for(unsigned i=0;i<9;++i)Check(pass->GetLayerForAnalysis(0)->GetMaterialTextureForAnalysis()->GetTextureStatesForAnalysis()[i]==capture.U32(),"Original default font texture state");
    auto cases=capture.U32();Check(cases==11,"Eleven original geometry cases");
    for(unsigned i=0;i<cases;++i){
        const auto mode=capture.U32(),present=capture.U32(),wrap=capture.U32(),align=capture.U32(),color=capture.U32();
        const std::array<float,3> position{capture.Float(),capture.Float(),capture.Float()};auto text=capture.Blob();text.push_back(0);
        const auto vb=capture.Blob(),ib=capture.Blob();spFontManager::Text3DGeometryForAnalysis geometry;
        const char* bytes=present?reinterpret_cast<const char*>(text.data()):nullptr;bool ok=false;
        if(!mode){manager.SelectFontAndColorForAnalysis(font.get(),color);ok=manager.BuildPCText3DGeometryForAnalysis(position,bytes,wrap,geometry,&error);}
        else {
            spTextRenderable object;object.SetColorForAnalysis(color);
            Check(object.SetTextForAnalysis(bytes,&manager,&error)&&object.SetWrapWidthForAnalysis(wrap,&manager,&error)
                &&object.SetAlignmentForAnalysis(align,&manager,&error),"Text fields use original shared layout");
            ok=object.BuildPCGeometryForAnalysis(manager,geometry,&error);
            const spRenderable& base=object;Check(base.RequiresPCAlphaQueueForAnalysis(),"Text gate queues independently of material existence");
            object.SetAlphaSortEnabledForAnalysis(false);Check(!base.RequiresPCAlphaQueueForAnalysis(),"Text alpha override can disable queue");
        }
        if(!ok)throw std::runtime_error(error);
        Match(geometry.vertices.data(),geometry.vertices.size()*24,vb,"Original vertex bytes including color/UV");
        Match(geometry.indices.data(),geometry.indices.size()*2,ib,"Original triangle bytes");
    }
    Check(capture.file.peek()==std::char_traits<char>::eof(),"Fixture exact end");
    spFontManager::Text3DGeometryForAnalysis output;output.vertices.push_back({{1,2,3},0xaabbccdd,{4,5}});const auto before=output.vertices;
    manager.SelectFontAndColorForAnalysis(font.get(),1);
    Check(!manager.BuildPCText3DGeometryForAnalysis({},"ABAB",5,output,&error)&&error.find("ITERATION_LIMIT")!=std::string::npos,"Observed original long-word loop is bounded explicitly");
    Check(output.vertices.size()==before.size()&&std::memcmp(output.vertices.data(),before.data(),before.size()*24)==0,"Rejected geometry leaves caller output unchanged");
    Check(!manager.BuildPCText3DGeometryForAnalysis({0,0,std::numeric_limits<float>::infinity()},"A",0,output,&error),"Nonfinite host position refused");
    const std::string huge(4097,'A');Check(!manager.BuildPCText3DGeometryForAnalysis({},huge.c_str(),0,output,&error)&&error.find("GLYPH_LIMIT")!=std::string::npos,"Glyph output cap enforced");
    spFontManager missing;Check(!missing.BuildPCText3DGeometryForAnalysis({},"A",0,output,&error),"No invented default Font");
    Check(missing.BuildPCText3DGeometryForAnalysis({},nullptr,0,output,&error)&&output.vertices.empty(),"NULL returns before dependencies");
    Check(!missing.InitializeForAnalysis()&&!missing.IsInitializedForAnalysis(),"Unspecified platform startup does not fabricate success");
    spPCFontManager pc;Check(!pc.InitializeForAnalysis()&&!pc.IsPlatformBufferReadyForAnalysis()
        &&pc.GetFallbackMaterialForAnalysis()!=nullptr,"PC material prefix completed, system Font producer explicitly absent");
}
}
int main(){try{Run();CustomMaterial();std::cout<<"PASS "<<checks<<" checks: "<<originalBytes<<" original PC text geometry bytes\n";return 0;}
catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
