#include "Code/Sparkplug/spNetworkPacket.h"
#include "Code/SparkBase/spMemoryStream.h"
#include "Analysis/PC/spNetworkPacketAbi.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
    std::string Quote(const std::string& v){return '"'+v+'"';}
    std::string Hex(const void* data,std::size_t size)
    {
        const char digits[]="0123456789abcdef";std::string result;
        auto* p=static_cast<const std::uint8_t*>(data);
        for(std::size_t i=0;i<size;++i){result+=digits[p[i]>>4];result+=digits[p[i]&15];}return result;
    }
    std::vector<std::uint8_t> Payload(unsigned size)
    {std::vector<std::uint8_t> p(size);for(unsigned i=0;i<size;++i)p[i]=static_cast<std::uint8_t>(i*31+7);return p;}
    struct Host final:spNetworkPacketHost
    {
        std::vector<std::string> events;
        void ReportFailure(const char* text) override{events.push_back("error:"+std::string(text));}
    };
    struct Stream final:spStream
    {
        Host& h;spMemoryStream memory;unsigned writes=0,failWrite=0;std::string saved;
        std::uint32_t closedSize=0,closedPosition=0;
        explicit Stream(Host& host):h(host){}
        bool Open(const char* n) override{return memory.Open(n);}
        bool Open(std::uint32_t m,const char* n) override{return memory.Open(m,n);}
        bool Close() override
        {
            h.events.push_back("close");Check(memory.GetSize(&closedSize)&&memory.GetCurrentPosition(closedPosition),"state before close");
            saved=Hex(memory.GetBuffer(),closedSize);return memory.Close();
        }
        bool Seek(SeekSource s,std::int32_t o) override{return memory.Seek(s,o);}
        bool GetCurrentPosition(std::uint32_t& p) const override
        {h.events.push_back("get-position");return memory.GetCurrentPosition(p);}
        bool ReadData(void* p,std::uint32_t n) override
        {h.events.push_back("read:"+std::to_string(n));return memory.ReadData(p,n);}
        bool WriteData(const void* p,std::uint32_t n) override
        {h.events.push_back("write:"+std::to_string(n));return ++writes==failWrite?false:memory.WriteData(p,n);}
        bool vfunc_WriteFromStream(spStream* p,std::uint32_t n) override{return memory.vfunc_WriteFromStream(p,n);}
        bool GetSize(std::uint32_t* p) const override
        {h.events.push_back("get-size");return memory.GetSize(p);}
        void* GetBuffer() noexcept override{return memory.GetBuffer();}
    };
    spNetworkPacketHeaderForAnalysis Header(unsigned size)
    {
        spNetworkPacketHeaderForAnalysis h;h.source=0x1234;h.destination=0xfedc;h.packetType=0x8765;h.useTcp=255;
        h.dataField1=0x11223344;h.dataField2=0x80000000;h.size=static_cast<std::uint16_t>(size);return h;
    }
    std::vector<std::uint8_t> Wire(unsigned size)
    {
        std::vector<std::uint8_t> bytes{0x34,0x12,0xdc,0xfe,0x65,0x87,0xff,0x44,0x33,0x22,0x11,0,0,0,0x80,static_cast<std::uint8_t>(size),static_cast<std::uint8_t>(size>>8)};
        auto p=Payload(size);bytes.insert(bytes.end(),p.begin(),p.end());return bytes;
    }
    std::string State(const spNetworkPacket& p)
    {
        const auto& h=p.HeaderForAnalysis();std::ostringstream s;s<<'['<<h.source<<','<<h.destination<<',';
        if(h.packetType)s<<*h.packetType;else s<<"null";s<<',';
        if(h.useTcp)s<<unsigned(*h.useTcp);else s<<"null";
        s<<','<<h.dataField1<<','<<h.dataField2<<','<<h.size<<','<<Quote(Hex(p.KnownPayloadForAnalysis(p.KnownPayloadPrefixForAnalysis()),p.KnownPayloadPrefixForAnalysis()))<<']';return s.str();
    }
    const char* Modes[]={"defaults","clone","copy","roundtrip-0","roundtrip-1","roundtrip-3","roundtrip-16","roundtrip-255","roundtrip-256","roundtrip-zero-fields","truncated-0","truncated-1","truncated-3","truncated-5","truncated-6","truncated-9","truncated-13","truncated-16","truncated-17","truncated-19","write-fail-1","write-fail-2","write-fail-3","write-fail-4","write-fail-5","write-fail-6","write-fail-7","write-fail-8"};
    std::string Run(const std::string& mode)
    {
        Check(std::find(std::begin(Modes),std::end(Modes),mode)!=std::end(Modes),"known packet case");
        auto host=std::make_shared<Host>();SetNetworkPacketFactoryHostForAnalysis(host);
        spNetworkPacket packet;spCloneManager manager;
        Check(packet.IsExactly(spNetworkPacket::ClassID)&&packet.IsKindOf(spBaseObject::ClassID),"packet RTTI");
        std::vector<std::string> states{State(packet)};std::vector<unsigned> results;
        if(mode=="clone"||mode=="copy")
        {
            packet.SetHeaderForAnalysis(Header(3));auto data=Payload(3);packet.SetPayloadForAnalysis(data.data(),data.size());
            auto other=mode=="clone"?packet.vfunc_10(manager):std::make_unique<spNetworkPacket>();
            results.push_back(mode=="clone"?manager.FindClone(packet)==other.get():packet.vfunc_14(*other,manager));
            states.push_back(State(packet));states.push_back(State(static_cast<const spNetworkPacket&>(*other)));
        }
        else if(mode!="defaults")
        {
            Stream stream(*host);const unsigned size=mode=="roundtrip-zero-fields"?3:mode.rfind("roundtrip-",0)==0?std::stoul(mode.substr(10)):3;
            auto wire=Wire(size);
            if(mode=="roundtrip-zero-fields")wire[4]=wire[5]=wire[6]=0;
            if(mode.rfind("write-fail-",0)==0)
            {
                stream.failWrite=std::stoul(mode.substr(11));Check(stream.memory.ResizeAndSetSize(256)&&stream.memory.Reset(),"write setup");
                packet.SetHeaderForAnalysis(Header(size));auto data=Payload(size);packet.SetPayloadForAnalysis(data.data(),size);
                results.push_back(packet.WriteForAnalysis(stream));states.push_back(State(packet));
            }
            else
            {
                const unsigned length=mode.rfind("truncated-",0)==0?std::stoul(mode.substr(10)):static_cast<unsigned>(wire.size());
                Check(stream.memory.ResizeAndSetSize(length),"read setup");if(length)std::memcpy(stream.memory.GetBuffer(),wire.data(),length);
                results.push_back(packet.ReadForAnalysis(stream));states.push_back(State(packet));
                if(mode.rfind("roundtrip-",0)==0)
                {Check(stream.memory.Reset(),"roundtrip rewind");host->events.push_back("reset");results.push_back(packet.WriteForAnalysis(stream));}
            }
            std::uint32_t amount=stream.closedSize,position=stream.closedPosition;
            auto* buffer=stream.memory.GetBuffer();
            if(buffer)Check(stream.memory.GetSize(&amount)&&stream.memory.GetCurrentPosition(position),"final memory state");
            const auto contents=buffer?Hex(buffer,amount):stream.saved;
            states.push_back('['+std::to_string(amount)+','+std::to_string(position)+','+std::to_string(buffer!=nullptr)+','+Quote(contents)+']');
        }
        SetNetworkPacketFactoryHostForAnalysis(nullptr);
        std::ostringstream out;out<<"{\"results\":[";
        for(unsigned i=0;i<results.size();++i){if(i)out<<',';out<<results[i];}out<<"],\"states\":[";
        for(unsigned i=0;i<states.size();++i){if(i)out<<',';out<<states[i];}out<<"],\"events\":[";
        for(unsigned i=0;i<host->events.size();++i){if(i)out<<',';out<<Quote(host->events[i]);}out<<"]}";return out.str();
    }
    void QualifiedGuards()
    {
        auto host=std::make_shared<Host>();spNetworkPacket packet(host);spMemoryStream memory;
        Check(memory.Open("guard"),"guard stream");bool unknown=false;
        try{(void)packet.WriteForAnalysis(memory);}catch(const std::logic_error&){unknown=true;}
        Check(unknown,"unknown original packet type is not invented");
        auto h=Header(257);packet.SetHeaderForAnalysis(h);Check(memory.Reset(),"guard reset");bool oversize=false;
        try{(void)packet.WriteForAnalysis(memory);}catch(const std::length_error&){oversize=true;}
        std::uint32_t position=0;Check(memory.GetCurrentPosition(position)&&position==17&&oversize,"oversize write stops after the original header prefix");
        auto wire=Wire(257);Check(memory.ResizeAndSetSize(static_cast<std::uint32_t>(wire.size())),"oversize input");std::memcpy(memory.GetBuffer(),wire.data(),wire.size());oversize=false;
        try{(void)packet.ReadForAnalysis(memory);}catch(const std::length_error&){oversize=true;}
        Check(memory.GetCurrentPosition(position)&&position==17&&oversize,"oversize read stops after the original header prefix");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--case"){std::cout<<Run(argv[2])<<'\n';return 0;}
        for(const auto* mode:Modes)(void)Run(mode);QualifiedGuards();
        std::cout<<"PASS "<<std::size(Modes)<<" Packet operation cases and explicit guards\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
