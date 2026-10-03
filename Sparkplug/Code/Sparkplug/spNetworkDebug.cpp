#include "spNetworkDebug.h"
#include <stdexcept>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::shared_ptr<spNetworkDebugHost> FactoryHost;
        std::string Hex(std::uint32_t value,unsigned width=0)
        {
            constexpr char Digits[]="0123456789ABCDEF";
            std::string result;
            do{result.insert(result.begin(),Digits[value&15]);value>>=4;}while(value);
            while(result.size()<width)result.insert(result.begin(),'0');
            return result;
        }
    }
    void SetNetworkDebugFactoryHostForAnalysis(std::shared_ptr<spNetworkDebugHost> host){FactoryHost=std::move(host);}
    std::shared_ptr<spNetworkDebugHost> GetNetworkDebugFactoryHostForAnalysis(){return FactoryHost;}
    spNetworkDebug::spNetworkDebug(std::shared_ptr<spNetworkDebugHost> host):host_(std::move(host))
    {
        if(!host_)throw std::logic_error("NetworkDebug host is missing");
        lock_=host_->CreateLock();
        state_.start=host_->TimeGetTime();
    }
    spNetworkDebug::~spNetworkDebug(){StopForAnalysis();}
    const spRTTIRecord& spNetworkDebug::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID,spBaseObject::ClassID,"spNetworkDebug",&spBaseObject::StaticRTTI(),
            +[]()->std::unique_ptr<spBaseObject>{return std::make_unique<spNetworkDebug>();},nullptr};
        static const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;return record;
    }
    const spRTTIRecord& spNetworkDebug::vfunc_18() const noexcept{return StaticRTTI();}
    std::unique_ptr<spBaseObject> spNetworkDebug::vfunc_10(spCloneManager& manager) const
    {
        // Native factory uses the current global environment, independently of
        // the source. Applications must configure it for parameterless Clone.
        auto clone=std::make_unique<spNetworkDebug>();
        manager.RegisterCloneForAnalysis(*this,*clone);
        return vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
    void spNetworkDebug::vfunc_0C(const void* notification) noexcept
    {
        // Native dereferences a uint32 notification code; null is outside its
        // domain. Portable protection does not establish native null behavior.
        if(notification&&*static_cast<const std::uint32_t*>(notification)==0x1a)RenderFrameRateForAnalysis();
    }
    bool spNetworkDebug::StartForAnalysis()
    {
        state_.start=host_->TimeGetTime();state_.timerActive=1;
        if(!lock_)lock_=host_->CreateLock();
        return true;
    }
    void spNetworkDebug::StopTimer() noexcept
    {
        // Default embedded spTimer has limited=0. Stop does not test active.
        state_.accumulated+=host_->TimeGetTime()-state_.start;
        state_.timerActive=0;
    }
    void spNetworkDebug::StopForAnalysis() noexcept
    {
        StopTimer();
        if(stream_){if(state_.fileEnabled)(void)stream_->Close();stream_.reset();}
        state_.fileEnabled=0;
        if(lock_){host_->FreeLockStorage(lock_);lock_=0;}
        if(state_.subscribed){host_->Unsubscribe(*this);state_.subscribed=0;}
    }
    void spNetworkDebug::SetFileOutputForAnalysis(std::uint8_t enabled,const std::string& suffix)
    {
        if(enabled)
        {
            state_.fileEnabled=1;
            if(stream_)return; // Re-enabling a retained closed stream does not reopen it.
            const std::string path=host_->LogDirectory()+suffix;
            if(path.size()>=200)throw std::length_error("Native NetworkDebug path exceeds its 200-byte buffer");
            stream_=host_->CreateStream();
            if(!stream_)throw std::logic_error("Native NetworkDebug null stream is outside host scope");
            (void)stream_->Open(path,4);
            stream_->Seek(1,0);
        }
        else
        {
            state_.fileEnabled=0;
            if(stream_)(void)stream_->Close();
        }
    }
    void spNetworkDebug::RenderFrameRateForAnalysis()
    {
        if(state_.timerActive){StopTimer();state_.start=host_->TimeGetTime();state_.timerActive=1;}
        const auto divisor=host_->TimerDivisor();
        if(!divisor)throw std::logic_error("NetworkDebug timer divisor is zero");
        const std::uint32_t elapsed=state_.accumulated/divisor;
        const float rate=static_cast<float>(1000.0/static_cast<double>(elapsed));
        state_.accumulated=0;state_.start=host_->TimeGetTime();state_.timerActive=1;
        const std::string text=host_->FormatFrameRate(rate);
        if(!state_.context)throw std::logic_error("NetworkDebug frame context is missing");
        const auto target=host_->ReadContextField14(state_.context)+0x190;
        host_->DrawFrameRate(600,target,text,0);
    }
    bool spNetworkDebug::CanLog(std::int32_t level) const noexcept
    {return level<=state_.level&&(state_.consoleEnabled||state_.fileEnabled);}
    void spNetworkDebug::LogForAnalysis(const char* category,std::int32_t level,const char* format,...)
    {
        if(!CanLog(level))return;
        if(!lock_)throw std::logic_error("NetworkDebug logging requires its native lock");
        const spNetworkDebugOutputLockForAnalysis outputLock(*host_,lock_);
        std::va_list arguments;va_start(arguments,format);
        std::string text;
        try{text=host_->FormatMessage(format,arguments,8191);}catch(...){va_end(arguments);throw;}
        va_end(arguments);EmitLine(category,text);
    }
    void spNetworkDebug::LogTextForAnalysis(const std::string& category,std::int32_t level,const std::string& text)
    {
        if(!CanLog(level))return;
        if(text.size()>=8191)throw std::length_error("Unterminated native NetworkDebug format is outside host scope");
        if(!lock_)throw std::logic_error("NetworkDebug logging requires its native lock");
        const spNetworkDebugOutputLockForAnalysis outputLock(*host_,lock_);
        EmitLine(category,text);
    }
    void spNetworkDebug::EmitLine(const std::string& category,const std::string& text)
    {
        if(text.size()>=8191)throw std::length_error("Unterminated native NetworkDebug format is outside host scope");
        const auto clock=host_->ReadEngineClock();
        if(!clock.divisor)throw std::logic_error("NetworkDebug clock divisor is zero");
        const std::uint32_t timestamp=clock.rawTicks/clock.divisor+clock.bias;
        const std::int64_t signedTimestamp=timestamp<0x80000000u?timestamp:std::int64_t(timestamp)-0x100000000LL;
        state_.lastLine=category+" : "+std::to_string(signedTimestamp)+" : "+text+"\n";
        if(state_.fileEnabled)
        {
            if(!stream_)throw std::logic_error("NetworkDebug enabled file has no stream");
            stream_->WriteLine(state_.lastLine,0);
        }
        if(state_.consoleEnabled)host_->ConsoleOutput(state_.lastLine);
    }
    void spNetworkDebug::DumpPacketForAnalysis(const PacketForAnalysis& packet)
    {
        if(state_.level<2)return;
        if(packet.data.size()>0xffff)throw std::length_error("NetworkDebug packet size exceeds uint16");
        auto line=[&](const std::string& text){if(auto* debug=host_->PacketDebug())debug->LogTextForAnalysis("spNetworkPacket",2,text);};
        line("---------------------------------------------");
        line("Source:      "+Hex(packet.source));line("Destination: "+Hex(packet.destination));
        line("Type:        "+std::to_string(packet.type));line("Data Size:   "+std::to_string(packet.data.size()));
        std::string data;for(auto value:packet.data)data+=Hex(value,2)+" ";
        line("Data:        "+data);line("---------------------------------------------");
    }
    spNetworkDebug::StateForAnalysis spNetworkDebug::GetStateForAnalysis() const
    {auto result=state_;result.hasStream=bool(stream_);result.hasLock=lock_!=0;return result;}
    void spNetworkDebug::SetControlsForAnalysis(std::uint8_t console,std::uint8_t unknown12,
        std::uint8_t subscribed,std::uintptr_t context,std::int32_t level) noexcept
    {state_.consoleEnabled=console;state_.unknown12=unknown12;state_.subscribed=subscribed;state_.context=context;state_.level=level;}
}
