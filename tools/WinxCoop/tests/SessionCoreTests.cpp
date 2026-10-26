#include "../src/Networking/SessionCore.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace winxcoop::networking;
using namespace sparkplug::reconstruction;
namespace
{
    void Check(const bool condition, const char* description)
    {
        if(!condition) throw std::runtime_error(description);
    }
    std::vector<std::uint8_t> Bytes(const char* text)
    {
        std::string compact;
        for(const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p; ++p)
            if(!std::isspace(*p)) compact += static_cast<char>(*p);
        Check(compact.size() % 2 == 0, "complete literal wire fixture");
        std::vector<std::uint8_t> result;
        for(std::size_t i = 0; i < compact.size(); i += 2)
            result.push_back(static_cast<std::uint8_t>(std::stoul(compact.substr(i,2), nullptr, 16)));
        return result;
    }
    // Independent golden bytes, deliberately not made by either SessionCore.
    const auto Probe = Bytes("d0 ba 00 00 00 00 01 00 00 00 00 00 00 00 00 01 00 2a");
    const auto Info = Bytes("00 00 ff ff 01 00 01 00 00 00 00 00 00 00 00 02 00 01 00");
    const auto Ready = Bytes("01 00 00 00 03 00 01 00 00 00 00 00 00 00 00 01 00 01");
    const auto Start = Bytes("00 00 01 00 04 00 01 00 00 00 00 00 00 00 00 04 00 01 00 34 12");
    const auto Ping5000 = Bytes("01 00 00 00 05 00 01 00 00 00 00 00 00 00 00 08 00 88 13 00 00 00 00 00 00");
    const auto Pong10000 = Bytes("00 00 01 00 06 00 01 00 00 00 00 00 00 00 00 08 00 88 13 00 00 10 27 00 00");
    const auto PingZero = Bytes("01 00 00 00 05 00 01 00 00 00 00 00 00 00 00 08 00 00 00 00 00 00 00 00 00");
    const auto PongZero = Bytes("00 00 01 00 06 00 01 00 00 00 00 00 00 00 00 08 00 00 00 00 00 00 00 00 00");

