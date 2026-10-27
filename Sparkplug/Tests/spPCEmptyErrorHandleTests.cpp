#include "Code/SparkplugPC/spPCErrorManager.h"
#include <iostream>
#include <stdexcept>
#include <string>

using namespace sparkplug::reconstruction;
namespace
{
    using Context=spPCErrorManager::WarmEmptyContextForAnalysis;
    using Status=spPCErrorManager::WarmEmptyHandleStatusForAnalysis;
    void Check(bool value,const char* message)
    { if(!value)throw std::runtime_error(message); }
    void Replay()
    {
        std::uint32_t initial=0;unsigned repeats=0;
        while(std::cin>>initial>>repeats)
        {
            Check(repeats==3,"three observed warm empty reference gates");
            spPCErrorManager manager;auto raw=initial;
            for(unsigned i=0;i<repeats;++i)
            {
                const auto before=raw;
                const auto status=manager.HandleWarmEmptyPcForAnalysis(raw,Context::StableBorrowed);
                Check(status==Status::Completed&&!manager.HadFatalForAnalysis()&&!manager.HadDataOverflowForAnalysis(),"analytical completion and portable profile flags");
                std::cout<<before<<' '<<raw<<' '<<manager.GetDataUsedForAnalysis()<<' '
                    <<(manager.GetHeadForAnalysis()!=nullptr)<<'\n';
            }
        }
    }
    void Contracts()
    {
        {
            spPCErrorManager manager;std::uint32_t raw=0x12345678;
            Check(!manager.HandleForAnalysis(spErrorSeverity::Error),"shared empty Handle false retained");
            Check(manager.HandleWarmEmptyPcForAnalysis(raw,Context::UnqualifiedMutable)==Status::UnqualifiedContext&&raw==0x12345678,"unknown/mutable borrow refuses before raw write");
            Check(manager.HandleWarmEmptyPcForAnalysis(raw,Context::StableBorrowed)==Status::Completed&&raw==0,"analytical completion differs from native empty false");
            raw=0xFFFFFFFF;
            Check(manager.HandleWarmEmptyPcForAnalysis(raw,Context::StableBorrowed)==Status::Completed&&raw==0,"repeated warm gate clears all raw bits");
        }
        {
            spPCErrorManager old;std::uint32_t raw=123;
            {
                spPCErrorManager current;
                Check(old.HandleWarmEmptyPcForAnalysis(raw,Context::StableBorrowed)==Status::NotCurrentManager&&raw==123,"noncurrent manager refuses before effects");
            }
            Check(spErrorManager::GetInstance()==nullptr,"existing unconditional singleton teardown retained");
            Check(old.HandleWarmEmptyPcForAnalysis(raw,Context::StableBorrowed)==Status::NotCurrentManager&&raw==123,"absent singleton is not acquired");
        }
        {
            spPCErrorManager manager;std::uint32_t raw=123;
            auto* error=manager.CreateErrorForAnalysis(spErrorSeverity::Error,1,nullptr,0);
            Check(error!=nullptr,"queued fixture");const auto used=manager.GetDataUsedForAnalysis();
            Check(manager.HandleWarmEmptyPcForAnalysis(raw,Context::StableBorrowed)==Status::NonEmptyOrFlaggedState&&raw==123&&manager.GetHeadForAnalysis()==error&&manager.GetDataUsedForAnalysis()==used,"queued route refuses without clearing or dispatch");
        }
        {
            spError error;spPCErrorManager manager;std::uint32_t raw=123;
            Check(manager.StoreMessageForAnalysis(error,"x"),"unlinked occupied stack fixture");
            Check(manager.GetHeadForAnalysis()==nullptr&&manager.GetDataUsedForAnalysis()==2,"used guard independent of head");
            Check(manager.HandleWarmEmptyPcForAnalysis(raw,Context::StableBorrowed)==Status::NonEmptyOrFlaggedState&&raw==123&&manager.GetDataUsedForAnalysis()==2,"nonzero used refuses before effects");
        }
        {
            spError error;spPCErrorManager manager;std::uint32_t raw=123;
            const std::string tooLarge(manager.GetDataCapacityForAnalysis(),'x');
            Check(!manager.StoreMessageForAnalysis(error,tooLarge.c_str())&&manager.HadDataOverflowForAnalysis(),"existing overflow fixture");
            Check(manager.HandleWarmEmptyPcForAnalysis(raw,Context::StableBorrowed)==Status::NonEmptyOrFlaggedState&&raw==123&&manager.GetDataUsedForAnalysis()==0,"empty overflow state refuses before effects");
        }
        {
            spError fatal(spErrorSeverity::Fatal,0,nullptr,0);spPCErrorManager manager;std::uint32_t raw=123;
            manager.LinkErrorForAnalysis(fatal);
            Check(manager.HandleForAnalysis(spErrorSeverity::Error)&&manager.HadFatalForAnalysis(),"portable nonmodal handler establishes fatal fixture; no native modal parity claim");
            Check(manager.GetHeadForAnalysis()==nullptr&&manager.GetDataUsedForAnalysis()==0,"fatal guard independent of queue");
            Check(manager.HandleWarmEmptyPcForAnalysis(raw,Context::StableBorrowed)==Status::NonEmptyOrFlaggedState&&raw==123&&manager.HadFatalForAnalysis(),"past fatal refuses without normalizing flags");
        }
    }
}
int main(int argc,char** argv)
{
    try {if(argc==2&&std::string(argv[1])=="--replay")Replay();else {Contracts();std::cout<<"PC warm empty error adapter contracts passed\n";}return 0;}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
