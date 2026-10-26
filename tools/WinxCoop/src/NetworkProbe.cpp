// Bounded diagnostic for the real transport, not the game launcher.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "Networking/WindowsTcpTransport.h"
#include <algorithm>
#include <charconv>
#include <deque>
#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace winxcoop::networking;
namespace
{
    constexpr std::uint32_t HostRequest = 0x57434f50, GuestRequest = 0x57434f52, StressClass = 0x57435354;
    std::string Utf8(const wchar_t* value)
    {
        const auto length = WideCharToMultiByte(CP_UTF8,0,value,-1,nullptr,0,nullptr,nullptr);
        if(length <= 0) throw std::runtime_error("Cannot decode command line");
        std::string result(static_cast<std::size_t>(length),0);
        WideCharToMultiByte(CP_UTF8,0,value,-1,result.data(),length,nullptr,nullptr); result.pop_back(); return result;
    }
    std::string Quote(const std::string& value)
    {
        std::string result = "\"";
        for(const auto raw : value)
        {
            const auto c = static_cast<unsigned char>(raw);
            if(c == '"' || c == '\\') { result += '\\'; result += static_cast<char>(c); }
            else if(c < 32) throw std::invalid_argument("Control character in diagnostic string");
            else result += static_cast<char>(c);
        }
        return result + '"';
    }
    std::uint64_t Number(const std::string& value, const std::uint64_t maximum)
    {
        std::uint64_t result = 0;
        const auto parsed = std::from_chars(value.data(),value.data()+value.size(),result);
        if(parsed.ec != std::errc{} || parsed.ptr != value.data()+value.size() || result > maximum)
            throw std::invalid_argument("Invalid bounded integer: " + value);
        return result;
    }
    Endpoint ParseEndpoint(const std::string& value)
    {
        const auto colon = value.rfind(':');
        if(colon == std::string::npos) throw std::invalid_argument("Endpoint must be IPv4:port");
        const auto ip = value.substr(0,colon); std::uint32_t address = 0; std::size_t begin = 0;
        for(unsigned index = 0; index < 4; ++index)
        {
            const auto end = index == 3 ? ip.size() : ip.find('.',begin);
            if(end == std::string::npos || end <= begin) throw std::invalid_argument("Invalid IPv4 endpoint");
            address = (address << 8) | static_cast<std::uint32_t>(Number(ip.substr(begin,end-begin),255)); begin = end+1;
        }
        return {address,static_cast<std::uint16_t>(Number(value.substr(colon+1),65535))};
    }
    std::string Address(const Endpoint value)
    { return std::to_string(value.address>>24)+'.'+std::to_string((value.address>>16)&255)+'.'+std::to_string((value.address>>8)&255)+'.'+std::to_string(value.address&255); }
    std::string EndpointJson(const Endpoint value)
    { return "{\"address\":"+Quote(Address(value))+",\"port\":"+std::to_string(value.port)+'}'; }
    void FreshFile(const std::filesystem::path& path, const std::string& text)
    {
        if(path.empty()) return;
        const auto handle = CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(handle == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot create fresh diagnostic output");
        DWORD written = 0;
        const bool ok = text.size() <= MAXDWORD && WriteFile(handle,text.data(),static_cast<DWORD>(text.size()),&written,nullptr) && written == text.size();
        CloseHandle(handle);
        if(!ok) throw std::runtime_error("Cannot write diagnostic output");
    }
    std::vector<std::uint8_t> Pattern(const std::size_t size, const unsigned role, const std::uint32_t seed = 0)
    {
        std::vector<std::uint8_t> data(size);
        for(std::size_t i = 0; i < size; ++i) data[i] = static_cast<std::uint8_t>(size+i*17+role*31+seed);
        return data;
    }
    std::string Hex(const std::vector<std::uint8_t>& data)
    {
        constexpr char digits[] = "0123456789abcdef"; std::string result; result.reserve(data.size()*2);
        for(auto value : data) { result += digits[value>>4]; result += digits[value&15]; } return result;
    }
    struct Options
    {
        TcpTransportConfig tcp;
        std::filesystem::path report,ready;
        std::uint64_t runMs = 3000, expectedConnections = 1, sleepMs = 1;
        std::optional<std::uint64_t> startDelay;
        std::uint16_t token = 0x1234;
        bool preauthorize = false, exitAfterExchange = false;
        std::vector<std::uint16_t> sizes;
        std::uint32_t stressMessages = 0;
    };
    Options Parse(const int argc, wchar_t** argv)
    {
        Options options; bool role = false, bind = false;
        for(int index = 1; index < argc; ++index)
        {
            const auto name = Utf8(argv[index]);
            if(name == "--preauthorize") { options.preauthorize = true; continue; }
            if(name == "--exit-after-exchange") { options.exitAfterExchange = true; continue; }
            if(index+1 >= argc) throw std::invalid_argument("Missing option value");
            const auto value = Utf8(argv[++index]);
            if(name == "--role") { if(value != "host" && value != "guest") throw std::invalid_argument("Role is host or guest"); options.tcp.role = value == "host" ? SessionRole::Host : SessionRole::Guest; role = true; }
            else if(name == "--bind") { options.tcp.localBind = ParseEndpoint(value); bind = true; }
            else if(name == "--server") options.tcp.server = ParseEndpoint(value);
            else if(name == "--report") options.report = std::filesystem::u8path(value);
            else if(name == "--ready-file") options.ready = std::filesystem::u8path(value);
            else if(name == "--run-ms") options.runMs = Number(value,60000);
            else if(name == "--start-delay-ms") options.startDelay = Number(value,60000);
            else if(name == "--token") options.token = static_cast<std::uint16_t>(Number(value,65535));
            else if(name == "--expect-connections") options.expectedConnections = Number(value,64);
            else if(name == "--send-chunk") options.tcp.maxSendChunk = static_cast<std::size_t>(Number(value,65536));
            else if(name == "--recv-chunk") options.tcp.maxReceiveChunk = static_cast<std::size_t>(Number(value,65536));
            else if(name == "--heartbeat-ms") options.tcp.heartbeatMilliseconds = Number(value,60000);
            else if(name == "--timeout-ms") options.tcp.timeoutMilliseconds = Number(value,60000);
            else if(name == "--connect-timeout-ms") options.tcp.connectTimeoutMilliseconds = Number(value,60000);
            else if(name == "--send-buffer") options.tcp.sendBufferBytes = static_cast<int>(Number(value,1048576));
            else if(name == "--recv-buffer") options.tcp.receiveBufferBytes = static_cast<int>(Number(value,1048576));
            else if(name == "--pump-sleep-ms") options.sleepMs = Number(value,10);
            else if(name == "--stress-messages") options.stressMessages = static_cast<std::uint32_t>(Number(value,32768));
            else if(name == "--echo-sizes")
            {
                std::size_t at = 0;
                while(at <= value.size())
                {
                    const auto end = value.find(',',at); const auto size = static_cast<std::uint16_t>(Number(value.substr(at,end-at),256));
                    if(std::find(options.sizes.begin(),options.sizes.end(),size) != options.sizes.end() || options.sizes.size() >= 3)
                        throw std::invalid_argument("At most three distinct echo sizes");
                    options.sizes.push_back(size); if(end == std::string::npos) break; at = end+1;
                }
            }
            else throw std::invalid_argument("Unknown option: "+name);
        }
        if(!role || !bind || options.report.empty() || !options.runMs || !options.token ||
           (options.preauthorize && options.startDelay) || (options.tcp.role == SessionRole::Guest && (options.preauthorize || options.startDelay)) ||
           (options.exitAfterExchange && options.tcp.role != SessionRole::Guest) ||
           (options.stressMessages && (options.tcp.role != SessionRole::Host || !options.sizes.empty())))
            throw std::invalid_argument("Invalid diagnostic configuration");
        return options;
    }
    struct Event { std::uint64_t id,time; SessionPhase phase; SessionError error; std::optional<std::uint16_t> token; int socketError; TcpCloseReason closeReason; std::string kind; };
    struct Message { std::uint64_t id; std::uint32_t classId,key; std::vector<std::uint8_t> data; bool sent; };
    struct Pending { std::uint64_t id; std::uint32_t classId,key; std::vector<std::uint8_t> data; };
    struct Exchange { bool corpusQueued = false; unsigned requests = 0,replies = 0; };
    class Probe
    {
    public:
        explicit Probe(Options options):options_(std::move(options)),transport_(options_.tcp) {}
        int Run()
        {
            started_ = GetTickCount64();
            try
            {
                if(!transport_.Start(started_)) { exitReason_ = "start-failed"; exitCode_ = 3; }
                else
                {
                    bound_ = transport_.BoundEndpoint();
                    if(options_.preauthorize && !transport_.HostStart(options_.token,started_)) throw std::runtime_error("Host authorization failed");
                    FreshFile(options_.ready,"{\"pid\":"+std::to_string(GetCurrentProcessId())+",\"role\":"+Quote(Role())+",\"bound\":"+EndpointJson(bound_)+"}\n");
                    while(GetTickCount64()-started_ < options_.runMs)
                    {
                        const auto now = GetTickCount64(); transport_.Pump(now); Observe(now);
                        const auto* session = transport_.Session();
                        if(session && options_.tcp.role == SessionRole::Host && session->Phase() == SessionPhase::WaitingForHost)
                        {
                            if(!waitingAt_) waitingAt_ = now;
                            if(options_.startDelay && now-*waitingAt_ >= *options_.startDelay)
                            { if(!transport_.HostStart(options_.token,now)) throw std::runtime_error("Delayed host authorization failed"); Observe(now); }
                        }
                        const auto id = transport_.ActiveConnectionId(); session = transport_.Session();
                        while(!pending_.empty() && pending_.front().id != id) pending_.pop_front();
                        if(id && session && session->Phase() == SessionPhase::Running)
                        {
                            if(exchanges_.size() >= 64 && !exchanges_.count(id)) throw std::runtime_error("Probe connection-record bound");
                            auto& exchange = exchanges_[id];
                            if(!exchange.corpusQueued)
                            {
                                for(auto size : options_.sizes) Queue({id,OwnRequest(),size,Pattern(size,RoleIndex())});
                                exchange.corpusQueued = true;
                            }
                            for(unsigned budget = 0; budget < 8 && stressQueued_ < options_.stressMessages; ++budget)
                            {
                                const auto data = Pattern(256,0,stressQueued_);
                                if(!transport_.SendApplicationMessage(id,StressClass,stressQueued_,data.data(),256,now)) break;
                                ++stressQueued_;
                            }
                        }
                        while(pending_.size() < 16)
                        {
                            auto received = transport_.TakeMessage(); if(!received) break; Handle(*received);
                        }
                        while(!pending_.empty())
                        {
                            const auto& item = pending_.front(); const auto* core = transport_.Session();
                            if(item.id != transport_.ActiveConnectionId() || !core || core->Phase() != SessionPhase::Running) { pending_.pop_front(); continue; }
                            if(!transport_.SendApplicationMessage(item.id,item.classId,item.key,item.data.empty()?nullptr:item.data.data(),static_cast<std::uint16_t>(item.data.size()),now)) break;
                            Record({item.id,item.classId,item.key,item.data,true}); pending_.pop_front();
                        }
                        Observe(now);
                        session = transport_.Session();
                        const bool complete = id && exchanges_.count(id) && !options_.sizes.empty() && exchanges_[id].requests == options_.sizes.size() && exchanges_[id].replies == options_.sizes.size();
                        if(options_.exitAfterExchange && complete && !shutdownRequested_ && pending_.empty() && !transport_.PendingSendBytes() && session && session->Phase() == SessionPhase::Running && !session->OutboundCount())
                        {
                            if(transport_.ShutdownSend()) { shutdownRequested_ = true; exitReason_ = "exchange-draining"; }
                        }
                        if(options_.tcp.role == SessionRole::Guest && (transport_.State() == TcpTransportState::Closed || transport_.State() == TcpTransportState::Failed))
                        {
                            exitReason_ = shutdownRequested_ ? "exchange-complete" : "peer-terminal";
                            const auto terminal = transport_.LastTerminal();
                            if(transport_.State() == TcpTransportState::Failed || !terminal || terminal->sessionError != SessionError::None || terminal->socketError) exitCode_ = 3;
                            break;
                        }
                        if(transport_.State() == TcpTransportState::Failed) { exitReason_ = "transport-failed"; exitCode_ = 3; break; }
                        Sleep(static_cast<DWORD>(options_.sleepMs));
                    }
                    if(exitReason_ == "exchange-draining") { errors_.push_back("No clean EOF after completed exchange"); exitCode_ = 3; }
                    if(transport_.Metrics().connections < options_.expectedConnections) { errors_.push_back("Fewer connections than expected"); exitCode_ = 3; }
                }
            }
            catch(const std::exception& error) { errors_.push_back(error.what()); exitReason_ = "probe-error"; exitCode_ = 3; }
            Observe(GetTickCount64());
            if(bound_.port == 0) bound_ = transport_.BoundEndpoint();
            transport_.Close(); Observe(GetTickCount64());
            if(!errors_.empty()) exitCode_ = 3;
            FreshFile(options_.report,Report());
            std::cout << "WinxCoop TCP diagnostic " << exitReason_ << " exit " << exitCode_ << '\n';
            return exitCode_;
        }
    private:
        std::string Role() const { return options_.tcp.role == SessionRole::Host ? "host" : "guest"; }
        unsigned RoleIndex() const { return options_.tcp.role == SessionRole::Host ? 0u : 1u; }
        std::uint32_t OwnRequest() const { return RoleIndex() == 0 ? HostRequest : GuestRequest; }
        void Queue(Pending item) { if(pending_.size() >= 16) throw std::runtime_error("Probe pending-message bound"); pending_.push_back(std::move(item)); }
        void Record(Message item) { if(messages_.size() >= 512) throw std::runtime_error("Probe message-record bound"); messages_.push_back(std::move(item)); }
        void Handle(const TcpReceivedMessage& received)
        {
            const auto& item = received.message; Record({received.connectionId,item.classId,item.key,item.payload,false});
            const auto otherRequest = RoleIndex() == 0 ? GuestRequest : HostRequest;
            if(item.key > 256) throw std::runtime_error("Unexpected diagnostic payload key");
            if(item.classId == otherRequest)
            {
                if(item.payload != Pattern(item.key,1-RoleIndex())) throw std::runtime_error("Peer request payload mismatch");
                ++exchanges_[received.connectionId].requests;
                const auto* core = transport_.Session();
                if(received.connectionId == transport_.ActiveConnectionId() && core && core->Phase() == SessionPhase::Running)
                    Queue({received.connectionId,item.classId+1,item.key,item.payload});
            }
            else if(item.classId == OwnRequest()+1)
            {
                if(item.payload != Pattern(item.key,RoleIndex())) throw std::runtime_error("Echo reply payload mismatch");
                ++exchanges_[received.connectionId].replies;
            }
            else throw std::runtime_error("Unexpected diagnostic application class");
        }
        void Observe(const std::uint64_t now)
        {
            while(auto event = transport_.TakeSessionEvent())
            {
                if(events_.size() >= 512) throw std::runtime_error("Probe event-record bound");
                events_.push_back({event->connectionId,event->nowMilliseconds,event->sessionPhase,event->sessionError,event->startToken,0,TcpCloseReason::None,"phase"});
            }
            const auto terminal = transport_.LastTerminal();
            if(terminal && (terminal->connectionId != lastTerminalId_ || static_cast<int>(terminal->reason) != lastTerminalReason_))
            {
                if(events_.size() >= 512) throw std::runtime_error("Probe event-record bound");
                events_.push_back({terminal->connectionId,now,terminal->sessionPhase,terminal->sessionError,std::nullopt,terminal->socketError,terminal->reason,"terminal"});
                lastTerminalId_ = terminal->connectionId; lastTerminalReason_ = static_cast<int>(terminal->reason);
            }
        }
        std::string Report() const
        {
            std::ostringstream out; const auto& m = transport_.Metrics();
            out << "{\"pid\":" << GetCurrentProcessId() << ",\"role\":" << Quote(Role()) << ",\"bound\":" << EndpointJson(bound_)
                << ",\"exitReason\":" << Quote(exitReason_) << ",\"lastSocketError\":" << transport_.LastSocketError()
                << ",\"ownedSocketsAfterClose\":" << transport_.OwnedSocketCount() << ",\"events\":[";
            bool first = true;
            for(const auto& e : events_)
            {
                if(!first) out << ','; first = false;
                out << "{\"event\":" << Quote(e.kind) << ",\"connectionId\":" << e.id << ",\"elapsedMs\":" << (e.time >= started_ ? e.time-started_ : 0)
                    << ",\"phase\":" << static_cast<int>(e.phase) << ",\"error\":" << static_cast<int>(e.error) << ",\"socketError\":" << e.socketError << ",\"closeReason\":" << static_cast<int>(e.closeReason) << ",\"startToken\":";
                if(e.token) out << *e.token; else out << "null"; out << '}';
            }
            out << "],\"messages\":["; first = true;
            for(const auto& item : messages_)
            {
                if(!first) out << ','; first = false;
                out << "{\"connectionId\":" << item.id << ",\"direction\":" << Quote(item.sent?"sent":"received") << ",\"stage\":" << Quote(item.sent?"queued":"delivered")
                    << ",\"classId\":" << item.classId << ",\"key\":" << item.key << ",\"payloadHex\":" << Quote(Hex(item.data)) << '}';
            }
            out << "],\"metrics\":{\"accepted\":" << m.accepted << ",\"refused\":" << m.refused << ",\"connections\":" << m.connections
                << ",\"sendCalls\":" << m.sendCalls << ",\"recvCalls\":" << m.receiveCalls << ",\"bytesSent\":" << m.sentBytes << ",\"bytesReceived\":" << m.receivedBytes
                << ",\"shortSendResults\":" << m.nativeShortSendResults << ",\"partialFrameAdvances\":" << m.framePartialAdvances
                << ",\"sendWouldBlock\":" << m.sendWouldBlock << ",\"receiveWouldBlock\":" << m.receiveWouldBlock
                << ",\"socketsOpened\":" << m.socketsOpened << ",\"socketsClosed\":" << m.socketsClosed << ",\"socketCloseErrors\":" << m.socketCloseErrors
                << ",\"wsaStarts\":" << m.wsaStarts << ",\"wsaCleanups\":" << m.wsaCleanups << ",\"wsaCleanupErrors\":" << m.wsaCleanupErrors
                << ",\"remoteEofs\":" << m.remoteEofs << ",\"socketErrors\":" << m.socketErrors << ",\"peakOwnedSockets\":" << m.peakOwnedSockets
                << ",\"droppedSessionEvents\":" << m.droppedSessionEvents << ",\"applicationSendBackpressure\":" << m.applicationSendBackpressure
                << ",\"receivePausedForInbox\":" << m.receivePausedForInbox << "},\"stress\":{\"requested\":" << options_.stressMessages << ",\"queued\":" << stressQueued_ << "},\"probeErrors\":[";
            first = true; for(const auto& error : errors_) { if(!first) out << ','; first = false; out << Quote(error); }
            out << "]}\n"; return out.str();
        }
        Options options_; WindowsTcpTransport transport_; Endpoint bound_{};
        std::uint64_t started_ = 0,lastTerminalId_ = ~std::uint64_t(0); int lastTerminalReason_ = -1;
        std::optional<std::uint64_t> waitingAt_;
        std::vector<Event> events_; std::vector<Message> messages_; std::deque<Pending> pending_;
        std::map<std::uint64_t,Exchange> exchanges_; std::vector<std::string> errors_;
        std::uint32_t stressQueued_ = 0; std::string exitReason_ = "run-limit"; int exitCode_ = 0; bool shutdownRequested_ = false;
    };
}
int wmain(const int argc, wchar_t** argv)
{
    try { return Probe(Parse(argc,argv)).Run(); }
    catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 3; }
}
