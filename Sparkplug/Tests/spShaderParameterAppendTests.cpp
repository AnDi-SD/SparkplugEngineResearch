#include "Code/SparkplugPC/spPCVertexShader.h"
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace
{
    int checks=0;
    void Check(bool ok,const char* label){++checks;if(!ok)throw std::runtime_error(label);}
    template<class T>void Array(std::ostream& out,const T& values){out<<'[';bool first=true;for(const auto& v:values){if(!first)out<<',';first=false;out<<v;}out<<']';}
    const std::vector<std::string> Names{"view_proj_matrix","view_matrix","VPTransform","inv_view_matrix","BlendMatrices","AmbientCol","ConstColor","MatDiffuse","MatSpecular","MatSpecularPwr","LightMatDiff","LightMatSpec","LightPos","LightDir","LightInner","LightOuter","UVTransform","ViewDirLightDir0","LightAmbientColorDir0","LightDiffuseColorDir0","LightSpecularColorDir0","LightAttenuation","","unknown","matDiffuse","MatDiffuse[0]"};
    std::string Run(const std::string& mode)
    {
        spPCVertexShader shader;if(mode=="wrap"){spDXShader::ScalarWordsForAnalysis scalar{};scalar[8]=0xfffffffe;shader.SetScalarWordsForAnalysis(scalar);}
        const auto names=mode=="names"?Names:std::vector<std::string>{"MatDiffuse","MatSpecularPwr","unknown","BlendMatrices"};
        std::ostringstream out;out<<"[\""<<mode<<"\",[";
        for(unsigned i=0;i<names.size();++i)
        {
            spDXShader::ParameterForAnalysis p;p.name.fill(char(0xcc));std::memcpy(p.name.data(),names[i].c_str(),names[i].size()+1);p.type=0xdeadbeef;p.startRegister=mode=="overlap"?17:i*7;
            constexpr unsigned wrapCounts[]{3,0,0xffffffff,2};p.registerCount=mode=="wrap"?wrapCounts[i]:i%5;
            Check(shader.AppendParameterForAnalysis(p),"actual by-value append");
            if(i)out<<',';out<<'['<<shader.GetScalarWordsForAnalysis()[8]<<",[";bool first=true;
            for(const auto& parameter:shader.GetParametersForAnalysis())
            {if(!first)out<<',';first=false;std::array<unsigned,11> raw;std::memcpy(raw.data(),parameter.name.data(),32);raw[8]=parameter.type;raw[9]=parameter.startRegister;raw[10]=parameter.registerCount;Array(out,raw);}
            out<<"]]";
        }
        out<<"]]";return out.str();
    }
}
int main(int argc,char** argv)
{
    try{if(argc==3&&std::string(argv[1])=="--case"){std::cout<<Run(argv[2])<<'\n';return 0;}
        for(const auto* mode:{"names","overlap","wrap"})(void)Run(mode);
        spPCVertexShader shader;spDXShader::ParameterForAnalysis p;p.name.fill('x');p.registerCount=7;
        Check(!shader.AppendParameterForAnalysis(p)&&shader.GetParametersForAnalysis().empty()&&shader.GetScalarWordsForAnalysis()[8]==0,"unterminated name refused without mutation");
        std::cout<<"PASS "<<checks<<'/'<<checks<<": shader parameter append\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