    SessionConfig HostConfig()
    {
        SessionConfig config;
        config.role = SessionRole::Host;
        config.local = {0x7f000001, 42000};
        return config;
    }
    SessionConfig GuestConfig()
    {
        SessionConfig config;
        config.role = SessionRole::Guest;
        config.local = {0x7f000001, 42001};
        config.server = Endpoint{0x7f000001, 42000};
        return config;
    }
    std::vector<std::uint8_t> Outbound(SessionCore& session)
    {
        auto output = session.TakeOutbound();
        Check(output.has_value(), "expected one outbound frame");
        return std::move(*output);
    }
    void Deliver(SessionCore& target, const std::vector<std::uint8_t>& bytes,
        const std::size_t chunk, const std::uint64_t now)
    {
        Check(chunk != 0, "positive platform fragment size");
        for(std::size_t offset = 0; offset < bytes.size();)
        {
            const auto amount = std::min(chunk, bytes.size() - offset);
            Check(target.Receive(bytes.data() + offset, amount, now) == amount, "valid platform fragment consumed completely");
            Check(target.Phase() != SessionPhase::Failed && target.PendingBytes() <= 273, "valid receive has bounded framing state");
            offset += amount;
        }
    }
    bool Transfer(SessionCore& source, SessionCore& target, const std::size_t chunk, const std::uint64_t now)
    {
        auto frame = source.TakeOutbound();
        if(!frame) return false;
        Deliver(target, *frame, chunk, now);
        return true;
    }
    struct Pair
    {
        SessionCore host{HostConfig()}, guest{GuestConfig()};
        void Attach(const std::uint64_t now)
        { Check(host.Attach(now) && guest.Attach(now), "platform attaches host and guest exactly once"); }
        void Pump(const std::size_t chunk, const std::uint64_t now)
        {
            for(unsigned i = 0; i < 64; ++i)
            {
                const bool toGuest = Transfer(host, guest, chunk, now);
                const bool toHost = Transfer(guest, host, chunk, now);
                if(!toGuest && !toHost) return;
            }
            throw std::runtime_error("bounded protocol pump did not quiesce");
        }
        void Wait(const std::size_t chunk = 500, const std::uint64_t now = 0)
        {
            Attach(now); Pump(chunk, now);
            Check(host.Phase() == SessionPhase::WaitingForHost && guest.Phase() == SessionPhase::WaitingForHost,
                "ready guest waits for explicit host authorization");
            Check(host.IsReady() && guest.IsReady() && host.LocalId() == 0 && guest.LocalId() == 1,
                "explicit two-peer identities and readiness");
        }
        void Run(const std::size_t chunk = 500, const std::uint64_t now = 0)
        {
            Wait(chunk, now);
            Check(host.HostStart(0x1234, now), "host authorizes running session");
            Pump(chunk, now);
            Check(host.Phase() == SessionPhase::Running && guest.Phase() == SessionPhase::Running
                && host.StartToken() == 0x1234 && guest.StartToken() == 0x1234, "both enter the server-assigned session");
        }
    };
    std::vector<std::uint8_t> Wire(const std::uint16_t source, const std::uint16_t destination,
        const std::uint16_t type, const std::vector<std::uint8_t>& data = {},
        const std::uint8_t tcp = 1, const std::uint32_t field1 = 0, const std::uint32_t field2 = 0)
    {
        spNetworkPacket packet;
        Check(data.size() <= 256 && packet.FillForAnalysis(source,destination,type,tcp,field1,field2,
            static_cast<std::uint16_t>(data.size()),data.empty() ? nullptr : data.data()), "shared codec creates test packet");
        spMemoryStream stream;
        Check(stream.Open("WinxCoop session test") && packet.WriteForAnalysis(stream), "shared codec serializes test packet");
        std::uint32_t size = 0;
        Check(stream.GetSize(&size), "shared encoded packet size");
        const auto* bytes = static_cast<const std::uint8_t*>(stream.GetBuffer());
        return {bytes, bytes + size};
    }
    void Failed(const SessionCore& session, const SessionError expected)
    {
        Check(session.Phase() == SessionPhase::Failed && session.Error() == expected && !session.IsReady(),
            "specific session failure is terminal");
        Check(session.OutboundCount() == 0 && session.MessageCount() == 0, "failure clears all unsent frames and undelivered messages");
    }
    void Reject(SessionCore& target, const std::vector<std::uint8_t>& frame, const SessionError error)
    {
        Check(target.Receive(frame.data(),frame.size(),0) == frame.size(), "complete invalid packet is consumed once");
        Failed(target,error);
        Check(target.Receive(Probe.data(),Probe.size(),0) == 0 && !target.Attach(0), "failed connection cannot be resumed");
    }

    void GoldenHandshake()
    {
        Pair pair;
        Check(pair.host.LocalId() == 0 && pair.guest.LocalId() == 0xbad0, "guest starts with original unassigned transport ID");
        Check(!pair.host.SendApplicationMessage(1,2,nullptr,0,0) && !pair.guest.SendApplicationMessage(1,2,nullptr,0,0), "Idle sends do not synthesize traffic");
        pair.Attach(0);
        Check(!pair.host.Attach(0) && !pair.guest.Attach(0), "duplicate attach rejected");
        Check(pair.host.Phase() == SessionPhase::AwaitingProbe && pair.guest.Phase() == SessionPhase::AwaitingInfo,
            "explicit attach phases");
        Check(!pair.host.TakeOutbound(), "host does not start or assign before probe");
        Check(Outbound(pair.guest) == Probe, "independent golden Probe wire");
        Deliver(pair.host,Probe,1,0);
        Check(pair.host.Phase() == SessionPhase::AwaitingReady && Outbound(pair.host) == Info, "independent golden Info wire");
        Deliver(pair.guest,Info,7,0);
        Check(pair.guest.Phase() == SessionPhase::WaitingForHost && Outbound(pair.guest) == Ready, "independent golden Ready wire");
        Deliver(pair.host,Ready,19,0);
        Check(pair.host.Phase() == SessionPhase::WaitingForHost && !pair.host.TakeOutbound(), "Ready cannot authorize start");
        Check(!pair.guest.HostStart(0x9999,1000000) && !pair.guest.StartToken() && !pair.guest.TakeOutbound(),
            "guest host-command rejection emits no traffic or clock mutation");
        Check(!pair.host.HostStart(0,0) && !pair.host.StartToken(), "zero session token rejected");
        Check(pair.host.HostStart(0x1234,0) && Outbound(pair.host) == Start, "independent golden GameStart wire");
        Deliver(pair.guest,Start,500,0);
        Check(pair.guest.Phase() == SessionPhase::Running && pair.guest.StartToken() == 0x1234, "only host frame starts guest");
        Check(pair.host.HostStart(0x1234,0) && !pair.host.HostStart(0x4321,0) && !pair.host.TakeOutbound(),
            "host authorization is idempotent and cannot silently change token");
    }

