#include "spParticleCapture.h"
#include "Analysis/PC/spParticleSampling.h"
#include "Code/Sparkplug/spParticleSystemSerializer.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/Sparkplug/spRenderNode.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <cstring>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

using namespace sparkplug::reconstruction;
namespace
{
    using Bytes=std::vector<std::uint8_t>;
    unsigned checks=0;
    void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
    std::string Hex(const Bytes& values)
    {constexpr char digits[]="0123456789abcdef";std::string text;for(auto v:values){text+=digits[v>>4];text+=digits[v&15];}return text;}
    Bytes Unhex(const std::string& text)
    {
        Check(text.size()%2==0&&text.size()<=8192,"Bounded particle hex input");Bytes result;
        const auto nibble=[](char c)->unsigned{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;throw std::runtime_error("Invalid hex digit");};
        for(std::size_t i=0;i<text.size();i+=2)result.push_back(std::uint8_t(nibble(text[i])*16+nibble(text[i+1])));return result;
    }
    void Open(spMemoryStream& stream,const Bytes& bytes={})
    {Check(stream.ResizeAndSetSize(std::uint32_t(bytes.size())),"Particle stream extent");if(!bytes.empty())std::memcpy(stream.GetBuffer(),bytes.data(),bytes.size());Check(stream.Seek(spStream::SeekSource::essStart,0),"Particle stream rewind");}
    Bytes Data(spMemoryStream& stream)
    {std::uint32_t size=0;Check(stream.GetSize(&size),"Particle output extent");const auto* data=static_cast<const std::uint8_t*>(stream.GetBuffer());return size?Bytes(data,data+size):Bytes{};}
    bool Read(spParticleSystem& object,const Bytes& payload,std::string& error)
    {
        spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
        spMemoryStream stream;Open(stream,payload);return spParticleSystemSerializer{}.ReadPayloadForAnalysis(context,stream,std::uint32_t(payload.size()),object,&error);
    }
    std::string Sample(std::uint32_t tag,std::uint32_t seed,std::uint32_t count,const std::string& text)
    {
        Check(count<=128,"Bounded sample count");spParticleSystem object;auto& p=object.Parameters();p.regionType=tag;
        sparkplug::evidence::pc::ParticleRandomForAnalysis random;random.Seed(seed);
        const auto bytes=Unhex(text);Check(bytes.size()%4==0&&bytes.size()<=32,"Region extent");
        p.region.resize(bytes.size()/4);std::memcpy(p.region.data(),bytes.data(),bytes.size());
        std::vector<spParticleSystem::Vector3> positions;Check(object.SampleEmissionRegionForAnalysis(random,count,positions),"Bounded region sampling");
        Bytes state;const auto append=[&](const void* data,std::size_t size){const auto* b=static_cast<const std::uint8_t*>(data);state.insert(state.end(),b,b+size);};
        for(const auto& position:positions)append(position.data(),12);
        const auto positionsHex=Hex(state);state.clear();append(random.state.data(),random.state.size()*4);
        return "{\"positionsHex\":\""+positionsHex+"\",\"randomIndex\":"+std::to_string(random.index)+",\"randomStateHex\":\""+Hex(state)+"\"}";
    }
    void Guards()
    {
        using sparkplug::evidence::pc::ParticleRandomForAnalysis;
        ParticleRandomForAnalysis random;
        for(auto expected:{3499211612u,581869302u,3890346734u,3586334585u,545404204u})
            Check(random.Next()==expected,"Original unseeded MT19937 default sequence");
        const std::array<const char*,3> golden{
            "de489040000020c0f7029240fef09b40000020c02c290c416a04e13f000020c0e5021941",
            "bc912040147860bf9fae1240005b093ef71b51c026f18840bc3cb8be57b458c0a6167c40",
            "c0fbd8be91db0bc0b9699140bcc4943ffe462cc0803cb1404cab054034c80cc0931ab040"};
        unsigned goldenIndex=0;
        for(auto tag:{4u,6u,7u})
        {
            spParticleSystem sampled;sampled.Parameters().regionType=tag;
            sampled.Parameters().region=tag==4?std::vector<float>{1.25f,-2.5f,3.75f,0,1,0,4,6}:
                tag==6?std::vector<float>{1.25f,-2.5f,3.75f,4,2}:std::vector<float>{1.25f,-2.5f,3.75f,1,2,3};
            random.Seed(5489);std::vector<spParticleSystem::Vector3> positions;
            Check(sampled.SampleEmissionRegionForAnalysis(random,3,positions),"Native region sample fixture");
            const auto* begin=reinterpret_cast<const std::uint8_t*>(positions.data());
            Check(Hex(Bytes(begin,begin+36))==golden[goldenIndex++],"Original plane/cylinder/cone capture including member order and rounding");
            Check(!sampled.SampleEmissionRegionForAnalysis(random,129,positions),"Native caller batch128 host bound");
        }
        spParticleSystem object;spParticleSystemSerializer serializer;spMemoryStream output;Open(output);std::string error;
        Check(object.IsKindOf(spRenderable::ClassID)&&serializer.GetTargetClassIDForAnalysis()==object.ClassID,"Particle RTTI and serializer target");
        Check(serializer.WritePayloadForAnalysis(output,object,&error),error.c_str());
        Check(Hex(Data(output))=="62010000006300000000006b0000000000","Original default writer omits ranges and absent region");
        for(const char* text:{"0000","00ac0c00000000000000000000000000","0027006a00000000ac0c00000000000000000000000000",
            "002700a10c0000807f0000000000000000ac0c00000000000000000000000000",
            "002700ac0c000000000000000000000000ac0c00000000000000000000000000"})
        {spParticleSystem failed;Check(!Read(failed,Unhex(text),error),"Incomplete, looping, zero-capacity, nonfinite or duplicate-region input rejected");}
        object.Parameters().flags[0]=0;object.Parameters().rate=8;object.Parameters().times={-1,.75f};
        Check(object.PrepareNonLoopingForAnalysis()&&object.GetPoolStateForAnalysis()[1]==6,"Non-looping initial free-list count");
        object.Parameters().times={2,1};Check(object.PrepareNonLoopingForAnalysis()&&object.GetPoolStateForAnalysis()[1]==16,"Positive emission duration is used without min(lifetime)");
        object.Parameters().rate=0;Check(!object.PrepareNonLoopingForAnalysis(),"Native zero-capacity corruption is host-guarded");
        std::weak_ptr<spRenderNode> weakNode;std::weak_ptr<spParticleSystem> weakParticle;
        {auto node=std::make_shared<spRenderNode>();auto particle=std::make_shared<spParticleSystem>();weakNode=node;weakParticle=particle;
            Check(node->AttachRenderableForAnalysis(particle),"RenderNode owns Particle");particle->SetRenderNodeForAnalysis(node);
            Check(particle->GetRenderNodeForAnalysis()==node,"Particle borrows RenderNode");}
        Check(weakNode.expired()&&weakParticle.expired(),"Particle backlink creates no ownership cycle");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==2&&std::string(argv[1])=="--sample-batch")
        {
            std::string line;unsigned rows=0;
            while(std::getline(std::cin,line))
            {
                Check(++rows<=64&&line.size()<=128,"Bounded sample batch");
                std::istringstream input(line);std::uint32_t tag=0,seed=0,count=0;std::string hex,extra;
                Check(bool(input>>tag>>seed>>count>>hex)&&!(input>>extra),"Sample batch record");
                std::cout<<Sample(tag,seed,count,hex)<<'\n';
            }
        }
        else if(argc==5&&std::string(argv[1])=="--sample")
        {
            std::string text;std::getline(std::cin,text);
            std::cout<<Sample(std::stoul(argv[2]),std::stoul(argv[3]),std::stoul(argv[4]),text)<<'\n';
        }
        else if(argc==2&&std::string(argv[1])=="--payload")
        {
            std::string text,error;std::getline(std::cin,text);spParticleSystem object;
            if(text!="default")Check(Read(object,Unhex(text),error),error.c_str());
            spMemoryStream output;Open(output);Check(spParticleSystemSerializer{}.WritePayloadForAnalysis(output,object,&error),error.c_str());
            std::cout<<"{\"stateHex\":\""<<Hex(particle_test::State(object))<<"\",\"writerHex\":\""<<Hex(Data(output))<<"\"}\n";
        }
        else {Guards();std::cout<<"PASS "<<checks<<" particle checks\n";}
        return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
