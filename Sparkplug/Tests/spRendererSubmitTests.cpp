#include "Code/SparkplugDX/spDXRenderer.h"
#include "Code/SparkplugDX/spDXIndexBuffer.h"
#include "Code/SparkplugDX/spDXVertexBuffer.h"
#include "Code/SparkplugDX/spDXMaterial.h"
#include "Code/SparkplugPC/spPCVertexDeclaration.h"
#include "Code/SparkplugPC/spPCShaderManager.h"
#include "Code/Sparkplug/spMaterialPassLayer.h"
#include "Code/Sparkplug/spStdLayer.h"
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
    std::string Run(const std::string& mode)
    {
        const bool weighted=mode.rfind("weighted",0)==0;
        const bool matrixMode=mode.find("matrix")!=std::string::npos,produced=mode.find("append")!=std::string::npos;
        Sink sink;sink.fail=mode=="failed"||mode=="weighted-failed";spPCShaderManager manager;
        auto fallback=Material(1,2),material=Material(mode=="empty"?0:mode=="two"||mode=="weighted-two"?2:1,1);
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
        for(const auto* mode:{"empty","one","two","failed","switch","mesh","weighted","weighted-two","weighted-failed","weighted-matrix","weighted-append","weighted-append-matrix"})(void)Run(mode);
        std::cout<<"PASS "<<checks<<'/'<<checks<<": renderer full unlit submission\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