    void FragmentedEndToEndAndLateJoin()
    {
        for(const std::size_t chunk : {std::size_t(1),std::size_t(7),std::size_t(19),std::size_t(500)})
        {
            Pair pair; pair.Run(chunk);
            std::vector<std::pair<SessionMessage,std::vector<std::uint8_t>>> retained;
            for(const std::size_t size : {std::size_t(0),std::size_t(1),std::size_t(256)})
            {
                std::vector<std::uint8_t> original(size);
                for(std::size_t i = 0; i < size; ++i) original[i] = static_cast<std::uint8_t>(i * 29u + 3u);
                auto input = original;
                Check(pair.guest.SendApplicationMessage(0x12345678,0x89abcdef,input.empty()?nullptr:input.data(),static_cast<std::uint16_t>(size),0),
                    "guest bounded data send");
                std::fill(input.begin(),input.end(),0xff);
                pair.Pump(chunk,0);
                auto message = pair.host.TakeMessage();
                Check(message && message->classId == 0x12345678 && message->key == 0x89abcdef && message->payload == original,
                    "host owns copied exact guest class/key/payload");
                Check(!pair.host.TakeMessage(), "one delivered message exactly once");
                Check(pair.host.SendApplicationMessage(0xfedcba98,0x76543210,original.empty()?nullptr:original.data(),static_cast<std::uint16_t>(size),0),
                    "host bounded data send");
                pair.Pump(chunk,0);
                auto reply = pair.guest.TakeMessage();
                Check(reply && reply->classId == 0xfedcba98 && reply->key == 0x76543210 && reply->payload == original,
                    "guest owns exact independent host message");
                retained.emplace_back(std::move(*message),original);
                retained.emplace_back(std::move(*reply),original);
            }
            pair.host.Close(); pair.guest.Close();
            for(const auto& item : retained)
                Check(item.first.payload == item.second, "returned messages own their bytes after both endpoints close");
            Pair late;
            Check(late.host.HostStart(0x1234,0) && late.host.Phase() == SessionPhase::Idle && !late.host.TakeOutbound(),
                "host authorizes before transport connection without emitting traffic");
            late.Attach(10); late.Pump(chunk,10);
            Check(late.host.Phase() == SessionPhase::Running && late.guest.Phase() == SessionPhase::Running
                && late.guest.StartToken() == 0x1234, "late guest joins preauthorized session after its Ready");
        }
    }

    void StartAndDataOrder()
    {
        Pair pair; pair.Wait();
        Check(!pair.host.SendApplicationMessage(1,2,nullptr,0,0) && !pair.guest.SendApplicationMessage(1,2,nullptr,0,0), "ready state does not permit gameplay data");
        Check(pair.host.HostStart(0x1234,0), "host start before immediate data");
        const std::uint8_t data = 0x77;
        Check(pair.host.SendApplicationMessage(0x11223344,0x55667788,&data,1,0), "host can enqueue data behind start");
        auto combined = Outbound(pair.host);
        Check(combined == Start, "start is first queued frame");
        auto following = Outbound(pair.host); combined.insert(combined.end(),following.begin(),following.end());
        Deliver(pair.guest,combined,500,0);
        auto message = pair.guest.TakeMessage();
        Check(pair.guest.Phase() == SessionPhase::Running && message && message->classId == 0x11223344
            && message->key == 0x55667788 && message->payload == std::vector<std::uint8_t>{0x77}, "coalesced start then data preserves protocol order");
        Pair before; before.Wait(); Reject(before.guest,Wire(0,1,2,{0x77}),SessionError::UnexpectedPacket);
        Pair forged; forged.Wait(); Reject(forged.host,Wire(1,0,4,{1,0,0x34,0x12}),SessionError::UnexpectedPacket);
    }

