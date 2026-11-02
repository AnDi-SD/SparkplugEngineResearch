#include "WorldStateProtocol.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace winxcoop::world_state
{
    namespace
    {
        using sparkplug::reconstruction::spMemoryStream;
        static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559);
        bool Finite(float v,float bound) noexcept { return std::isfinite(v) && std::abs(v)<=bound; }
        template<class Pose> bool ValidPose(const Pose& a,bool nonzeroScale=true) noexcept
        {
            for(auto v:a.position) if(!Finite(v,10000000)) return false;
            for(auto v:a.scale) if(!Finite(v,10000) || (nonzeroScale&&std::abs(v)<0.000001f)) return false;
            for(auto v:a.rotation) if(!Finite(v,2)) return false;
            return true;
        }
        bool ValidActor(const ActorRecord& a) noexcept
        {
            if(!a.stableId || !a.typeId || a.animation>static_cast<std::uint32_t>(Animation::Other) || (a.flags&~ActorFlags)) return false;
            if(!ValidPose(a)) return false;
            for(auto v:a.velocity) if(!Finite(v,1000000) || (!(a.flags&HasVelocity) && v!=0)) return false;
            if(a.flags&HasHealth)
            { if(a.maximumHp<=0 || a.hp<0 || a.hp>a.maximumHp) return false; }
            else if(a.hp!=-1 || a.maximumHp!=-1) return false;
            return true;
        }
        bool ValidSharedProfile(const std::uint8_t* data,std::size_t bytes) noexcept
        {
            if(!data || bytes!=SharedProfileBytes) return false;
            for(std::size_t i=126;i<176;++i) if(data[i]>1) return false;
            for(std::size_t i=186;i<411;++i) if(data[i]>1) return false;
            // Permission/context bytes176/177 and counts/rank are raw native
            // scalars. Do not guess a cap or interpret all bits as booleans.
            return true;
        }
        bool ValidEntrySeedValue(const EntrySeed& value) noexcept
        {
            if(value.version!=EntrySeedVersion || value.level<1 || value.level>50 || !value.levelEpoch ||
                !value.generation || !value.hostEntryOrdinal || value.reserved || value.preSavedSpawn6E0>1 ||
                !ValidSharedProfile(value.preProfile.data(),value.preProfile.size())) return false;
            for(auto coordinate:value.preSavedPosition584) if(!Finite(coordinate,10000000)) return false;
            return true;
        }
        bool ValidStar(const StarRecord& value) noexcept
        {
            return value.activationId && value.category<4 && value.slot<(value.category==3?100u:50u) &&
                !(value.flags&~(Present|Visible)) && (value.flags&Present) && !value.reserved && ValidPose(value,false);
        }
        template<class T> T ReadPod(const std::uint8_t* bytes,std::size_t at) noexcept
        {
            // Allocation-free structural validation of this explicit Windows
            // little-endian ABI. Encoding/decoding still uses MemoryStream.
            static_assert(std::is_trivially_copyable_v<T>);
            T value{};std::memcpy(&value,bytes+at,sizeof(value));return value;
        }
        bool ValidEntrySeedPayload(const std::uint8_t* data,std::size_t bytes) noexcept
        { return data && bytes==EntrySeedBytes && ValidEntrySeedValue(ReadPod<EntrySeed>(data,0)); }
        bool ValidStarPayload(const std::uint8_t* data,std::size_t bytes) noexcept
        {
            if(!data || bytes<StarPresentationHeaderBytes ||
                ReadPod<std::uint32_t>(data,0)!=StarPresentationVersion ||
                ReadPod<std::uint32_t>(data,8)!=StarRecordBytes || ReadPod<std::uint32_t>(data,12)) return false;
            const auto count=ReadPod<std::uint32_t>(data,4);
            if(count>MaximumStars || bytes!=StarPresentationHeaderBytes+std::size_t(count)*StarRecordBytes) return false;
            for(std::uint32_t i=0;i<count;++i)
            {
                const auto value=ReadPod<StarRecord>(data,StarPresentationHeaderBytes+std::size_t(i)*StarRecordBytes);
                if(!ValidStar(value)) return false;
                for(std::uint32_t k=0;k<i;++k)
                {
                    const auto prior=ReadPod<StarRecord>(data,StarPresentationHeaderBytes+std::size_t(k)*StarRecordBytes);
                    if(prior.activationId==value.activationId || (prior.category==value.category && prior.slot==value.slot)) return false;
                }
            }
            return true;
        }
        bool ValidBlockSize(BlockType type,std::size_t bytes) noexcept
        {
            switch(type)
            {
            case BlockType::SharedProfileV1:return bytes==SharedProfileBytes;
            case BlockType::EntryWitnessV1:return bytes==EntryWitnessBytes;
            case BlockType::EntrySeedV1:return bytes==EntrySeedBytes;
            case BlockType::StarPresentationV1:
                return bytes>=StarPresentationHeaderBytes &&
                    bytes<=StarPresentationHeaderBytes+MaximumStars*StarRecordBytes &&
                    (bytes-StarPresentationHeaderBytes)%StarRecordBytes==0;
            default:return false;
            }
        }
        bool ValidBlock(const Block& b) noexcept
        {
            if(!ValidBlockSize(b.type,b.data.size())) return false;
            switch(b.type)
            {
            case BlockType::SharedProfileV1:return ValidSharedProfile(b.data.data(),b.data.size());
            case BlockType::EntryWitnessV1:return true; // Native owner validates the semantic witness.
            case BlockType::EntrySeedV1:return ValidEntrySeedPayload(b.data.data(),b.data.size());
            case BlockType::StarPresentationV1:return ValidStarPayload(b.data.data(),b.data.size());
            default:return false;
            }
        }
        template<class Scalar,class Data> bool EntrySeedFields(spMemoryStream& stream,EntrySeed& value,Scalar scalar,Data data)
        {
            if(!scalar(stream,value.version)||!scalar(stream,value.level)||!scalar(stream,value.levelEpoch)||
                !scalar(stream,value.generation)||!scalar(stream,value.hostEntryOrdinal)||!scalar(stream,value.reserved)||
                !data(stream,value.preProfile.data(),SharedProfileBytes)||!scalar(stream,value.preSavedSpawn6E0)) return false;
            for(auto& coordinate:value.preSavedPosition584) if(!scalar(stream,coordinate)) return false;
            return true;
        }
        template<class Operation> bool StarFields(spMemoryStream& stream,StarRecord& value,Operation op)
        {
            if(!op(stream,value.activationId)||!op(stream,value.category)||!op(stream,value.slot)||
                !op(stream,value.stateC8)||!op(stream,value.flags)) return false;
            for(auto& coordinate:value.position) if(!op(stream,coordinate)) return false;
            for(auto& coordinate:value.scale) if(!op(stream,coordinate)) return false;
            for(auto& coordinate:value.rotation) if(!op(stream,coordinate)) return false;
            return op(stream,value.reserved);
        }
        template<class Operation> bool ActorFields(spMemoryStream& s,ActorRecord& a,Operation op)
        {
            if(!op(s,a.stableId)||!op(s,a.typeId)||!op(s,a.stateId)||!op(s,a.animation)||!op(s,a.flags)) return false;
            for(auto& v:a.position) if(!op(s,v)) return false;
            for(auto& v:a.scale) if(!op(s,v)) return false;
            for(auto& v:a.rotation) if(!op(s,v)) return false;
            for(auto& v:a.velocity) if(!op(s,v)) return false;
            return op(s,a.hp)&&op(s,a.maximumHp);
        }
        template<class Operation> bool Envelope(spMemoryStream& s,Message& m,std::uint32_t& version,std::uint32_t& count,Operation op)
        {
            auto kind=static_cast<std::uint32_t>(m.kind);
            const bool ok=op(s,version)&&op(s,kind)&&op(s,m.sessionId)&&op(s,m.scenarioId)&&op(s,m.connectionToken)&&
                op(s,m.levelEpoch)&&op(s,m.generation)&&op(s,m.syncEpoch)&&op(s,m.revision)&&op(s,m.flags)&&
                op(s,m.totalBytes)&&op(s,m.offset)&&op(s,count)&&op(s,m.terminalReason);
            m.kind=static_cast<Kind>(kind); return ok;
        }
        std::size_t SnapshotSize(const Snapshot& s) noexcept
        {
            std::size_t size=SnapshotHeaderBytes+(s.bodies.size()+s.entities.size())*ActorRecordBytes;
            for(const auto& b:s.blocks) size+=8+b.data.size();
            return size;
        }
        std::vector<std::uint8_t> Bytes(spMemoryStream& s,std::uint32_t size)
        {
            const auto* data=static_cast<const std::uint8_t*>(s.GetBuffer()); return {data,data+size};
        }
    }
    std::optional<std::vector<std::uint8_t>> EncodeEntrySeed(const EntrySeed& source)
    {
        if(!ValidEntrySeedValue(source)) return std::nullopt;
        spMemoryStream stream;if(!stream.ResizeAndSetSize(EntrySeedBytes)||!stream.Reset()) return std::nullopt;
        EntrySeed value=source;
        if(!EntrySeedFields(stream,value,[](auto& s,auto& v){return s.Write(v);},
            [](auto& s,auto* bytes,std::uint32_t count){return s.WriteData(bytes,count);})) return std::nullopt;
        std::uint32_t actual=0;if(!stream.GetSize(&actual)||actual!=EntrySeedBytes) return std::nullopt;
        return Bytes(stream,actual);
    }
    std::optional<EntrySeed> DecodeEntrySeed(const void* source,std::size_t bytes)
    {
        if(!ValidEntrySeedPayload(static_cast<const std::uint8_t*>(source),bytes)) return std::nullopt;
        spMemoryStream stream;if(!stream.ResizeAndSetSize(EntrySeedBytes)) return std::nullopt;
        std::memcpy(stream.GetBuffer(),source,bytes);EntrySeed value;
        if(!EntrySeedFields(stream,value,[](auto& s,auto& v){return s.Read(v);},
            [](auto& s,auto* data,std::uint32_t count){return s.ReadData(data,count);})) return std::nullopt;
        std::uint32_t consumed=0;if(!stream.GetCurrentPosition(consumed)||consumed!=bytes) return std::nullopt;
        return value;
    }
    std::optional<std::vector<std::uint8_t>> EncodeStarPresentation(const std::vector<StarRecord>& source)
    {
        if(source.size()>MaximumStars) return std::nullopt;
        for(std::size_t i=0;i<source.size();++i)
        {
            if(!ValidStar(source[i])) return std::nullopt;
            for(std::size_t k=0;k<i;++k)
                if(source[k].activationId==source[i].activationId ||
                    (source[k].category==source[i].category&&source[k].slot==source[i].slot)) return std::nullopt;
        }
        const auto bytes=StarPresentationHeaderBytes+static_cast<std::uint32_t>(source.size())*StarRecordBytes;
        spMemoryStream stream;if(!stream.ResizeAndSetSize(bytes)||!stream.Reset()) return std::nullopt;
        const auto version=StarPresentationVersion,count=static_cast<std::uint32_t>(source.size()),recordBytes=StarRecordBytes,reserved=0u;
        if(!stream.Write(version)||!stream.Write(count)||!stream.Write(recordBytes)||!stream.Write(reserved)) return std::nullopt;
        for(auto value:source) if(!StarFields(stream,value,[](auto& s,auto& v){return s.Write(v);})) return std::nullopt;
        std::uint32_t actual=0;if(!stream.GetSize(&actual)||actual!=bytes) return std::nullopt;return Bytes(stream,actual);
    }
    std::optional<std::vector<StarRecord>> DecodeStarPresentation(const void* source,std::size_t bytes)
    {
        if(!ValidStarPayload(static_cast<const std::uint8_t*>(source),bytes)) return std::nullopt;
        spMemoryStream stream;if(!stream.ResizeAndSetSize(static_cast<std::uint32_t>(bytes))) return std::nullopt;
        std::memcpy(stream.GetBuffer(),source,bytes);
        std::uint32_t version=0,count=0,recordBytes=0,reserved=0;
        if(!stream.Read(version)||!stream.Read(count)||!stream.Read(recordBytes)||!stream.Read(reserved)) return std::nullopt;
        std::vector<StarRecord> values(count);
        for(auto& value:values) if(!StarFields(stream,value,[](auto& s,auto& v){return s.Read(v);})) return std::nullopt;
        std::uint32_t consumed=0;if(!stream.GetCurrentPosition(consumed)||consumed!=bytes) return std::nullopt;return values;
    }
    bool ValidSnapshot(const Snapshot& s) noexcept
    {
        if(!s.revision || !s.nativeTick || (s.flags&~NativePaused) || s.bodies.empty() || s.bodies.size()>MaximumBodies ||
            s.entities.size()>MaximumEntities || s.blocks.empty() || s.blocks.size()>MaximumSnapshotBlocks || s.blocks.front().type!=BlockType::SharedProfileV1 ||
            SnapshotSize(s)>MaximumSnapshotBytes) return false;
        for(std::size_t i=0;i<s.bodies.size();++i)
        {
            if(!ValidActor(s.bodies[i])) return false;
            for(std::size_t k=0;k<i;++k) if(s.bodies[i].stableId==s.bodies[k].stableId) return false;
        }
        for(std::size_t i=0;i<s.entities.size();++i)
        {
            if(!ValidActor(s.entities[i])) return false;
            for(const auto& body:s.bodies) if(body.stableId==s.entities[i].stableId) return false;
            for(std::size_t k=0;k<i;++k) if(s.entities[i].stableId==s.entities[k].stableId) return false;
        }
        for(std::size_t i=0;i<s.blocks.size();++i)
        {
            if(!ValidBlock(s.blocks[i])) return false;
            for(std::size_t k=0;k<i;++k) if(s.blocks[k].type==s.blocks[i].type) return false;
        }
        return true;
    }
    std::optional<std::vector<std::uint8_t>> EncodeSnapshot(const Snapshot& value)
    {
        if(!ValidSnapshot(value)) return std::nullopt;
        Snapshot s=value; const auto size=static_cast<std::uint32_t>(SnapshotSize(s));
        spMemoryStream stream; if(!stream.ResizeAndSetSize(size)||!stream.Reset()) return std::nullopt;
        const auto write=[](auto& st,auto& v){return st.Write(v);};
        std::uint32_t version=SnapshotVersion,reserved=0,bodies=static_cast<std::uint32_t>(s.bodies.size()),entities=static_cast<std::uint32_t>(s.entities.size()),blocks=static_cast<std::uint32_t>(s.blocks.size());
        if(!stream.Write(version)||!stream.Write(s.flags)||!stream.Write(s.revision)||!stream.Write(s.nativeTick)||
            !stream.Write(bodies)||!stream.Write(entities)||!stream.Write(blocks)||!stream.Write(reserved)) return std::nullopt;
        for(auto& a:s.bodies) if(!ActorFields(stream,a,write)) return std::nullopt;
        for(auto& a:s.entities) if(!ActorFields(stream,a,write)) return std::nullopt;
        for(const auto& b:s.blocks)
        {
            const auto type=static_cast<std::uint32_t>(b.type),count=static_cast<std::uint32_t>(b.data.size());
            if(!stream.Write(type)||!stream.Write(count)||!stream.WriteData(b.data.data(),count)) return std::nullopt;
        }
        std::uint32_t actual=0; if(!stream.GetSize(&actual)||actual!=size) return std::nullopt; return Bytes(stream,size);
    }
    std::optional<Snapshot> DecodeSnapshot(const void* data,std::size_t bytes)
    {
        if(!data || bytes<SnapshotHeaderBytes || bytes>MaximumSnapshotBytes) return std::nullopt;
        spMemoryStream stream; if(!stream.ResizeAndSetSize(static_cast<std::uint32_t>(bytes))) return std::nullopt;
        std::memcpy(stream.GetBuffer(),data,bytes);
        Snapshot s; std::uint32_t version=0,reserved=0,bodies=0,entities=0,blocks=0;
        if(!stream.Read(version)||!stream.Read(s.flags)||!stream.Read(s.revision)||!stream.Read(s.nativeTick)||
            !stream.Read(bodies)||!stream.Read(entities)||!stream.Read(blocks)||!stream.Read(reserved)||version!=SnapshotVersion||reserved||
            bodies<1||bodies>MaximumBodies||entities>MaximumEntities||blocks<1||blocks>MaximumSnapshotBlocks||
            SnapshotHeaderBytes+std::size_t(bodies+entities)*ActorRecordBytes+std::size_t(blocks)*8>bytes) return std::nullopt;
        s.bodies.resize(bodies); s.entities.resize(entities);
        const auto read=[](auto& st,auto& v){return st.Read(v);};
        for(auto& a:s.bodies) if(!ActorFields(stream,a,read)) return std::nullopt;
        for(auto& a:s.entities) if(!ActorFields(stream,a,read)) return std::nullopt;
        for(std::uint32_t i=0;i<blocks;++i)
        {
            std::uint32_t type=0,count=0,at=0;
            if(!stream.Read(type)||!stream.Read(count)||!stream.GetCurrentPosition(at)||count>bytes-at||
                !ValidBlockSize(static_cast<BlockType>(type),count)) return std::nullopt;
            Block b; b.type=static_cast<BlockType>(type); b.data.resize(count);
            if(!stream.ReadData(b.data.data(),count)) return std::nullopt; s.blocks.push_back(std::move(b));
        }
        std::uint32_t consumed=0;
        if(!stream.GetCurrentPosition(consumed)||consumed!=bytes||!ValidSnapshot(s)) return std::nullopt; return s;
    }
    bool ValidMessage(const Message& m) noexcept
    {
        if(!m.sessionId||!m.scenarioId||!m.levelEpoch||!m.generation||m.terminalReason>static_cast<std::uint32_t>(Terminal::NativeApply)) return false;
        if(m.kind==Kind::Hello)
            return !m.connectionToken&&!m.syncEpoch&&m.revision&&m.flags<=1&&!m.totalBytes&&!m.offset&&m.chunk.empty()&&!m.terminalReason;
        if(!m.connectionToken) return false;
        switch(m.kind)
        {
        case Kind::Ready: return m.syncEpoch&&m.revision&&m.flags==1&&!m.totalBytes&&!m.offset&&m.chunk.empty()&&!m.terminalReason;
        case Kind::SourceState: return m.revision&&m.flags<=1&&!m.totalBytes&&!m.offset&&m.chunk.empty()&&!m.terminalReason;
        case Kind::Snapshot:
            return m.syncEpoch&&m.revision&&!m.flags&&!m.terminalReason&&m.totalBytes>=SnapshotHeaderBytes&&m.totalBytes<=MaximumSnapshotBytes&&
                !m.chunk.empty()&&m.chunk.size()<=MaximumChunkBytes&&m.offset<m.totalBytes&&
                m.chunk.size()==(std::min)(MaximumChunkBytes,m.totalBytes-m.offset);
        case Kind::AppliedAck: return m.syncEpoch&&m.revision&&!m.flags&&!m.totalBytes&&!m.offset&&m.chunk.empty()&&!m.terminalReason;
        case Kind::Pause: return m.syncEpoch&&!m.revision&&!m.flags&&!m.totalBytes&&!m.offset&&m.chunk.empty()&&!m.terminalReason;
        case Kind::Stop: return m.terminalReason&&!m.revision&&!m.flags&&!m.totalBytes&&!m.offset&&m.chunk.empty();
        default:return false;
        }
    }
    std::optional<std::vector<std::uint8_t>> Encode(const Message& value)
    {
        if(!ValidMessage(value)) return std::nullopt;
        const auto size=WireHeaderBytes+static_cast<std::uint32_t>(value.chunk.size());
        spMemoryStream stream; if(!stream.ResizeAndSetSize(size)||!stream.Reset()) return std::nullopt;
        Message m=value; std::uint32_t version=ProtocolVersion,count=static_cast<std::uint32_t>(m.chunk.size());
        if(!Envelope(stream,m,version,count,[](auto& s,auto& v){return s.Write(v);})||
            (count&&!stream.WriteData(m.chunk.data(),count))) return std::nullopt;
        std::uint32_t actual=0; if(!stream.GetSize(&actual)||actual!=size) return std::nullopt; return Bytes(stream,size);
    }
    std::optional<Message> Decode(const networking::SessionMessage& value)
    {
        if(value.classId!=ApplicationClass||value.payload.size()<WireHeaderBytes||value.payload.size()>networking::TcpPacketFramer::MaximumPayloadSize) return std::nullopt;
        spMemoryStream stream; if(!stream.ResizeAndSetSize(static_cast<std::uint32_t>(value.payload.size()))) return std::nullopt;
        std::memcpy(stream.GetBuffer(),value.payload.data(),value.payload.size());
        Message m; std::uint32_t version=0,count=0;
        if(!Envelope(stream,m,version,count,[](auto& s,auto& v){return s.Read(v);})||version!=ProtocolVersion||
            value.key!=static_cast<std::uint32_t>(m.kind)||count>MaximumChunkBytes||value.payload.size()!=WireHeaderBytes+count) return std::nullopt;
        m.chunk.resize(count); if(count&&!stream.ReadData(m.chunk.data(),count)) return std::nullopt;
        if(!ValidMessage(m)) return std::nullopt; return m;
    }
    bool Assembly::Push(const Message& m)
    {
        if(m.kind!=Kind::Snapshot||!ValidMessage(m)) return false;
        if(data_.empty())
        { if(m.offset) return false; revision_=m.revision; total_=m.totalBytes; epoch_=m.syncEpoch; data_.reserve(total_); }
        if(m.revision!=revision_||m.syncEpoch!=epoch_||m.totalBytes!=total_||m.offset!=data_.size()||data_.size()+m.chunk.size()>total_) return false;
        data_.insert(data_.end(),m.chunk.begin(),m.chunk.end()); return true;
    }
    std::optional<Snapshot> Assembly::Complete() const
    {
        if(!revision_||data_.size()!=total_) return std::nullopt;
        auto result=DecodeSnapshot(data_.data(),data_.size());
        if(!result||result->revision!=revision_) return std::nullopt; return result;
    }
    void Assembly::Clear() noexcept { revision_=0;total_=epoch_=0;data_.clear(); }
}
