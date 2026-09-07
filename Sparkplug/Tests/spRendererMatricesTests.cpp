#include "Code/SparkplugDX/spDXRenderer.h"
#include "Code/SparkplugPC/spPCVertexShader.h"
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace
{
    int checks=0;
    void Check(bool value,const char* label){++checks;if(!value)throw std::runtime_error(label);}
    std::uint32_t Bits(float value){std::uint32_t raw;std::memcpy(&raw,&value,4);return raw;}
    template<class T>void Array(std::ostream& o,const T& values){o<<'[';bool first=true;for(const auto& v:values){if(!first)o<<',';first=false;o<<v;}o<<']';}
    std::string Run(const std::string& mode)
    {
        using R=spDXRenderer;R::MatrixStateForAnalysis state;spPCVertexShader shader;
        shader.SetParametersForAnalysis({{{},1,0,4}});
        spDXShader::ConstantInputsForAnalysis input;input.rendererMatrices=&state;
        std::uint32_t seed=0x12345678;std::ostringstream out;out<<"[\""<<mode<<"\",[";
        for(unsigned c=0;c<12;++c)
        {
            for(unsigned m=0;m<3;++m)for(unsigned i=0;i<16;++i)
            {
                seed=seed*1664525u+1013904223u;
                state.inputs[m][i]=Bits(c?float(int(seed>>24)-128)/32.F:float(int((i+m)%5)-2));
            }
            state.dirty=true;
            for(unsigned repeat=0;repeat<2;++repeat)
            {
                if(repeat)state.inputs[0][0]=Bits(99.F);
                const bool refresh=mode=="direct"||state.dirty;std::vector<unsigned> constants;
                bool result;
                if(mode=="direct")result=R::RefreshMatricesForAnalysis(state);
                else if(mode=="constants"){constants.resize(16);result=shader.BuildConstantsForAnalysis(constants,input);}
                else result=R::GetCachedMatrixForAnalysis(state,mode=="view"?0:mode=="vp"?1:2)!=nullptr;
                Check(result&&!state.dirty,"complete original matrix refresh/getter");
                if(c||repeat)out<<',';out<<'['<<c<<','<<repeat<<','<<refresh<<",[";
                for(unsigned m=0;m<3;++m){if(m)out<<',';Array(out,state.cached[m]);}
                out<<"],";Array(out,constants);out<<']';
            }
        }
        out<<"]]";return out.str();
    }
}
int main(int argc,char** argv)
{
    try{
        if(argc==3&&std::string(argv[1])=="--case"){std::cout<<Run(argv[2])<<'\n';return 0;}
        for(const auto* mode:{"direct","view","vp","wvp","constants"})(void)Run(mode);
        spDXRenderer::MatrixStateForAnalysis state;state.dirty=true;state.inputs[1][0]=0x7fc00000;
        Check(!spDXRenderer::RefreshMatricesForAnalysis(state)&&state.dirty,"unknown nonfinite input guarded without clear");
        Check(!spDXRenderer::GetCachedMatrixForAnalysis(state,3),"invalid cache index guarded");
        state.dirty=false;Check(spDXRenderer::GetCachedMatrixForAnalysis(state,0)==&state.cached[0],"clean getter does not inspect changed inputs");
        std::cout<<"PASS "<<checks<<'/'<<checks<<": renderer matrices\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