    void InvalidPackets()
    {
        { Pair p; p.Attach(0); auto bad=Probe; bad[0]=1; Reject(p.host,bad,SessionError::InvalidIdentity); }
        { Pair p; p.Attach(0); auto bad=Probe; bad[2]=1; Reject(p.host,bad,SessionError::InvalidIdentity); }
        { Pair p; p.Attach(0); auto bad=Probe; bad[6]=0; Reject(p.host,bad,SessionError::InvalidPacket); }
        { Pair p; p.Attach(0); Reject(p.host,Wire(0xbad0,0,0,{0x2a},1,1),SessionError::InvalidPacket); }
        { Pair p; p.Attach(0); Reject(p.host,Wire(0xbad0,0,0,{}),SessionError::InvalidPacket); }
        { Pair p; p.Attach(0); Reject(p.host,Wire(0xbad0,0,0,{0x2b}),SessionError::InvalidPacket); }
        { Pair p; p.Attach(0); Reject(p.guest,Wire(0,0xffff,1,{1}),SessionError::InvalidPacket); }
        for(const auto& id : std::vector<std::vector<std::uint8_t>>{{0xd0,0xba}, {0,0}, {2,0}})
        { Pair p; p.Attach(0); Reject(p.guest,Wire(0,0xffff,1,id),SessionError::InvalidAssignment); }
        { Pair p; p.Attach(0); Deliver(p.host,Probe,500,0); Reject(p.host,Wire(1,0,3,{}),SessionError::InvalidPacket); }
        { Pair p; p.Attach(0); Deliver(p.host,Probe,500,0); Reject(p.host,Wire(1,0,3,{0}),SessionError::InvalidPacket); }
        { Pair p; p.Wait(); Reject(p.guest,Wire(0,1,4,{1,0,0x34}),SessionError::InvalidPacket); }
        { Pair p; p.Wait(); Reject(p.guest,Wire(0,1,4,{1,0,0,0}),SessionError::InvalidAssignment); }
        { Pair p; p.Wait(); Reject(p.guest,Wire(0,1,4,{0,0,1,0}),SessionError::InvalidPacket); }
        { Pair p; p.Wait(); Reject(p.host,Wire(1,0,5,{0,0,0,0,1,0,0,0}),SessionError::InvalidPacket); }
        { Pair p; p.Wait(); Reject(p.host,Wire(1,0,5,{0,0,0,0,0,0,0}),SessionError::InvalidPacket); }
        { Pair p; p.Wait(); Reject(p.guest,Wire(0,1,6,{0,0,0,0,0,0,0,0}),SessionError::InvalidPacket); }
        { Pair p; p.Run(); Reject(p.host,Wire(1,0,7),SessionError::UnexpectedPacket); }
        { Pair p; p.Run(); Reject(p.guest,Start,SessionError::UnexpectedPacket); }
        { Pair p; p.Run(); Reject(p.host,Wire(0,0,2),SessionError::InvalidIdentity); }
        { Pair p; p.Run(); Reject(p.guest,Wire(0,0xffff,2),SessionError::InvalidIdentity); }
        { Pair p; p.Attach(0); auto raw=Probe; raw[6]=255; Deliver(p.host,raw,500,0); Check(p.host.Phase()==SessionPhase::AwaitingReady,"nonzero raw TCP byte accepted"); }
        { Pair p; p.Attach(0); auto oversized=Probe; oversized[15]=1; oversized[16]=1;
          Check(p.host.Receive(oversized.data(),oversized.size(),0)==17,"oversized length rejected before payload"); Failed(p.host,SessionError::FramingFailure); }
    }

    void Backpressure()
    {
        { Pair p; p.Run(); const std::uint8_t data=3;
          Check(p.guest.SendApplicationMessage(1,2,&data,1,0) && Transfer(p.guest,p.host,500,0),"queued inbound before outbound failure");
          for(std::size_t i=0;i<SessionCore::MaximumOutboundFrames;++i)
              Check(p.host.SendApplicationMessage(3,4,&data,1,0) && p.host.OutboundCount()==i+1,"outbound budget accepted exactly");
          Check(!p.host.SendApplicationMessage(3,4,&data,1,0),"ninth unsent frame rejected"); Failed(p.host,SessionError::Backpressure); }
        { Pair p; p.Run(); const std::uint8_t data=7;
          Check(p.host.SendApplicationMessage(1,2,&data,1,0),"unsent frame before inbound failure");
          for(std::size_t i=0;i<SessionCore::MaximumInboundMessages;++i)
          { Check(p.guest.SendApplicationMessage(3,4,&data,1,0) && Transfer(p.guest,p.host,500,0),"inbound budget accepts complete packet");
            Check(p.host.MessageCount()==i+1,"inbound count bounded at sixteen"); }
          Check(p.guest.SendApplicationMessage(3,4,&data,1,0),"one further peer frame serialized"); auto frame=Outbound(p.guest);
          Check(p.host.Receive(frame.data(),frame.size(),0)==frame.size(),"overflow packet consumed once"); Failed(p.host,SessionError::Backpressure); }
    }

