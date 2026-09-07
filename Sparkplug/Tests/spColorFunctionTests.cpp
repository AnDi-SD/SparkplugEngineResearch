#include "Code/Sparkplug/spColorFuncEval.h"
#include "Code/Sparkplug/spColorFuncEvalSerializer.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace
{
    using Bytes=std::vector<std::uint8_t>;int checks=0;
    void Check(bool ok,const char* text){++checks;if(!ok)throw std::runtime_error(text);}
    template<class T>void Add(Bytes& bytes,const T& value){const auto* p=reinterpret_cast<const std::uint8_t*>(&value);bytes.insert(bytes.end(),p,p+sizeof(T));}
    void Field(Bytes& bytes,std::uint8_t id,std::uint32_t raw){bytes.push_back(static_cast<std::uint8_t>(0xa0+id));bytes.push_back(4);Add(bytes,raw);}
    std::uint32_t Bits(float value){std::uint32_t raw;std::memcpy(&raw,&value,4);return raw;}
    void Open(spMemoryStream& stream,const Bytes& bytes={}){Check(stream.ResizeAndSetSize(static_cast<std::uint32_t>(bytes.size())),"capacity");if(!bytes.empty())std::memcpy(stream.GetBuffer(),bytes.data(),bytes.size());Check(stream.Seek(spStream::SeekSource::essStart,0),"rewind");}
    std::string Hex(const Bytes& bytes){constexpr char d[]="0123456789abcdef";std::string result;for(auto b:bytes){result+=d[b>>4];result+=d[b&15];}return result;}
    std::string Run(const std::string& mode)
    {
        spFunctionEval::SharedRandomForAnalysis().Seed(5489);spColorFuncEval color;Bytes input,output;std::ostringstream states;states<<std::setprecision(17)<<'[';
        if(mode=="clone")
        {
            color.SetColorsForAnalysis(0x12345678,0xFF000000);auto s=color.GetFunctionForAnalysis().GetStateForAnalysis();s.yOffset=7;s.functionType=8;color.GetFunctionForAnalysis().SetStateForAnalysis(s);
            auto owner=color.Clone();const auto* clone=dynamic_cast<const spColorFuncEval*>(owner.get());Check(clone!=nullptr,"ColorFunc clone factory");
            states<<clone->GetColor1ForAnalysis()<<','<<clone->GetColor2ForAnalysis()<<','<<clone->GetFunctionForAnalysis().GetStateForAnalysis().functionType<<','<<clone->GetFunctionForAnalysis().GetStateForAnalysis().yOffset;
        }
        else if(mode.rfind("blend",0)==0)
        {
            if(mode=="blend")color.SetColorsForAnalysis(0x80402010,0xff112244);if(mode=="blend-white")color.SetColorsForAnalysis(0xffffffff,0xffffffff);
            bool first=true;for(float value:{-3.F,-1.F,-.5F,0.F,.5F,1.F,3.F})
            {auto s=color.GetFunctionForAnalysis().GetStateForAnalysis();s.yOffset=value;color.GetFunctionForAnalysis().SetStateForAnalysis(s);std::uint32_t argb=0;Check(color.EvaluateColorForAnalysis(.25F,argb),"finite clamped byte blend");if(!first)states<<',';first=false;states<<'['<<value<<','<<argb<<']';}
        }
        else if(mode.rfind("type",0)==0)
        {
            color.SetColorsForAnalysis(0x80402010,0xff112244);auto s=color.GetFunctionForAnalysis().GetStateForAnalysis();s.functionType=static_cast<std::uint32_t>(std::stoul(mode.substr(4)));s.frequency=2;s.reciprocal=.5F;s.amplitude=1.5F;s.xOffset=.125F;s.yOffset=-.25F;s.pitch=.5F;color.GetFunctionForAnalysis().SetStateForAnalysis(s);
            bool first=true;for(float delta:{.25F,.5F,.75F,-.5F,1.F})
            {std::uint32_t argb=0;Check(color.EvaluateColorForAnalysis(delta,argb),"scalar-driven color");if(!first)states<<',';first=false;states<<'['<<delta<<','<<argb<<','<<color.GetFunctionForAnalysis().GetStateForAnalysis().time<<']';}
        }
        else if(mode.rfind("codec",0)==0)
        {
            if(mode=="codec-values"){std::uint8_t id=0;for(auto raw:{0x80402010u,0xff112244u,8u,Bits(2),Bits(1.5F),Bits(.125F),Bits(-.25F),Bits(.5F)})Field(input,id++,raw);}
            if(mode=="codec-repeat"){input={0xaf,6,'i','g','n','o','r','e'};Field(input,0,0x12345678);Field(input,0,0x80402010);Field(input,3,Bits(-.5F));}
            if(mode=="codec-zero"){Field(input,3,0);Field(input,5,0x80000000);}
            if(mode=="codec-nan"){Field(input,4,0x7fc12345);Field(input,6,0x7fc54321);}input.push_back(0);
            spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);spColorFuncEvalSerializer codec;spMemoryStream source;Open(source,input);std::string error;
            Check(codec.ReadPayloadForAnalysis(context,source,static_cast<std::uint32_t>(input.size()),color,&error),error.c_str());spMemoryStream destination;Open(destination);Check(codec.WritePayloadForAnalysis(destination,color,&error),error.c_str());
            std::uint32_t size=0;Check(destination.GetSize(&size),"writer size");const auto* p=static_cast<const std::uint8_t*>(destination.GetBuffer());output.assign(p,p+size);
        }
        states<<']';std::ostringstream row;row<<"[\""<<mode<<"\",\""<<Hex(input)<<"\","<<states.str()<<",\""<<Hex(output)<<"\"]";return row.str();
    }
    void Guards()
    {
        spColorFuncEval color;std::uint32_t output=0;Check(color.EvaluateColorForAnalysis(0,output)&&output==0xfe000000,"default black alpha is254 due separate truncation");
        auto state=color.GetFunctionForAnalysis().GetStateForAnalysis();state.yOffset=std::numeric_limits<float>::quiet_NaN();color.GetFunctionForAnalysis().SetStateForAnalysis(state);output=123;
        Check(!color.EvaluateColorForAnalysis(0,output)&&output==123,"unsafe finite contract leaves output untouched");
        for(const Bytes bytes:{Bytes{0xa0,1,4,0},Bytes{0xa3,4,0,0,0},Bytes{0,0}})
        {spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);spMemoryStream stream;Open(stream,bytes);spColorFuncEvalSerializer codec;std::string error;Check(!codec.ReadPayloadForAnalysis(context,stream,static_cast<std::uint32_t>(bytes.size()),color,&error)&&context.failed,"common envelope guards reject malformed color fields");}
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--color"){std::cout<<Run(argv[2])<<'\n';return 0;}
        for(const auto* mode:{"factory","clone","blend","blend-white","blend-default","codec-default","codec-values","codec-repeat","codec-zero","codec-nan"})(void)Run(mode);
        for(unsigned type=0;type<10;++type)(void)Run("type"+std::to_string(type));Guards();std::cout<<"PASS "<<checks<<'/'<<checks<<": PC ColorFunc runtime/shared scalar codec\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
