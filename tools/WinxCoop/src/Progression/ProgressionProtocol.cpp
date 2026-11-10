#include "ProgressionProtocol.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <algorithm>
#include <cstring>
#include <tuple>

namespace winxcoop::progression
{
    namespace
    {
        using sparkplug::reconstruction::spMemoryStream;
        bool Empty(const Digest& value) noexcept
        { return std::all_of(value.begin(),value.end(),[](auto byte){return byte==0;}); }
        bool Connection(const ConnectionIdentity& value) noexcept
        { return value.sessionId && value.scenarioId && value.token; }
        template<class Scalar,class Data>
        bool Fields(spMemoryStream& s,Message& m,std::uint32_t& version,Scalar scalar,Data data)
        {
            auto kind=static_cast<std::uint32_t>(m.kind),reason=static_cast<std::uint32_t>(m.decision.reason);
            auto terminal=static_cast<std::uint32_t>(m.terminal);
            auto& d=m.decision;auto& c=m.channels;
            const bool ok=scalar(s,version)&&scalar(s,kind)&&scalar(s,m.connection.sessionId)&&
                scalar(s,m.connection.scenarioId)&&scalar(s,m.connection.token)&&scalar(s,d.operation)&&
                scalar(s,d.previous.levelEpoch)&&scalar(s,d.previous.generation)&&
                scalar(s,d.next.levelEpoch)&&scalar(s,d.next.generation)&&scalar(s,reason)&&
                scalar(s,d.destination.level)&&scalar(s,d.destination.entry)&&data(s,d.destination.digest.data(),32)&&
                scalar(s,c.sessionId)&&scalar(s,c.inputToken)&&scalar(s,c.worldToken)&&
                scalar(s,c.hostInputNonce)&&scalar(s,c.hostWorldNonce)&&scalar(s,c.guestInputNonce)&&scalar(s,c.guestWorldNonce)&&
                data(s,m.seed.data(),32)&&data(s,m.witness.data(),32)&&scalar(s,m.appliedRevision)&&scalar(s,m.nativeTick)&&
                scalar(s,m.retirementMask)&&scalar(s,terminal);
            m.kind=static_cast<Kind>(kind);d.reason=static_cast<Reason>(reason);m.terminal=static_cast<Terminal>(terminal);
            return ok;
        }
    }
    bool operator==(const SceneIdentity& a,const SceneIdentity& b) noexcept
    { return a.levelEpoch==b.levelEpoch&&a.generation==b.generation; }
    bool operator==(const LevelDescriptor& a,const LevelDescriptor& b) noexcept
    { return a.level==b.level&&a.entry==b.entry&&a.digest==b.digest; }
    bool operator==(const Decision& a,const Decision& b) noexcept
    { return a.operation==b.operation&&a.previous==b.previous&&a.next==b.next&&a.reason==b.reason&&a.destination==b.destination; }
    bool operator==(const ChannelGeneration& a,const ChannelGeneration& b) noexcept
    {
        return std::tie(a.sessionId,a.inputToken,a.worldToken,a.hostInputNonce,a.hostWorldNonce,a.guestInputNonce,a.guestWorldNonce)==
            std::tie(b.sessionId,b.inputToken,b.worldToken,b.hostInputNonce,b.hostWorldNonce,b.guestInputNonce,b.guestWorldNonce);
    }
    bool operator==(const ConnectionIdentity& a,const ConnectionIdentity& b) noexcept
    { return a.sessionId==b.sessionId&&a.scenarioId==b.scenarioId&&a.token==b.token; }
    bool operator==(const Message& a,const Message& b) noexcept
    {
        return a.kind==b.kind&&a.connection==b.connection&&a.decision==b.decision&&a.channels==b.channels&&
            a.seed==b.seed&&a.witness==b.witness&&a.appliedRevision==b.appliedRevision&&a.nativeTick==b.nativeTick&&
            a.retirementMask==b.retirementMask&&a.terminal==b.terminal;
    }
    bool ValidScene(SceneIdentity value) noexcept { return value.levelEpoch&&value.generation; }
    bool ValidDescriptor(const LevelDescriptor& value) noexcept
    { return value.level>=1&&value.level<=50&&!Empty(value.digest); }
    bool ValidChannels(const ChannelGeneration& value) noexcept
    {
        if(!value.sessionId||!value.inputToken||!value.worldToken||value.inputToken==value.worldToken) return false;
        const std::array<std::uint64_t,4> nonces={value.hostInputNonce,value.hostWorldNonce,value.guestInputNonce,value.guestWorldNonce};
        for(std::size_t i=0;i<nonces.size();++i)
        {
            if(!nonces[i]) return false;
            for(std::size_t j=0;j<i;++j)if(nonces[i]==nonces[j])return false;
        }
        return true;
    }
    bool ValidDecision(const Decision& value) noexcept
    {
        return value.operation&&ValidScene(value.previous)&&ValidScene(value.next)&&
            value.previous.levelEpoch!=UINT32_MAX&&value.previous.generation!=UINT32_MAX&&
            value.next.levelEpoch==value.previous.levelEpoch+1&&value.next.generation==value.previous.generation+1&&
            value.reason>=Reason::LevelChange&&value.reason<=Reason::Cutscene&&ValidDescriptor(value.destination);
    }
    bool ValidMessage(const Message& m) noexcept
    {
        if(!Connection(m.connection)||!ValidDecision(m.decision)||m.terminal>Terminal::Disconnected)return false;
        const bool empty=m.channels==ChannelGeneration{}&&Empty(m.seed)&&Empty(m.witness)&&!m.appliedRevision&&!m.nativeTick;
        switch(m.kind)
        {
        case Kind::Prepare:return empty&&!m.retirementMask&&m.terminal==Terminal::None;
        case Kind::Retired:return empty&&m.retirementMask==CompleteRetirementMask&&m.terminal==Terminal::None;
        case Kind::Load:return ValidChannels(m.channels)&&!Empty(m.seed)&&!Empty(m.witness)&&
            !m.appliedRevision&&!m.nativeTick&&!m.retirementMask&&m.terminal==Terminal::None;
        case Kind::Loaded:case Kind::Resume:return ValidChannels(m.channels)&&!Empty(m.seed)&&!Empty(m.witness)&&
            m.appliedRevision&&m.nativeTick&&!m.retirementMask&&m.terminal==Terminal::None;
        case Kind::Abort:return empty&&!m.retirementMask&&m.terminal!=Terminal::None;
        default:return false;
        }
    }
    std::optional<std::vector<std::uint8_t>> Encode(const Message& value)
    {
        if(!ValidMessage(value))return std::nullopt;
        spMemoryStream stream;
        if(!stream.ResizeAndSetSize(WireBytes)||!stream.Reset())return std::nullopt;
        Message copy=value;std::uint32_t version=ProtocolVersion;
        if(!Fields(stream,copy,version,[](auto& s,auto& v){return s.Write(v);},
            [](auto& s,auto* p,std::uint32_t bytes){return s.WriteData(p,bytes);}))return std::nullopt;
        std::uint32_t bytes=0;
        if(!stream.GetSize(&bytes)||bytes!=WireBytes)return std::nullopt;
        const auto* data=static_cast<const std::uint8_t*>(stream.GetBuffer());
        return std::vector<std::uint8_t>(data,data+bytes);
    }
    std::optional<Message> Decode(const networking::SessionMessage& value)
    {
        if(value.classId!=ApplicationClass||value.payload.size()!=WireBytes)return std::nullopt;
        spMemoryStream stream;if(!stream.ResizeAndSetSize(WireBytes))return std::nullopt;
        std::memcpy(stream.GetBuffer(),value.payload.data(),WireBytes);
        Message result;std::uint32_t version=0;
        if(!Fields(stream,result,version,[](auto& s,auto& v){return s.Read(v);},
            [](auto& s,auto* p,std::uint32_t bytes){return s.ReadData(p,bytes);})||version!=ProtocolVersion||
            value.key!=static_cast<std::uint32_t>(result.kind)||!ValidMessage(result))return std::nullopt;
        return result;
    }
}