    void EndAndReconnect()
    {
        for(unsigned operation=0;operation<3;++operation)
        {
            Pair p; p.Run(); const std::uint8_t data=9;
            Check(p.guest.SendApplicationMessage(1,2,&data,1,0) && Transfer(p.guest,p.host,7,0),"inbound before end");
            Check(p.host.SendApplicationMessage(3,4,&data,1,0),"outbound before end");
            if(operation==0) p.host.EndOfInput(0);
            else if(operation==1)
            { Check(p.host.Receive(Ready.data(),10,0)==10 && p.host.PendingBytes()==10,"partial header before EOF"); p.host.EndOfInput(0); }
            else p.host.FailTransport();
            Check(p.host.OutboundCount()==0 && p.host.MessageCount()==0 && !p.host.IsReady(),"every end clears undelivered state");
            if(operation==0) Check(p.host.Phase()==SessionPhase::Closed && p.host.Error()==SessionError::None,"clean EOF closes");
            else Failed(p.host,operation==1?SessionError::FramingFailure:SessionError::TransportFailure);
            Check(!p.host.Attach(0) && p.host.Receive(Probe.data(),Probe.size(),0)==0,"closed transport scope cannot be reattached");
            p.host.Close(); p.host.FailTransport();
            Pair replacement; replacement.Run(1);
            Check(replacement.host.PendingBytes()==0 && replacement.guest.PendingBytes()==0,"new connection objects have no stale fragments");
        }
    }

    void TimeAndHeartbeat()
    {
        { Pair p; p.Wait(); p.host.Tick(40000); Check(p.host.Phase()==SessionPhase::WaitingForHost,"strict timeout accepts exactly40000");
          p.host.Tick(40001); Failed(p.host,SessionError::Timeout); }
        { Pair p; p.Wait(); Check(p.host.Receive(Ping5000.data(),Ping5000.size(),40001)==0,"late packet cannot refresh expired session"); Failed(p.host,SessionError::Timeout); }
        { Pair p; p.Wait(); Check(!p.host.HostStart(1,40001),"late start cannot revive session"); Failed(p.host,SessionError::Timeout); }
        { Pair p; p.Run(); Check(!p.host.SendApplicationMessage(1,2,nullptr,0,40001),"late send cannot evade timeout"); Failed(p.host,SessionError::Timeout); }
        { Pair p; p.Wait(); p.guest.EndOfInput(40001); Failed(p.guest,SessionError::Timeout); }
        { Pair p; p.Wait(); p.host.Tick(1); Check(p.host.Receive(Ping5000.data(),Ping5000.size(),0)==0,"backward receive rejected before parsing"); Failed(p.host,SessionError::ClockMovedBackward); }
        { SessionCore host(HostConfig()); Check(host.HostStart(1,100),"preauthorization establishes monotonic clock");
          Check(!host.Attach(99),"backward attach rejected"); Failed(host,SessionError::ClockMovedBackward); }
        { Pair p; p.Wait(); p.guest.Tick(4999); Check(!p.guest.TakeOutbound(),"heartbeat waits full interval");
          p.guest.Tick(5000); Check(Outbound(p.guest)==Ping5000,"independent golden heartbeat request");
          p.guest.Tick(9999); Check(!p.guest.TakeOutbound(),"only one ping can be pending");
          Deliver(p.host,Ping5000,7,10000); Check(Outbound(p.host)==Pong10000,"independent golden heartbeat echo/serverstamp");
          Deliver(p.guest,Pong10000,1,10000);
          Check(p.host.Phase()==SessionPhase::WaitingForHost && p.guest.Phase()==SessionPhase::WaitingForHost,"heartbeat never starts waiting campaign");
          p.guest.Tick(10000); Check(p.guest.OutboundCount()==1,"echo releases pending-ping slot"); }
        { Pair p; p.Run(); p.guest.Tick(5000); p.Pump(19,5000);
          Check(p.host.Phase()==SessionPhase::Running && p.guest.Phase()==SessionPhase::Running,"heartbeat works while running"); }
        { Pair p; p.Run(); p.guest.Tick(5000); (void)Outbound(p.guest);
          const auto data=Wire(0,1,2,{1}); Deliver(p.guest,data,500,40000);
          p.guest.Tick(45000); Check(p.guest.Phase()==SessionPhase::Running,"pending ping accepts exact40000 deadline independently of fresh data");
          p.guest.Tick(45001); Failed(p.guest,SessionError::Timeout); }
        { Pair p; p.Wait(); Check(p.host.Receive(Ping5000.data(),10,39999)==10,"partial bytes before deadline accepted into bounded buffer");
          p.host.Tick(40001); Failed(p.host,SessionError::Timeout); }
        { Pair p; p.Wait(); p.guest.Tick(5000); (void)Outbound(p.guest);
          auto wrong=Pong10000; wrong[17]^=1; Check(p.guest.Receive(wrong.data(),wrong.size(),5000)==wrong.size(),"wrong echo consumed"); Failed(p.guest,SessionError::InvalidPacket); }
        { constexpr std::uint64_t wrap=std::uint64_t(1)<<32; Pair p; p.Wait(7,wrap-5000);
          p.guest.Tick(wrap); Check(Outbound(p.guest)==PingZero,"uint32 timestamp zero after wrap remains a valid pending value");
          Deliver(p.host,PingZero,1,wrap); Check(Outbound(p.host)==PongZero,"zero timestamp echo preserves native-shaped wire");
          Deliver(p.guest,PongZero,7,wrap); p.guest.Tick(wrap+5000);
          Check(Outbound(p.guest)==Ping5000 && p.guest.Phase()==SessionPhase::WaitingForHost,"uint64 liveness clock survives wire timestamp wrap"); }
        { Pair p; p.Wait(); p.host.Tick(40000); Check(p.host.Receive(Ping5000.data(),Ping5000.size(),40000)==Ping5000.size(),"valid exact-deadline packet refreshes liveness");
          p.host.Tick(80000); Check(p.host.Phase()==SessionPhase::WaitingForHost,"new exact deadline accepted");
          p.host.Tick(80001); Failed(p.host,SessionError::Timeout); }
    }

