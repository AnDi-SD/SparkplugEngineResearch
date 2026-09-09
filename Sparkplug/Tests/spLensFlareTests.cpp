#include "Code/Sparkplug/spLensFlare.h"
#include "Code/Sparkplug/spLensFlareSerializer.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/Sparkplug/spResourceFATSerializer.h"
#include "Code/SparkplugDX/spDXMaterial.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace {
int checks=0;using Bytes=std::vector<std::uint8_t>;
void Check(bool value,const char* label){++checks;if(!value)throw std::runtime_error(label);}
std::uint32_t Bits(float value){std::uint32_t bits;std::memcpy(&bits,&value,4);return bits;}
Bytes Parse(const std::string& text){Bytes result;Check(text.size()%2==0,"hex extent");for(std::size_t i=0;i<text.size();i+=2)result.push_back(std::uint8_t(std::stoul(text.substr(i,2),nullptr,16)));return result;}
bool Read(const Bytes& data,spLensFlare& object){spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);spMemoryStream stream;
    Check(stream.ResizeAndSetSize(static_cast<std::uint32_t>(data.size())),"test stream");if(!data.empty())std::memcpy(stream.GetBuffer(),data.data(),data.size());
    spLensFlareSerializer serializer;std::string error;return serializer.ReadPayloadForAnalysis(context,stream,static_cast<std::uint32_t>(data.size()),object,&error);}
void Element(const spLensFlare::ElementForAnalysis& value){std::cout<<"{\"material\":"<<(value.quad.GetMaterialForAnalysis()?7:0)<<",\"tail\":["<<value.color<<','<<Bits(value.distance)<<','<<Bits(value.scale)<<"]}";}
}
int main(int argc,char** argv){try{
    if(argc==3&&(std::string(argv[1])=="--capture"||std::string(argv[1])=="--links")){
        spLensFlare flare;spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
        const bool links=std::string(argv[1])=="--links";
        if(links){context.externalOwners.push_back(std::make_shared<spDXMaterial>());
            const std::uint8_t directory[]={1,0,0,0,7,0,0,0,0,0,0xec,0x39,0x7b,0x79,0,0,0,0,0,0,0,0};
            spMemoryStream index;Check(index.ResizeAndSetSize(sizeof(directory)),"typed material FAT");std::memcpy(index.GetBuffer(),directory,sizeof(directory));
            Check(manager.GetFATForAnalysis()->LoadIndexForAnalysis(index),"FAT load");manager.GetFATForAnalysis()->FindByIDForAnalysis(7)->object=context.externalOwners.front().get();}
        auto bytes=Parse(argv[2]);spMemoryStream input;Check(input.ResizeAndSetSize(static_cast<std::uint32_t>(bytes.size())),"reader stream");std::memcpy(input.GetBuffer(),bytes.data(),bytes.size());spLensFlareSerializer serializer;std::string error;
        Check(serializer.ReadPayloadForAnalysis(context,input,static_cast<std::uint32_t>(bytes.size()),flare,&error),"actual common LensFlare reader");
        Check(!flare.GetRenderNodeForAnalysis(),"null node fixture");
        std::cout<<"{\"occlusion_bits\":["<<Bits(flare.GetOcclusionRadiusForAnalysis())<<','<<Bits(flare.GetOcclusionSpeedForAnalysis())<<"],\"render_node\":0,\"primary\":";Element(flare.GetPrimaryForAnalysis());std::cout<<",\"counted\":[";
        bool comma=false;for(const auto& value:flare.GetElementsForAnalysis()){if(comma)std::cout<<',';comma=true;Element(*value);}std::cout<<']';
        if(links)std::cout<<",\"material_refs\":"<<context.externalOwners.front().use_count();std::cout<<"}\n";return 0;
    }
    spLensFlare flare;Check(Read({0,0},flare),"omitted fields preserve actual defaults");Check(flare.GetOcclusionRadiusForAnalysis()==1&&flare.GetOcclusionSpeedForAnalysis()==1,"constructor scalar defaults");
    Check(flare.GetPrimaryForAnalysis().color==0xffffffff&&flare.GetPrimaryForAnalysis().distance==0&&flare.GetPrimaryForAnalysis().scale==1,"primary record defaults");
    Check(!Read({0,0xa1,4,1,0,0,0,0},flare),"truncated counted record rejected");
    Check(!Read({0,0xa2,4,0,0,0,0,0},flare),"short occlusion field rejected");
    Check(!Read({0,0xa1,4,1,16,0,0,0},flare),"bounded element allocation");
    Check(!Read({0,0xa3,1,0,0},flare),"short reference rejected");
    Check(!Read({0,0,0},flare),"trailing derived bytes rejected");
    Check(Bits(flare.GetBoundingSphereForAnalysis()[3])==0x3fb504f3,"native radius factor");
    std::cout<<"PASS "<<checks<<"/"<<checks<<": LensFlare shared reader\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
