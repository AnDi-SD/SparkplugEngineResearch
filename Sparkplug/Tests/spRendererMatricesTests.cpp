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
    std::string RunDirtyConstants(unsigned type)
    {
        Check(type>=1&&type<=4,"directed matrix descriptor type");
        using R=spDXRenderer;R::MatrixStateForAnalysis state;spPCVertexShader shader;
        shader.SetParametersForAnalysis({{{},type,1,4}});
        spDXShader::ConstantInputsForAnalysis input;input.rendererMatrices=&state;
        const std::array<std::array<float,16>,3> original{{
            {{2,0,0,0,0,3,0,0,0,0,4,0,1,2,3,1}},
            {{1,1,0,0,0,1,1,0,1,0,1,0,5,7,11,1}},
            {{2,0,1,0,0,3,0,1,1,0,2,0,0,0,1,1}}
        }};
        auto changed=original;changed[0][12]=13;changed[1][0]=3;
        changed[1][12]=-4;changed[2][5]=5;
        std::vector<unsigned> firstOutput;decltype(state.cached) firstCache{};
        std::ostringstream out;out<<'['<<type<<",[";
        for(unsigned phase=0;phase<3;++phase)
        {
            const auto& matrices=phase?changed:original;
            for(unsigned m=0;m<3;++m)for(unsigned i=0;i<16;++i)
                state.inputs[m][i]=Bits(matrices[m][i]);
            const bool refresh=phase!=1;state.dirty=refresh;
            std::vector<unsigned> output(48,0xa5a5a5a5);
            Check(shader.BuildConstantsForAnalysis(output,input)&&!state.dirty,
                "directed matrix constants consume dirty cache");
            for(unsigned i=0;i<output.size();++i)if(i<4||i>=20)
                Check(output[i]==0xa5a5a5a5,"matrix descriptor preserves words outside destination");
            if(phase==0){firstOutput=output;firstCache=state.cached;}
            else if(phase==1)Check(output==firstOutput&&state.cached==firstCache,
                "changed raw inputs leave clean cached constants unchanged");
            else Check(output!=firstOutput&&state.cached!=firstCache,
                "dirty input refresh changes constants and matrix cache");
            if(phase)out<<',';out<<'['<<phase<<','<<refresh<<','<<state.dirty<<",[";
            for(unsigned m=0;m<3;++m){if(m)out<<',';Array(out,state.cached[m]);}
            out<<"],";Array(out,output);out<<']';
        }
        out<<"]]";return out.str();
    }
}
int main(int argc,char** argv)
{
    try{
        if(argc==3&&std::string(argv[1])=="--case")
        {
            const std::string mode=argv[2];
            if(mode=="dirty-constants-1"||mode=="dirty-constants-2"||mode=="dirty-constants-3"||mode=="dirty-constants-4")
                std::cout<<RunDirtyConstants(static_cast<unsigned>(mode.back()-'0'))<<'\n';
            else std::cout<<Run(mode)<<'\n';
            return 0;
        }
        for(const auto* mode:{"direct","view","vp","wvp","constants"})(void)Run(mode);
        for(unsigned type=1;type<=4;++type)(void)RunDirtyConstants(type);
        spDXRenderer::MatrixStateForAnalysis state;state.dirty=true;state.inputs[1][0]=0x7fc00000;
        Check(!spDXRenderer::RefreshMatricesForAnalysis(state)&&state.dirty,"unknown nonfinite input guarded without clear");
        Check(!spDXRenderer::GetCachedMatrixForAnalysis(state,3),"invalid cache index guarded");
        state.dirty=false;Check(spDXRenderer::GetCachedMatrixForAnalysis(state,0)==&state.cached[0],"clean getter does not inspect changed inputs");
        std::cout<<"PASS "<<checks<<'/'<<checks<<": renderer matrices\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
