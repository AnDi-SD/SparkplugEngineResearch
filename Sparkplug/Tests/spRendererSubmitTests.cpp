#include "Code/SparkplugDX/spDXRenderer.h"
#include "Code/SparkplugDX/spDXIndexBuffer.h"
#include "Code/SparkplugDX/spDXVertexBuffer.h"
#include "Code/SparkplugDX/spDXMaterial.h"
#include "Code/SparkplugPC/spPCVertexDeclaration.h"
#include "Code/SparkplugPC/spPCShaderManager.h"
#include "Code/Sparkplug/spMaterialPassLayer.h"
#include "Code/Sparkplug/spStdLayer.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace
{
    using R=spDXRenderer;int checks=0;
    void Check(bool ok,const char* label){++checks;if(!ok)throw std::runtime_error(label);}
    template<class T>void Array(std::ostream& out,const T& values){out<<'[';bool first=true;for(const auto& v:values){if(!first)out<<',';first=false;out<<v;}out<<']';}
    std::uint32_t Bits(float value){std::uint32_t bits;std::memcpy(&bits,&value,4);return bits;}
    struct Sink
    {
        R::SubmissionStateForAnalysis state;bool fail=false;
        std::array<spPCVertexDeclaration,2> declarations;
        std::array<std::shared_ptr<spDXIndexBuffer>,2> indices{std::make_shared<spDXIndexBuffer>(),std::make_shared<spDXIndexBuffer>()};
        std::array<std::shared_ptr<spDXVertexBuffer>,2> vertices{std::make_shared<spDXVertexBuffer>(),std::make_shared<spDXVertexBuffer>()};
        std::vector<std::string> events;
        unsigned Token(const spBaseObject* p)const
        {if(!p)return 0;for(unsigned i=0;i<2;++i){if(p==&declarations[i])return 1+i;if(p==indices[i].get())return 3+i;if(p==vertices[i].get())return 5+i;}throw std::runtime_error("unknown geometry identity");}
        std::string State()const
        {std::ostringstream o;o<<'['<<Token(state.geometry.declaration)<<','<<Token(state.geometry.indices.get())<<','<<Token(state.geometry.vertices.get())<<",["<<indices[0].use_count()<<','<<indices[1].use_count()<<','<<vertices[0].use_count()<<','<<vertices[1].use_count()<<"]]";return o.str();}
        std::int32_t Event(const std::string& content){events.push_back('['+content+','+State()+']');return fail?-1:0;}
        static std::int32_t Geometry(void* ptr,unsigned kind,std::uintptr_t handle,std::uint32_t stride)noexcept
        {auto& s=*static_cast<Sink*>(ptr);std::ostringstream o;if(kind==0)o<<"\"declaration\","<<handle;else if(kind==1)o<<"\"indices\","<<handle;else o<<"\"stream\",0,"<<handle<<",0,"<<stride;return s.Event(o.str());}
        static std::int32_t Render(void* ptr,unsigned index,unsigned value)noexcept
        {return static_cast<Sink*>(ptr)->Event("\"render\","+std::to_string(index)+','+std::to_string(value));}
        static std::int32_t Material(void* ptr,const R::LightingStateForAnalysis& value)noexcept
        {std::array<std::uint32_t,17> words{};unsigned i=0;for(const auto* c:{&value.diffuse,&value.ambient,&value.specular,&value.emissive})for(float v:*c)words[i++]=Bits(v);words[16]=Bits(value.specularPower);std::ostringstream o;o<<"\"material\",";Array(o,words);return static_cast<Sink*>(ptr)->Event(o.str());}
        static std::int32_t TextureState(void* ptr,bool sampler,unsigned stage,unsigned index,unsigned value)noexcept
        {return static_cast<Sink*>(ptr)->Event(std::string(sampler?"\"sampler\",":"\"stage\",")+std::to_string(stage)+','+std::to_string(index)+','+std::to_string(value));}
        static std::int32_t Draw(void* ptr,bool indexed,const std::array<std::uint32_t,6>& args)noexcept
        {std::ostringstream o;o<<(indexed?"\"indexed\"":"\"draw\"");for(unsigned i=0;i<(indexed?6u:3u);++i)o<<','<<args[i];return static_cast<Sink*>(ptr)->Event(o.str());}
        static std::int32_t Shader(void* ptr,bool pixel,std::uintptr_t value)noexcept
        {return static_cast<Sink*>(ptr)->Event(std::string(pixel?"\"pixel-shader\",":"\"vertex-shader\",")+std::to_string(value));}
        static std::int32_t Constants(void* ptr,bool pixel,const std::array<unsigned,4>* rows,std::size_t count)noexcept
        {std::ostringstream o;std::vector<unsigned> words;for(unsigned i=0;i<count;++i)for(auto word:rows[i])words.push_back(word);o<<(pixel?"\"pixel-constants\",0,":"\"vertex-constants\",0,");Array(o,words);o<<','<<count;return static_cast<Sink*>(ptr)->Event(o.str());}
    };
    std::unique_ptr<spDXMaterial> Material(unsigned count,unsigned layers)
    {
        auto material=std::make_unique<spDXMaterial>();material->SetSpecularPowerForAnalysis(0);Check(material->SetRenderStateForAnalysis(8,2),"unlit input");
        for(unsigned i=0;i<count;++i){auto pass=std::make_shared<spMaterialPassLayer>();pass->SetFinalBlendOperationForAnalysis(i*3);for(unsigned j=0;j<layers;++j)Check(pass->SetLayerForAnalysis(j,std::make_unique<spStdLayer>()),"layer");Check(material->SetPassForAnalysis(i,pass),"pass");}
        return material;
    }
    void InitializeCaches()
    {
        // Captured fresh PC4C5AB0 ->4BCF20, renderer-state-init-fresh-run1.json.
        // Unwritten game bytes are deliberately distinct host inputs, not zero defaults.
        constexpr std::uint32_t seed=0xa5a5a5a5,invalid=0xffffffff;
        Sink owner;auto material=Material(0,0);auto& state=owner.state;
        state.geometry={&owner.declarations[1],owner.indices[1],owner.vertices[1]};
        state.installedMaterial=material.get();state.raw.fill(seed);state.frame=91;
        state.deviceStates.fill(seed);state.textures.palette=seed;
        for(unsigned stage=0;stage<8;++stage)
        {
            state.textures.desired[stage].fill(seed);state.textures.cache[stage].raw.fill(seed);
            state.textures.cache[stage].coordinateIndex=state.textures.cache[stage].transformFlags=seed;
            state.textures.desiredTextures[stage]=0x10000+stage;
            state.textures.boundTextures[stage]=0x20000+stage;state.textures.dirty[stage]=std::uint8_t(0x80+stage);
            state.uv[stage].fill(float(stage)-42.5F);
        }
        state.lighting.diffuse={1.25F,2.25F,3.25F,4.25F};state.lighting.ambient={5.25F,6.25F,7.25F,8.25F};
        state.lighting.specular={9.25F,10.25F,11.25F,12.25F};state.lighting.emissive={13.25F,14.25F,15.25F,16.25F};
        state.lighting.specularPower=17.25F;state.lighting.packedColorC194=0x10203040;
        state.lighting.globalBlackARGB=0x50607080;state.lighting.diffuseSource=seed;state.lighting.ambientSource=seed;
        state.draw.vertex.top=2;state.draw.pixel.top=3;state.draw.boundVertex=0x30000;state.draw.boundPixel=0x40000;state.draw.pixelEnabled=true;
        for(unsigned i=0;i<4;++i){state.draw.vertex.entries[i]={0x50000+i,0x60000+i};state.draw.pixel.entries[i]={0x70000+i,0x80000+i};}
        state.draw.vertexConstants={{{1,2,3,4}},{{5,6,7,8}}};state.draw.pixelConstants={{{9,10,11,12}}};
        const auto before=state;
        const auto unknownUnchanged=[&]()
        {
            const auto& a=state.lighting;const auto& b=before.lighting;
            if(state.geometry.declaration!=before.geometry.declaration||state.geometry.indices!=before.geometry.indices||state.geometry.vertices!=before.geometry.vertices
                ||state.installedMaterial!=before.installedMaterial||state.textures.desiredTextures!=before.textures.desiredTextures||state.uv!=before.uv
                ||a.diffuse!=b.diffuse||a.ambient!=b.ambient||a.specular!=b.specular||a.emissive!=b.emissive
                ||Bits(a.specularPower)!=Bits(b.specularPower)||a.packedColorC194!=b.packedColorC194||a.globalBlackARGB!=b.globalBlackARGB)return false;
            const auto& d=state.draw;const auto& old=before.draw;
            if(d.vertex.top!=old.vertex.top||d.pixel.top!=old.pixel.top||d.boundVertex!=old.boundVertex||d.boundPixel!=old.boundPixel
                ||d.pixelEnabled!=old.pixelEnabled||d.vertexConstants!=old.vertexConstants||d.pixelConstants!=old.pixelConstants)return false;
            for(unsigned i=0;i<4;++i)if(d.vertex.entries[i].identity!=old.vertex.entries[i].identity||d.vertex.entries[i].deviceShader!=old.vertex.entries[i].deviceShader
                ||d.pixel.entries[i].identity!=old.pixel.entries[i].identity||d.pixel.entries[i].deviceShader!=old.pixel.entries[i].deviceShader)return false;
            return true;
        };
        Check(!R::InitializePCSubmissionCachesForAnalysis(state,nullptr,nullptr),"cache initialization refuses missing backend");
        Check(unknownUnchanged()&&state.raw==before.raw&&state.deviceStates==before.deviceStates&&state.frame==before.frame
            &&state.textures.desired==before.textures.desired&&state.textures.boundTextures==before.textures.boundTextures
            &&state.textures.dirty==before.textures.dirty&&state.textures.palette==before.textures.palette
            &&state.lighting.diffuseSource==seed&&state.lighting.ambientSource==seed,"missing backend does not mutate inputs");
        for(unsigned stage=0;stage<8;++stage)Check(state.textures.cache[stage].raw==before.textures.cache[stage].raw
            &&state.textures.cache[stage].coordinateIndex==seed&&state.textures.cache[stage].transformFlags==seed,"missing backend keeps stage caches");
        struct Recorder final
        {
            R::SubmissionStateForAnalysis* state;std::vector<std::array<std::uint32_t,2>> calls;bool invalidBefore=true;
            static std::int32_t Render(void* context,std::uint32_t index,std::uint32_t value) noexcept
            {
                auto& self=*static_cast<Recorder*>(context);self.invalidBefore&=self.state->deviceStates[index]==0xffffffff;
                self.calls.push_back({index,value});return static_cast<std::int32_t>(0x80004005u);
            }
        } recorder{&state};
        Check(R::InitializePCSubmissionCachesForAnalysis(state,Recorder::Render,&recorder),"original cache initialization completes despite external E_FAIL");
        const std::vector<std::array<std::uint32_t,2>> expected{{143,1},{27,1},{15,1},{24,192},{25,7}};
        Check(recorder.calls==expected&&recorder.invalidBefore,"exact original five ordered setters see invalid previous cache");
        Check(unknownUnchanged(),"unknown and omitted submission inputs preserved semantically");
        Check(state.frame==1&&state.lighting.diffuseSource==11&&state.lighting.ambientSource==10,"actual frame and source-cache writes");
        Check(std::all_of(state.raw.begin(),state.raw.end(),[invalid](auto v){return v==invalid;}),"eleven represented engine cache words invalidated");
        const std::array<std::uint32_t,9> desired{0,3,1,0,0,0xff000000,2,0,0};
        for(unsigned stage=0;stage<8;++stage)
        {
            const auto& cache=state.textures.cache[stage];
            Check(std::all_of(cache.raw.begin(),cache.raw.end(),[invalid](auto v){return v==invalid;})&&cache.coordinateIndex==invalid&&cache.transformFlags==invalid
                &&state.textures.desired[stage]==desired&&state.textures.boundTextures[stage]==invalid&&state.textures.dirty[stage]==0,"actual per-stage constructor and invalidator subset");
        }
        Check(state.textures.palette==invalid,"actual invalid palette token");
        auto expectedDevice=before.deviceStates;expectedDevice.fill(invalid);for(const auto& entry:expected)expectedDevice[entry[0]]=entry[1];
        Check(state.deviceStates==expectedDevice,"E_FAIL still caches five desired values; all other device cache words remain invalid");
    }
    std::string Run(const std::string& mode)
    {
        const bool weighted=mode.rfind("weighted",0)==0;
        const bool matrixMode=mode.find("matrix")!=std::string::npos,produced=mode.find("append")!=std::string::npos;
        Sink sink;sink.fail=mode=="failed"||mode=="weighted-failed";spPCShaderManager manager;
        // Historical --case inputs retain their declared fallback for the
        // existing native comparisons. This case uses the actual PC producer.
        auto fallback=mode=="actual-default"?spRenderer::CreatePCDefaultMaterialForAnalysis():Material(1,2);
        Check(fallback!=nullptr,"owned fallback material available");
        auto material=Material(mode=="empty"?0:mode=="two"||mode=="weighted-two"?2:1,1);
        spDXShader::ConstantInputsForAnalysis constants;constants.material=material.get();constants.blendMatrices.resize(1);
        R::MatrixStateForAnalysis matrixState;
        if(weighted)
        {
            auto shader=std::make_unique<spPCVertexShader>();
            std::vector<spDXShader::ParameterForAnalysis> parameters{{{},5,0,3},{{},8,3,1}};
            if(matrixMode){parameters.push_back({{},1,4,4});constants.rendererMatrices=&matrixState;}
            if(produced)
            {
                for(auto parameter:parameters)
                {const char* name=parameter.type==5?"BlendMatrices":parameter.type==8?"MatDiffuse":"view_proj_matrix";std::memcpy(parameter.name.data(),name,std::strlen(name)+1);parameter.type=0xdeadbeef;Check(shader->AppendParameterForAnalysis(parameter),"common parameter producer");}
                Check(shader->GetScalarWordsForAnalysis()[8]==(matrixMode?8u:4u),"produced row sum");
            }
            else{spDXShader::ScalarWordsForAnalysis scalar{};scalar[8]=matrixMode?8:4;shader->SetScalarWordsForAnalysis(scalar);shader->SetParametersForAnalysis(std::move(parameters));}
            Check(manager.CacheShaderForAnalysis({0x20011,0},shader),"actual full submission shader key");
        }
        R::SubmissionDeviceForAnalysis device;device.geometry=Sink::Geometry;device.render=Sink::Render;device.material=Sink::Material;device.textureState=Sink::TextureState;device.draw=Sink::Draw;device.context=&sink;
        device.shader=Sink::Shader;device.constants=Sink::Constants;
        std::ostringstream out;out<<"[\""<<mode<<"\",[";const std::shared_ptr<spDXIndexBuffer> noIndices;
        for(unsigned iteration=0;iteration<4;++iteration)
        {
            if(weighted)
            {
                for(unsigned i=0;i<16;++i)constants.blendMatrices[0][i]=Bits(float(iteration*100+i));
                material->SetDiffuseColorForAnalysis({.25F,.5F,.75F,float(iteration)});
                if(matrixMode)
                {for(unsigned m=0;m<3;++m)for(unsigned i=0;i<16;++i)matrixState.inputs[m][i]=Bits(float(int((i+m+iteration)%5)-2));matrixState.dirty=true;}
            }
            const unsigned selected=mode=="switch"&&iteration>=2?1:0;
            const auto& indices=mode=="switch"&&iteration==3?noIndices:sink.indices[selected];
            sink.events.clear();const bool result=R::SubmitUnlitGeometryForAnalysis(sink.state,&sink.declarations[selected],indices,sink.vertices[selected],{},iteration<1?32:48,{iteration==3?1u:2u,11,13,17,19,weighted?0x803u:0x801u,0},*material,*fallback,manager,device,weighted?&constants:nullptr);
            Check(result,"whole unlit submit");if(iteration)out<<',';
            if(mode=="actual-default")
            {
                const std::array<std::uint32_t,9> unused{0,0,1,0,0,0xFF000000,2,0,0};
                Check(sink.state.textures.desired[1]==unused&&sink.state.textures.desired[7]==unused
                    &&sink.state.textures.desired[0][1]==3,"actual second fallback layer supplies unused stages through whole submit");
                Check(!fallback->HasInitializedSpecularPowerForAnalysis(),"whole submit does not invent unknown fallback power");
            }
            out<<'['<<iteration<<','<<result<<','<<sink.State()<<',';Array(out,sink.state.raw);out<<','<<material->GetRenderStateForAnalysis(7)<<",[";
            for(unsigned i=0;i<sink.events.size();++i){if(i)out<<',';out<<sink.events[i];}out<<"]]";
        }
        out<<"]]";
        Check(material->SetRenderStateForAnalysis(8,4),"guard lit input");
        Check(!R::SubmitUnlitGeometryForAnalysis(sink.state,&sink.declarations[0],sink.indices[0],sink.vertices[0],{},32,{2,11,13,17,19,0x801,0},*material,*fallback,manager,device),"unknown light chain refused");
        return out.str();
    }
}
int main(int argc,char** argv)
{
    try{if(argc==3&&std::string(argv[1])=="--case"){std::cout<<Run(argv[2])<<'\n';return 0;}
        InitializeCaches();
        for(const auto* mode:{"empty","one","two","failed","switch","mesh","weighted","weighted-two","weighted-failed","weighted-matrix","weighted-append","weighted-append-matrix","actual-default"})(void)Run(mode);
        std::cout<<"PASS "<<checks<<'/'<<checks<<": renderer full unlit submission\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
