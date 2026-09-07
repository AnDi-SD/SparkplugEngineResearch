#include "Code/SparkplugDX/spDXRenderer.h"
#include "Code/SparkplugDX/spDXMaterial.h"
#include "Code/Sparkplug/spMaterialColorController.h"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace
{
    int checks=0;void Check(bool ok,const char* text){++checks;if(!ok)throw std::runtime_error(text);}
    struct Sink
    {
        spDXRenderer::SceneStateForAnalysis* state=nullptr;
        struct Event{bool begin;std::uint32_t count,word;std::uint8_t active;};
        std::array<Event,4> events{};unsigned size=0;bool fail=false;
        static std::int32_t Call(void* data,bool begin) noexcept
        {auto& s=*static_cast<Sink*>(data);if(s.size<4)s.events[s.size++]={begin,s.state->counter40,s.state->resetWord4C,s.state->activeCBC0};return s.fail?-1:0;}
    };
    std::string Run(const std::string& mode)
    {
        spDXRenderer::SceneStateForAnalysis state{mode=="wrap"?0xffffffffu:0u,88,7};Sink sink;sink.state=&state;sink.fail=mode.find("-fail")!=std::string::npos;
        auto controller=std::make_shared<spMaterialColorController>();spDXMaterial material;
        if(mode=="material"){material.SetMaterialColorControllerForAnalysis(controller);auto s=controller->GetAlphaForAnalysis().GetStateForAnalysis();s.functionType=8;s.pitch=1;controller->GetAlphaForAnalysis().SetStateForAnalysis(s);}
        std::vector<bool> sequence;
        if(mode=="material")sequence={true,false,true,false};
        else sequence.assign(mode.rfind("double-",0)==0?2:1,mode.find("begin")!=std::string::npos);
        std::ostringstream states;states<<std::setprecision(17)<<std::boolalpha<<'[';bool first=true;
        for(bool begin:sequence)
        {
            if(mode=="material"&&begin)controller->ApplyForAnalysis(.25F);
            const bool result=begin?spDXRenderer::BeginSceneForAnalysis(state,&Sink::Call,&sink):spDXRenderer::EndSceneForAnalysis(state,&Sink::Call,&sink);Check(result,"native successful scene wrapper result");
            if(!first)states<<',';first=false;states<<"[\""<<(begin?"begin":"end")<<"\",1,"<<state.counter40<<','<<state.resetWord4C<<','<<unsigned(state.activeCBC0);
            if(mode=="material")
            {bool called=false;Check(material.UpdateColorForFrameForAnalysis(state.counter40,false,&called),"actual scene counter drives material gate");states<<','<<called<<','<<material.GetOpaqueRuntimeFieldForAnalysis()<<','<<material.GetDiffuseColorForAnalysis()[3]<<",["<<controller->GetAppliedTimeForAnalysis()<<','<<controller->GetAccumulatedTimeForAnalysis()<<']';}
            states<<']';
        }
        states<<']';Check(sink.size==sequence.size(),"every wrapper invokes device including duplicates/failures");
        std::ostringstream out;out<<"[\""<<mode<<"\",[";
        for(unsigned i=0;i<sink.size;++i){if(i)out<<',';const auto& e=sink.events[i];out<<"[\""<<(e.begin?"begin":"end")<<"\","<<e.count<<','<<e.word<<','<<unsigned(e.active)<<']';}
        out<<"],"<<states.str()<<']';return out.str();
    }
    struct OutputSink
    {
        std::array<std::array<std::uint32_t,4>,260> clears{};unsigned count=0,cooperative=0;bool fail=false,lost=false,reset=false;
        static std::int32_t Clear(void* data,std::uint32_t flags,std::uint32_t argb,float depth,std::uint32_t stencil) noexcept
        {auto& s=*static_cast<OutputSink*>(data);if(s.count<260)s.clears[s.count++]={flags,argb,depth==1.F?0x3f800000u:0u,stencil};return s.fail?-1:0;}
        static std::int32_t Present(void* data,bool test) noexcept
        {auto& s=*static_cast<OutputSink*>(data);if(test){++s.cooperative;return static_cast<std::int32_t>(s.reset?0x88760869u:0x88760868u);}++s.count;return static_cast<std::int32_t>(s.lost?0x88760868u:s.fail?0x80004005u:0u);}
    };
    std::string PresentClear(const std::string& mode)
    {
        OutputSink sink;sink.fail=mode.find("-fail")!=std::string::npos;sink.lost=mode.rfind("lost",0)==0;sink.reset=mode=="lost-reset-stop";
        std::ostringstream events,states;events<<'[';states<<'[';
        if(mode.rfind("clear",0)==0)
        {
            std::vector<std::uint32_t> flags;for(unsigned i=0;i<256;++i)flags.push_back(i);for(auto value:{0x100u,0x107u,0x80000000u,0xffffffffu})flags.push_back(value);
            bool first=true;for(auto flag:flags)
            {Check(spDXRenderer::ClearForAnalysis(flag,0x11223344,0x55667788,&OutputSink::Clear,&sink),"Clear ignores HRESULT");if(!first)states<<',';first=false;states<<'['<<flag<<",1]";}
            Check(sink.count==260,"full low-byte flag matrix");
            for(unsigned i=0;i<sink.count;++i){if(i)events<<',';events<<"[\"clear\"";for(auto value:sink.clears[i])events<<','<<value;events<<']';}
        }
        else
        {
            std::array<std::uint32_t,8> parameters{};for(unsigned i=0;i<8;++i)parameters[i]=0x100+i;
            const auto result=spDXRenderer::PresentBeforeResetForAnalysis(mode=="blocked"?1u:0u,parameters,&OutputSink::Present,&sink);
            if(sink.count)events<<"[\"present\"]";if(sink.cooperative)events<<",[\"cooperative\"]";
            if(mode=="lost-reset-stop")
            {Check(!result.completed&&result.resetRequired,"reset boundary is not successful native completion");states<<"[\"stopped\",[";for(unsigned i=0;i<8;++i){if(i)states<<',';states<<result.resetParameters[i];}states<<"]]";}
            else{Check(result.completed&&!result.resetRequired,"bounded Present branch completed");states<<unsigned(result.nativeResult);}
        }
        states<<']';events<<']';return "[\""+mode+"\","+events.str()+","+states.str()+"]";
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--scene"){std::cout<<Run(argv[2])<<'\n';return 0;}
        if(argc==3&&std::string(argv[1])=="--present-clear"){std::cout<<PresentClear(argv[2])<<'\n';return 0;}
        for(const auto* mode:{"begin","end","double-begin","double-end","begin-fail","end-fail","wrap","material"})(void)Run(mode);
        for(const auto* mode:{"clear","clear-fail","present","present-fail","blocked","lost","lost-reset-stop"})(void)PresentClear(mode);
        spDXRenderer::SceneStateForAnalysis state{3,7,9};Check(!spDXRenderer::BeginSceneForAnalysis(state,nullptr,nullptr)&&state.resetWord4C==7&&state.activeCBC0==9,"NULL begin host guard leaves state");
        Check(!spDXRenderer::EndSceneForAnalysis(state,nullptr,nullptr)&&state.counter40==3&&state.activeCBC0==9,"NULL end host guard leaves state");
        Check(!spDXRenderer::ClearForAnalysis(7,0,0,nullptr,nullptr),"NULL Clear host guard");
        Check(!spDXRenderer::PresentBeforeResetForAnalysis(0,{},nullptr,nullptr).completed,"missing device cannot be reported as completed");
        std::cout<<"PASS "<<checks<<'/'<<checks<<": PC renderer scene wrappers/material counter\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
