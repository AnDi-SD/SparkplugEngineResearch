#include "Code/SparkplugPC/spPCVertexShader.h"
#include "Code/SparkplugDX/spDXMaterial.h"
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace
{
    int checks=0;void Check(bool ok,const char* label){++checks;if(!ok)throw std::runtime_error(label);}
    unsigned Bits(float value){unsigned word;std::memcpy(&word,&value,4);return word;}
    template<class T>void Array(std::ostream& out,const T& values){out<<'[';bool first=true;for(auto v:values){if(!first)out<<',';first=false;out<<v;}out<<']';}
    std::string Run(const std::string& mode)
    {
        std::ostringstream out;out<<"[\""<<mode<<"\",[";bool first=true;auto separate=[&]{if(!first)out<<',';first=false;};
        if(mode=="names")
        {
            for(const char* name:{"","view_proj_matrix","view_matrix","VPTransform","inv_view_matrix","BlendMatrices","AmbientCol","ConstColor","MatDiffuse","MatSpecular","MatSpecularPwr","LightMatDiff","LightMatSpec","LightPos","LightDir","LightInner","LightOuter","UVTransform","ViewDirLightDir0","LightAmbientColorDir0","LightDiffuseColorDir0","LightSpecularColorDir0","LightAttenuation","MatDiffuseX","matdiffuse","LightPos[0]","unknown"})
            {separate();out<<"[\""<<name<<"\","<<spDXShader::LookupParameterTypeForAnalysis(name)<<']';++checks;}
        }
        else
        {
            spPCVertexShader shader;spDXMaterial material;spDXShader::ConstantInputsForAnalysis input;
            input.material=&material;input.constantColor=0x7f234567;
            material.SetDiffuseColorForAnalysis({.25f,.5f,.75f,1});material.SetSpecularColorForAnalysis({1.5f,2,2.5f,3});material.SetSpecularPowerForAnalysis(7.5f);
            material.SetAmbientColorForAnalysis({.125f,.375f,.625f,.875f});
            for(unsigned ordinal=0;ordinal<3;++ordinal)for(unsigned i=0;i<16;++i)input.cachedMatrices[ordinal][i]=Bits(float((ordinal+1)*10+i));
            std::vector<spDXShader::ParameterForAnalysis> descriptors;
            if(mode=="inverse")
            {
                constexpr std::array<float,16> view{2,0,0,0,0,4,0,0,0,0,.5f,0,3,5,7,1};
                for(unsigned i=0;i<16;++i)input.cachedMatrices[0][i]=Bits(view[i]);
                for(auto [start,count]:std::array<std::pair<unsigned,unsigned>,3>{{{0,4},{3,2},{2,0}}})descriptors.push_back({{},4,start,count});
            }
            else if(mode=="uv")
            {
                input.uvMatrices.resize(2);for(unsigned i=0;i<32;++i)input.uvMatrices[i/16][i%16]=Bits(float(200+i));
                for(unsigned start:{0u,3u})for(unsigned count=0;count<7;++count)descriptors.push_back({{},17,start,count});
            }
            else if(mode=="light-colors"||mode=="light-missing"||mode=="light-geometry"||mode=="geometry-missing")
            {
                input.lightListPresent=true;input.lights.resize(2);
                for(unsigned i=0;i<2;++i)
                {
                    auto& light=input.lights[i];const auto f=float(i);light.type=i;
                    light.color={.25f+f,.5f+f,.75f+f,1+f};for(unsigned j=0;j<4;++j)light.attenuation[j]=float(i*10+j+1);
                    light.localDirection={.25f+f,.5f+f,.75f+f};light.worldPosition={4+f,5+f,6+f};light.worldDirection={1+f,2+f,3+f};light.innerAngle=.5f+f;light.outerAngle=1+f;
                }
                if(mode=="light-colors"||mode=="light-geometry"){input.ambientLight=input.lights[1];input.directionalLight=input.lights[0];}
                input.rendererAmbient={.25f,.5f,.75f,1};const bool geometry=mode=="light-geometry"||mode=="geometry-missing";
                if(geometry)
                {
                    constexpr std::array<float,16> view{1,2,3,0,4,5,6,0,7,8,10,0,10,11,12,1};
                    for(unsigned i=0;i<16;++i)input.viewMatrix[i]=Bits(view[i]);
                }
                for(unsigned kind:geometry?std::vector<unsigned>{13,14,15,16,18}:std::vector<unsigned>{6,11,12,19,20,21,22})
                    for(auto [start,count]:std::array<std::pair<unsigned,unsigned>,3>{{{0,1},{3,2},{2,0}}})descriptors.push_back({{},kind,start,count});
            }
            else if(mode=="blend")
            {
                input.blendMatrices.resize(2);for(unsigned i=0;i<32;++i)input.blendMatrices[i/16][i%16]=Bits(float(100+i));
                for(unsigned start:{0u,3u})for(unsigned count=0;count<7;++count)descriptors.push_back({{},5,start,count});
            }
            else
            {
                const std::vector<unsigned> types=mode=="matrices"?std::vector<unsigned>{1,2,3}:mode=="material"?std::vector<unsigned>{7,8,9,10}:std::vector<unsigned>{0,23,0xffffffff};
                for(unsigned type:types)for(auto [start,count]:std::array<std::pair<unsigned,unsigned>,3>{{{0,1},{3,2},{2,0}}})descriptors.push_back({{},type,start,count});
            }
            for(auto descriptor:descriptors)
            {
                shader.SetParametersForAnalysis({descriptor});std::vector<unsigned> values(128,0xa5a5a5a5);
                Check(shader.BuildConstantsForAnalysis(values,input),"known original parameter branch");
                separate();out<<'['<<descriptor.type<<','<<descriptor.startRegister<<','<<descriptor.registerCount<<',';values.resize(64);Array(out,values);out<<']';
            }
        }
        out<<"]]";return out.str();
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--case")std::cout<<Run(argv[2])<<'\n';
        else
        {
            for(const char* mode:{"names","matrices","material","unknown","blend","inverse","uv","light-colors","light-missing","light-geometry","geometry-missing"})Run(mode);
            spPCVertexShader shader;std::vector<unsigned> output(4,0xa5a5a5a5);spDXShader::ConstantInputsForAnalysis input;
            shader.SetParametersForAnalysis({{{},4,1,4}});Check(!shader.BuildConstantsForAnalysis(output,input),"oversized inverse output refused");
            shader.SetParametersForAnalysis({{{},1,0,5}});Check(!shader.BuildConstantsForAnalysis(output,input),"oversized cached matrix refused");
            shader.SetParametersForAnalysis({{{},7,0xffffffff,0}});Check(!shader.BuildConstantsForAnalysis(output,input),"native unchecked output offset host guard");
            std::cout<<"PASS "<<checks<<'/'<<checks<<": shader constant slice\n";
        }
        return 0;
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
