#include "Code/SparkplugPS2/spPS2VideoStream.h"
#include "Analysis/PS2/spVideoStreamAbi.h"
#include <iostream>
#include <stdexcept>
#include <string>
using namespace sparkplug::reconstruction;
namespace
{
    unsigned checks=0;
    void Check(bool condition,const char* message)
    {++checks;if(!condition)throw std::runtime_error(message);}
    std::array<unsigned,9> Results(const spPS2VideoStream& video)
    {
        return {video.NativeHook00ForAnalysis(),video.NativeHook08ForAnalysis(),video.NativeHook0CForAnalysis(),
            video.NativeHook10ForAnalysis(),video.NativeHook14ForAnalysis(),video.NativeHook18ForAnalysis(),
            video.NativeHook1CForAnalysis(),video.NativeHook20ForAnalysis(),video.NativeHook24ForAnalysis()};
    }
    void Operation(unsigned seed,bool output)
    {
        unsigned releases=0;
        auto source=std::make_unique<spPS2VideoStream>();
        Check(source->IsExactly(spPS2VideoStream::ClassID)&&source->IsKindOf(spVideoStream::ClassID),"PS2 video identity chain");
        const auto create=spPS2VideoStream::StaticRTTI().factory;
        Check(create&&create()->IsExactly(spPS2VideoStream::ClassID),"non-null original factory");
        Check(source->GetStateForAnalysis().byte18==0&&source->GetStateForAnalysis().byte19==0&&
            source->GetStateForAnalysis().word20==0&&!source->GetOwnedBufferForAnalysis(),"PS2 constructor own defaults");
        source->SetName("PS2 video original name");
        const spVideoStream::StateForAnalysis changed{std::uint8_t(seed),std::uint8_t(seed>>8),seed^0x12345678U};
        source->SetStateForAnalysis(changed);
        auto* raw=new std::uint8_t[16];raw[0]=std::uint8_t(seed);
        source->AdoptBufferForAnalysis({raw,[&](std::uint8_t* p){++releases;delete[] p;}});
        const auto results=Results(*source);source->NativeHook04ForAnalysis();
        Check(results==std::array<unsigned,9>{1,1,1,1,1,1,1,0,0},"complete original PS2 interface leaves");
        Check(source->GetStateForAnalysis().byte18==changed.byte18&&source->GetStateForAnalysis().byte19==changed.byte19&&
            source->GetStateForAnalysis().word20==changed.word20&&source->GetOwnedBufferForAnalysis()==raw&&
            raw[0]==std::uint8_t(seed)&&!releases,"interface leaves preserve payload and buffer");
        auto owner=source->Clone();auto* clone=dynamic_cast<spPS2VideoStream*>(owner.get());
        Check(clone&&clone->GetName()&&std::string(clone->GetName())=="PS2 video original name","Named clone copies actual name");
        Check(clone->GetStateForAnalysis().byte18==0&&clone->GetStateForAnalysis().byte19==0&&
            clone->GetStateForAnalysis().word20==0&&!clone->GetOwnedBufferForAnalysis(),"clone resets own fields/buffer");
        source->SetName("new source name");
        Check(std::string(clone->GetName())=="PS2 video original name","clone preserves independent inherited name");
        owner.reset();Check(!releases,"fresh clone has no shared buffer ownership");
        source.reset();Check(releases==1,"source destructor releases buffer exactly once");
        if(output)
        {std::cout<<'[';for(unsigned i=0;i<results.size();++i){if(i)std::cout<<',';std::cout<<results[i];}std::cout<<"]\n";}
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==2&&std::string(argv[1])=="--batch")
        {unsigned seed,count=0;while(std::cin>>seed){if(++count>128)throw std::runtime_error("bounded PS2 video batch");Operation(seed,true);}return 0;}
        for(auto seed:{0U,1U,0xFFFFFFFFU,0x12345678U})Operation(seed,false);
        std::cout<<"PASS "<<checks<<" PS2 video checks\n";return 0;
    }
    catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
