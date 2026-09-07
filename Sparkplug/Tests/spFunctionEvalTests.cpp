#include "Code/Sparkplug/spFunctionEval.h"
#include "Code/Sparkplug/spFunctionEvalSerializer.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <cmath>
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
    template<class T>void Field(Bytes& out,std::uint8_t id,const T& value)
    {out.push_back(static_cast<std::uint8_t>(0xa0+id));out.push_back(sizeof(T));const auto* p=reinterpret_cast<const std::uint8_t*>(&value);out.insert(out.end(),p,p+sizeof(T));}
    void Open(spMemoryStream& stream,const Bytes& data={})
    {Check(stream.ResizeAndSetSize(static_cast<std::uint32_t>(data.size())),"stream capacity");if(!data.empty())std::memcpy(stream.GetBuffer(),data.data(),data.size());Check(stream.Seek(spStream::SeekSource::essStart,0),"rewind");}
    std::string Hex(const std::uint8_t* data,std::size_t size)
    {std::string out;constexpr char digits[]="0123456789abcdef";for(std::size_t i=0;i<size;++i){out+=digits[data[i]>>4];out+=digits[data[i]&15];}return out;}
    std::string Run(const std::string& mode)
    {
        spFunctionEval evaluator;spFunctionEvalSerializer serializer;spFunctionEval::RandomStateForAnalysis random;
        std::ostringstream states;states<<std::setprecision(17)<<'[';Bytes input{0};std::string written;
        if(mode=="factory")
        {
            auto s=evaluator.GetStateForAnalysis();s.time=2;s.frequency=3;s.reciprocal=4;s.amplitude=5;s.xOffset=6;s.yOffset=7;s.pitch=8;s.clampLimit=9;s.functionType=5;evaluator.SetStateForAnalysis(s);
            auto owner=evaluator.Clone();const auto* copy=dynamic_cast<spFunctionEval*>(owner.get());
            Check(copy&&copy->GetStateForAnalysis().functionType==0&&copy->GetStateForAnalysis().frequency==1&&copy->GetStateForAnalysis().time==0&&copy->GetStateForAnalysis().yOffset==0,"native virtual clone leaves factory defaults");
        }
        else if(mode=="rng")
        {for(unsigned i=0;i<630;++i){if(i)states<<',';states<<random.Next();}}
        else
        {
            const bool type=mode.rfind("type-",0)==0;
            std::uint32_t kind=type?static_cast<std::uint32_t>(std::stoul(mode.substr(5))):mode.rfind("clamp-",0)==0?8u:5u;
            const float frequency=type?1.F:mode=="negative-frequency"?-2.F:mode=="codec-zero-frequency"?0.F:2.F;
            const float amplitude=type?1.F:2.F,xoff=type?0.F:.125F,yoff=type?0.F:3.F;
            const float pitch=mode=="clamp-down"?-2.F:mode=="clamp-zero"?0.F:2.F;
            input.clear();Field(input,0,kind);std::uint8_t id=1;
            for(const float value:{frequency,amplitude,xoff,yoff,pitch})Field(input,id++,value);
            if(mode=="codec-negative-zero")
            {input.clear();for(const auto field:{3,4,5})Field(input,static_cast<std::uint8_t>(field),0x80000000u);}
            if(mode=="codec-nan")
            {input.clear();Field(input,2,0x7fc12345u);Field(input,3,0x7fc54321u);}
            if(mode=="unknown-repeat")
            {input.insert(input.begin(),{0xad,7,'i','g','n','o','r','e','d'});Field(input,4,7.F);}
            input.push_back(0);spMemoryStream source;Open(source,input);
            spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);std::string error;
            Check(serializer.ReadPayloadForAnalysis(context,source,static_cast<std::uint32_t>(input.size()),evaluator,&error),error.c_str());
            if(mode.rfind("clamp-",0)==0)
            {auto s=evaluator.GetStateForAnalysis();s.clampEnabled=true;s.clampLimit=mode=="clamp-down"?1.F:4.F;evaluator.SetStateForAnalysis(s);}
            if(mode.rfind("codec-",0)!=0&&mode!="unknown-repeat")
            {
                unsigned n=0;
                for(const float delta:{0.F,.25F,.25F,.5F,.25F,1.F,-.5F,-2.F,.125F})
                {
                    float value=0;Check(evaluator.EvaluateForAnalysis(delta,value,&random),"finite native scalar path");
                    if(n++)states<<',';states<<'['<<delta<<','<<value<<','<<evaluator.GetStateForAnalysis().time<<']';
                }
            }
            spMemoryStream output;Open(output);Check(serializer.WritePayloadForAnalysis(output,evaluator,&error),"scalar write");
            std::uint32_t size=0;Check(output.GetSize(&size),"writer size");written=Hex(static_cast<const std::uint8_t*>(output.GetBuffer()),size);
        }
        states<<']';std::ostringstream row;row<<"[\""<<mode<<"\",\""<<Hex(input.data(),input.size())<<"\","<<states.str()<<",\""<<written<<"\"]";return row.str();
    }
    void Guards()
    {
        spFunctionEval evaluator;float value=9;
        Check(evaluator.EvaluateForAnalysis(std::numeric_limits<float>::quiet_NaN(),value)&&value==0,"native constant skips delta math");
        auto state=evaluator.GetStateForAnalysis();state.functionType=3;state.frequency=0;state.reciprocal=std::numeric_limits<float>::infinity();evaluator.SetStateForAnalysis(state);
        Check(!evaluator.EvaluateForAnalysis(1,value)&&evaluator.GetStateForAnalysis().time==0,"host rejects nonfinite/zero frequency before mutation");
        state.frequency=1;state.reciprocal=1;state.time=std::numeric_limits<float>::infinity();evaluator.SetStateForAnalysis(state);
        Check(!evaluator.EvaluateForAnalysis(1,value),"infinite runtime clock safely refused");
        // Native zero pitch must not clamp even when constant exceeds limit.
        state={};state.functionType=8;state.yOffset=5;state.clampEnabled=true;state.clampLimit=2;evaluator.SetStateForAnalysis(state);
        Check(evaluator.EvaluateForAnalysis(1,value)&&value==5,"zero pitch excludes directional clamp");
        for(const Bytes bytes:{Bytes{0x60,1,2},Bytes{0},Bytes{0x60,1,0,0,0,0,9}})
        {
            spMemoryStream source;Open(source,bytes);spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);spFunctionEvalSerializer serializer;std::string error;
            const bool result=serializer.ReadPayloadForAnalysis(context,source,static_cast<std::uint32_t>(bytes.size()),evaluator,&error);
            Check(result==(bytes.size()==1),"strict scalar extent/terminator guards");Check(result||context.failed,"failed context stays poisoned");
        }
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--function"){std::cout<<Run(argv[2])<<'\n';return 0;}
        for(const auto* mode:{"factory","rng","type-0","type-1","type-2","type-3","type-4","type-5","type-6","type-7","type-8","type-9","configured","clamp-up","clamp-down","clamp-zero","negative-frequency","codec-zero-frequency","codec-negative-zero","codec-nan","unknown-repeat"})(void)Run(mode);
        Guards();std::cout<<"PASS "<<checks<<'/'<<checks<<": PC scalar function formulas, clock, RNG, shared codec and safety\n";return 0;
    }
    catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
