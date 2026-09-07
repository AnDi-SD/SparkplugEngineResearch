#include "Code/SparkplugDX/spDXRenderer.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace
{
    using R=spDXRenderer;int checks=0;
    void Check(bool value,const char* label){++checks;if(!value)throw std::runtime_error(label);}
    template<class T>void Array(std::ostream& o,const T& values){o<<'[';bool first=true;for(const auto& v:values){if(!first)o<<',';first=false;o<<v;}o<<']';}
    struct Sink
    {
        R::MatrixStateForAnalysis state;bool fail=false;std::vector<std::string> events;
        std::string State()const{std::ostringstream o;o<<'['<<state.dirty<<",[";for(unsigned i=0;i<3;++i){if(i)o<<',';Array(o,state.inputs[i]);}o<<"]]";return o.str();}
        static std::int32_t Submit(void* ptr,unsigned index,const R::MatrixStateForAnalysis::RawMatrix& matrix)noexcept
        {auto& s=*static_cast<Sink*>(ptr);std::ostringstream o;o<<'['<<index<<',';Array(o,matrix);o<<','<<s.State()<<']';s.events.push_back(o.str());return s.fail?-1:0;}
    };
    std::string Run(const std::string& mode)
    {
        Sink sink;sink.fail=mode=="failed";std::ostringstream out;out<<"[\""<<mode<<"\",[";
        for(unsigned ordinal=0;ordinal<6;++ordinal)
        {
            const unsigned index=mode=="failed"||mode=="alias"?ordinal%3:mode=="world"?0:mode=="view"?1:2;
            R::MatrixStateForAnalysis::RawMatrix input;
            for(unsigned i=0;i<16;++i)input[i]=ordinal<3?0x3f000000+0x12345*i:i==0?0x7fc00000:0xff000000+i;
            sink.state.dirty=false;sink.events.clear();const auto& matrix=mode=="alias"?sink.state.inputs[index]:input;
            const bool result=R::SetInputMatrixForAnalysis(sink.state,index,matrix,0xdeadbeef,Sink::Submit,&sink);
            Check(result&&R::GetInputMatrixForAnalysis(sink.state,index)==&sink.state.inputs[index],"original setter/raw getter");
            if(ordinal)out<<',';out<<'['<<ordinal<<','<<index<<','<<result<<','<<sink.State()<<",[";
            for(unsigned i=0;i<sink.events.size();++i){if(i)out<<',';out<<sink.events[i];}out<<"]]";
        }
        out<<"]]";return out.str();
    }
}
int main(int argc,char** argv)
{
    try{if(argc==3&&std::string(argv[1])=="--case"){std::cout<<Run(argv[2])<<'\n';return 0;}
        for(const auto* mode:{"world","view","projection","failed","alias"})(void)Run(mode);
        Sink sink;Check(!R::SetInputMatrixForAnalysis(sink.state,3,{},0,Sink::Submit,&sink)&&sink.events.empty(),"invalid index guarded");
        std::cout<<"PASS "<<checks<<'/'<<checks<<": renderer matrix inputs\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