    void ConfigAndCallerGuards()
    {
        auto reject=[](const SessionConfig& config)
        { bool threw=false; try { SessionCore session(config); } catch(const std::invalid_argument&) { threw=true; }
          Check(threw,"invalid explicit session configuration rejected"); };
        reject(SessionConfig{});
        auto host=HostConfig(); host.local.port=0; reject(host);
        host=HostConfig(); host.local.address=0xffffffff; reject(host);
        host=HostConfig(); host.local.address=0; reject(host);
        host=HostConfig(); host.server=Endpoint{1,1}; reject(host);
        host=HostConfig(); host.heartbeatMilliseconds=0; reject(host);
        host=HostConfig(); host.heartbeatMilliseconds=40000; reject(host);
        host=HostConfig(); host.timeoutMilliseconds=0; reject(host);
        host=HostConfig(); host.role=static_cast<SessionRole>(99); reject(host);
        auto guest=GuestConfig(); guest.server.reset(); reject(guest);
        guest=GuestConfig(); guest.server->port=0; reject(guest);
        guest=GuestConfig(); guest.server->address=0; reject(guest);
        Pair pair; pair.Run(); bool nullInput=false, nullMessage=false, oversized=false;
        try { (void)pair.host.Receive(nullptr,1,0); } catch(const std::invalid_argument&) { nullInput=true; }
        try { (void)pair.host.SendApplicationMessage(1,2,nullptr,1,0); } catch(const std::invalid_argument&) { nullMessage=true; }
        std::vector<std::uint8_t> data(257);
        try { (void)pair.host.SendApplicationMessage(1,2,data.data(),257,0); } catch(const std::invalid_argument&) { oversized=true; }
        Check(nullInput && nullMessage && oversized && pair.host.Phase()==SessionPhase::Running
            && pair.host.OutboundCount()==0 && pair.host.PendingBytes()==0,"programmer guards do not mutate live session");
    }
}

int main()
{
    try
    {
        GoldenHandshake();
        FragmentedEndToEndAndLateJoin();
        StartAndDataOrder();
        InvalidPackets();
        Backpressure();
        EndAndReconnect();
        TimeAndHeartbeat();
        ConfigAndCallerGuards();
        std::cout << "PASS WinxCoop explicit two-peer session contracts, golden wire and bounded failure lifecycle\n";
        return 0;
    }
    catch(const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
