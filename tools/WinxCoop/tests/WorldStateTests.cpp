#include "../src/WorldState/WorldStateSession.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace winxcoop::world_state;
namespace
{
    void Check(bool condition,const char* why) { if(!condition) throw std::runtime_error(why); }
    ActorRecord Actor(std::uint32_t id)
    {
        ActorRecord a;a.stableId=id;a.typeId=0x456789AB;a.stateId=3;a.animation=static_cast<unsigned>(Animation::Walk);
        a.flags=Present|Visible|HasHealth|HasVelocity;a.position={3,4,5};a.scale={0.2f,0.3f,0.4f};a.velocity={1,0,2};a.hp=4;a.maximumHp=6;return a;
    }
    Snapshot State(std::uint64_t revision)
    {
        Snapshot s;s.revision=revision;s.nativeTick=100+revision;s.bodies={Actor(1),Actor(2)};
        Block b;b.data.resize(SharedProfileBytes);s.blocks.push_back(b);return s;
    }
    std::uint32_t Raw32(const std::vector<std::uint8_t>& bytes,std::size_t at)
    {
        Check(at+4<=bytes.size(),"independent scalar observation is bounded");
        return std::uint32_t(bytes[at])|(std::uint32_t(bytes[at+1])<<8)|
            (std::uint32_t(bytes[at+2])<<16)|(std::uint32_t(bytes[at+3])<<24);
    }
    void Put32(std::vector<std::uint8_t>& bytes,std::size_t at,std::uint32_t value)
    {
        Check(at+4<=bytes.size(),"independent corruption is bounded");
        for(unsigned i=0;i<4;++i)bytes[at+i]=static_cast<std::uint8_t>(value>>(i*8));
    }
    EntrySeed Seed(std::uint32_t epoch=1,std::uint32_t generation=1)
    {
        EntrySeed seed;seed.level=28;seed.levelEpoch=epoch;seed.generation=generation;seed.hostEntryOrdinal=7;
        seed.preProfile[48]=255;seed.preProfile[126]=1;seed.preProfile[176]=254;
        seed.preSavedSpawn6E0=1;seed.preSavedPosition584={-1.5f,2.25f,3.0f};return seed;
    }
    StarRecord Star(std::uint64_t activation,std::uint32_t slot=0)
    {
        StarRecord star;star.activationId=activation;star.category=0;star.slot=slot;
        star.stateC8=UINT32_MAX;star.position={3,4,5};star.scale={1,2,3};return star;
    }
    Snapshot ExtendedState(std::uint64_t revision,std::uint32_t epoch=1,std::uint32_t generation=1,unsigned stars=2)
    {
        auto state=State(revision);state.entities={Actor(100)};
        auto seed=EncodeEntrySeed(Seed(epoch,generation));Check(seed.has_value(),"entry fixture encodes");
        std::vector<StarRecord> values;for(unsigned i=0;i<stars;++i)values.push_back(Star(1000+i,i));
        auto presentation=EncodeStarPresentation(values);Check(presentation.has_value(),"star fixture encodes");
        state.blocks.push_back({BlockType::EntryWitnessV1,std::vector<std::uint8_t>(EntryWitnessBytes)});
        state.blocks.push_back({BlockType::EntrySeedV1,std::move(*seed)});
        state.blocks.push_back({BlockType::StarPresentationV1,std::move(*presentation)});return state;
    }
    void SameBlocks(const Snapshot& expected,const Snapshot& actual)
    {
        Check(expected.blocks.size()==actual.blocks.size(),"all snapshot blocks retained");
        for(std::size_t i=0;i<expected.blocks.size();++i)
            Check(expected.blocks[i].type==actual.blocks[i].type&&expected.blocks[i].data==actual.blocks[i].data,
                "full typed block content survives its carrier unchanged");
    }
    Message CarrierRoundTrip(const Message& m)
    {
        const auto bytes=Encode(m);Check(bytes.has_value(),"valid world envelope encodes");
        Check(bytes->size()<=256,"shared original Packet capacity never enlarged");
        Check((*bytes)[0]==ProtocolVersion&&ProtocolVersion==1,"schema2 never changes the WCS1 envelope version");
        auto result=Decode({ApplicationClass,static_cast<unsigned>(m.kind),*bytes});Check(result.has_value(),"common stream roundtrip");return *result;
    }
    bool Transfer(Session& source,Session& target)
    {
        const auto next=source.PeekOutbound();if(!next) return false;
        auto m=CarrierRoundTrip(*next);Check(source.CommitOutbound(),"commit only after accepted carrier");Check(target.Receive(m),"peer admits valid ordered frame");return true;
    }
    void Flush(Session& host,Session& guest)
    { for(unsigned i=0;i<128;++i) {const bool a=Transfer(host,guest),b=Transfer(guest,host);if(!a&&!b)return;}throw std::runtime_error("bounded flush did not settle"); }
    struct Pair final
    {
        SessionIdentity id{11,22,33,1,1};Session host{Role::Host,id},guest{Role::Guest,id};
        Pair() {Check(host.ObserveActor(Availability::Ready,1),"host world ready");Check(guest.ObserveActor(Availability::Ready,1),"guest world ready");Flush(host,guest);}
    };
    void ScalarRoundTripAndValidation()
    {
        auto s=State(1);s.flags=NativePaused;s.entities={Actor(100)};
        auto& p=s.blocks[0].data;p[48]=255;p[176]=255;p[177]=255;std::memset(p.data()+178,255,8);
        const auto expectedProfile=p;
        s.blocks.push_back({BlockType::EntryWitnessV1,std::vector<std::uint8_t>(64)});
        auto encoded=EncodeSnapshot(s);Check(encoded&&encoded->size()==40+3*100+8+411+8+64,"exact variable snapshot scalar byte count");
        Check((*encoded)[0]==SnapshotVersion&&SnapshotVersion==2&&(*encoded)[4]==1&&(*encoded)[8]==1,"independent scalar version/flags/revision prefix");
        auto read=DecodeSnapshot(encoded->data(),encoded->size());Check(read&&read->bodies[1].scale[2]==0.4f&&read->entities[0].hp==4&&read->blocks[0].data==expectedProfile,"scale and typed block preserved");
        auto bad=*encoded;bad.push_back(0);Check(!DecodeSnapshot(bad.data(),bad.size()),"trailing bytes rejected");
        Check(!DecodeSnapshot(encoded->data(),encoded->size()-1),"truncated typed block rejected");
        s=State(1);s.blocks[0].data[126]=2;Check(!EncodeSnapshot(s),"profile boolean not arbitrary byte");
        s=State(1);s.blocks[0].data[410]=2;Check(!EncodeSnapshot(s),"manager facet bool enforced");
        s=State(1);s.blocks.push_back(s.blocks[0]);Check(!EncodeSnapshot(s),"duplicate profile owner block rejected");
        s=State(1);s.blocks[0].type=static_cast<BlockType>(99);Check(!EncodeSnapshot(s),"unknown schema block rejected");
        s=State(1);s.bodies[1].stableId=1;Check(!EncodeSnapshot(s),"stable IDs unique");
        s=State(1);s.bodies[0].scale[1]=0;Check(!EncodeSnapshot(s),"singular scale rejected");
        s=State(1);s.bodies[0].position[0]=std::numeric_limits<float>::quiet_NaN();Check(!EncodeSnapshot(s),"nonfinite pose rejected");
        s=State(1);s.bodies[0].hp=7;Check(!EncodeSnapshot(s),"invalid health range rejected");
        s=State(1);for(unsigned i=0;i<65;++i)s.entities.push_back(Actor(100+i));Check(!EncodeSnapshot(s),"entity capacity enforced before serialization");
    }
    void EntrySeedValidation()
    {
        const auto source=Seed();const auto bytes=EncodeEntrySeed(source);
        Check(bytes&&bytes->size()==448,"EntrySeed ABI exactly448 bytes");
        Check(Raw32(*bytes,0)==1&&Raw32(*bytes,4)==28&&Raw32(*bytes,8)==1&&Raw32(*bytes,12)==1&&
            Raw32(*bytes,16)==7&&Raw32(*bytes,20)==0,"entry six DWORD scalar prefix");
        Check((*bytes)[24+48]==255&&(*bytes)[24+126]==1&&(*bytes)[24+176]==254&&(*bytes)[435]==1,
            "preProfile411 and saved-spawn selector are separate fields");
        Check(Raw32(*bytes,436)==0xBFC00000u&&Raw32(*bytes,440)==0x40100000u&&Raw32(*bytes,444)==0x40400000u,
            "saved position has independent IEEE754 little-endian offsets");
        const auto decoded=DecodeEntrySeed(bytes->data(),bytes->size());
        Check(decoded&&decoded->preProfile==source.preProfile&&decoded->preSavedSpawn6E0==1&&
            decoded->preSavedPosition584==source.preSavedPosition584,"entry seed scalar roundtrip");
        Check(!DecodeEntrySeed(nullptr,448)&&!DecodeEntrySeed(bytes->data(),447),"entry null/truncation rejected");
        for(const auto at:std::vector<std::size_t>{0,4,8,12,16})
        {auto bad=*bytes;Put32(bad,at,0);Check(!DecodeEntrySeed(bad.data(),bad.size()),"entry zero version/identity fields fail closed");}
        auto bad=*bytes;Put32(bad,4,51);Check(!DecodeEntrySeed(bad.data(),bad.size()),"entry level outside1..50 rejected");
        bad=*bytes;Put32(bad,20,1);Check(!DecodeEntrySeed(bad.data(),bad.size()),"entry reserved DWORD rejected");
        bad=*bytes;bad[435]=2;Check(!DecodeEntrySeed(bad.data(),bad.size()),"saved spawn is native bool0/1");
        bad=*bytes;bad[24+126]=2;Check(!DecodeEntrySeed(bad.data(),bad.size()),"seed reuses Profile411 bool validation");
        bad=*bytes;Put32(bad,436,0x7FC00000);Check(!DecodeEntrySeed(bad.data(),bad.size()),"nonfinite saved position rejected");
        auto invalid=source;invalid.preSavedPosition584[0]=10000001.0f;Check(!EncodeEntrySeed(invalid),"saved position magnitude bounded");
        invalid=source;invalid.reserved=1;Check(!EncodeEntrySeed(invalid),"entry encoder applies same reserved guard");
        invalid=source;invalid.level=50;Check(EncodeEntrySeed(invalid).has_value(),"generic level upper bound is explicit50");
    }
    void StarPresentationValidation()
    {
        auto source=Star(0x0102030405060708ull,49);source.category=2;
        const auto bytes=EncodeStarPresentation({source});Check(bytes&&bytes->size()==104,"one Star is header16 plus record88");
        Check(Raw32(*bytes,0)==1&&Raw32(*bytes,4)==1&&Raw32(*bytes,8)==88&&Raw32(*bytes,12)==0,"Star header scalar oracle");
        Check((*bytes)[16]==8&&(*bytes)[23]==1&&Raw32(*bytes,24)==2&&Raw32(*bytes,28)==49&&
            Raw32(*bytes,32)==UINT32_MAX&&Raw32(*bytes,36)==17&&Raw32(*bytes,100)==0,
            "activation/category/slot/rawState/flags/reserved offsets are independent");
        const auto decoded=DecodeStarPresentation(bytes->data(),bytes->size());
        Check(decoded&&decoded->size()==1&&decoded->front().activationId==source.activationId&&
            decoded->front().stateC8==UINT32_MAX&&decoded->front().scale==source.scale,"Star scalar roundtrip preserves opaque stateC8");
        const auto empty=EncodeStarPresentation({});Check(empty&&empty->size()==16,"empty absolute Star state is a real block");
        const auto decodedEmpty=DecodeStarPresentation(empty->data(),empty->size());
        Check(decodedEmpty&&decodedEmpty->empty(),"empty block roundtrip preserves absolute absence without fake records");
        std::vector<StarRecord> full;for(unsigned i=0;i<MaximumStars;++i)full.push_back(Star(i+1,i));
        const auto maximum=EncodeStarPresentation(full);Check(maximum&&maximum->size()==1424,"maximum16 Star payload exactly1424");
        full.push_back(Star(17,16));Check(!EncodeStarPresentation(full),"seventeenth Star rejected before publication");
        auto bad=*bytes;Put32(bad,4,UINT32_MAX);Check(!DecodeStarPresentation(bad.data(),bad.size()),"unbounded Star count rejected before allocation");
        bad=*bytes;Put32(bad,8,84);Check(!DecodeStarPresentation(bad.data(),bad.size()),"unknown Star record size rejected");
        bad=*bytes;Put32(bad,12,1);Check(!DecodeStarPresentation(bad.data(),bad.size()),"nonzero Star header reserved rejected");
        bad=*bytes;Put32(bad,100,1);Check(!DecodeStarPresentation(bad.data(),bad.size()),"nonzero Star record reserved rejected");
        bad=*bytes;bad.push_back(0);Check(!DecodeStarPresentation(bad.data(),bad.size()),"Star block trailing bytes rejected");
        Check(!DecodeStarPresentation(bytes->data(),bytes->size()-1)&&!DecodeStarPresentation(nullptr,104),"Star truncated/null blocks rejected");
        auto invalid=source;invalid.activationId=0;Check(!EncodeStarPresentation({invalid}),"Star activation identity is nonzero");
        invalid=source;invalid.category=4;Check(!EncodeStarPresentation({invalid}),"unknown pool category rejected");
        invalid=source;invalid.slot=50;Check(!EncodeStarPresentation({invalid}),"ordinary pool slot50 rejected");
        invalid=source;invalid.category=3;invalid.slot=99;Check(EncodeStarPresentation({invalid}).has_value(),"optional pool permits slot99");
        invalid.slot=100;Check(!EncodeStarPresentation({invalid}),"optional pool slot100 rejected");
        invalid=source;invalid.flags=Visible;Check(!EncodeStarPresentation({invalid}),"included Star must be Present; absence is list omission");
        invalid=source;invalid.flags=Present|HasHealth;Check(!EncodeStarPresentation({invalid}),"Actor health flags are invalid for Star");
        invalid=source;invalid.flags=Present;Check(EncodeStarPresentation({invalid}).has_value(),"active hidden Star is representable");
        invalid=source;invalid.scale[0]=0;Check(EncodeStarPresentation({invalid}).has_value(),"Star presentation may have zero collect/fade scale");
        invalid=source;invalid.scale[0]=0.00000001f;Check(EncodeStarPresentation({invalid}).has_value(),"Star does not inherit the Actor invertibility epsilon");
        invalid=source;invalid.scale[0]=-1;Check(EncodeStarPresentation({invalid}).has_value(),"wire scale range follows existing signed Actor pose domain");
        invalid=source;invalid.scale[0]=10001;Check(!EncodeStarPresentation({invalid}),"Star scale magnitude remains bounded");
        invalid=source;invalid.scale[0]=std::numeric_limits<float>::quiet_NaN();Check(!EncodeStarPresentation({invalid}),"Star zero-scale allowance never accepts NaN");
        invalid=source;invalid.position[1]=std::numeric_limits<float>::infinity();Check(!EncodeStarPresentation({invalid}),"nonfinite Star position rejected");
        invalid=source;invalid.rotation[2]=2.1f;Check(!EncodeStarPresentation({invalid}),"Star rotation bounds match Actor pose domain");
        auto duplicate=source;duplicate.slot=48;Check(!EncodeStarPresentation({source,duplicate}),"activation IDs cannot repeat across slots");
        duplicate=source;duplicate.activationId=9;Check(!EncodeStarPresentation({source,duplicate}),"one native pool slot cannot map to two activations");
        duplicate.category=1;Check(EncodeStarPresentation({source,duplicate}).has_value(),"same slot in a different category is distinct");
    }
    void ExtendedSnapshotValidation()
    {
        auto source=ExtendedState(1);auto bytes=EncodeSnapshot(source);Check(bytes.has_value(),"four known blocks encode together");
        Check(Raw32(*bytes,0)==2&&Raw32(*bytes,32)==4,"schema2 explicitly carries four blocks");
        auto decoded=DecodeSnapshot(bytes->data(),bytes->size());Check(decoded.has_value(),"schema2 extended state decodes");SameBlocks(source,*decoded);
        auto bad=*bytes;Put32(bad,0,1);Check(!DecodeSnapshot(bad.data(),bad.size()),"schema1 payload never accepted under schema2");
        bad=*bytes;Put32(bad,32,5);Check(!DecodeSnapshot(bad.data(),bad.size()),"fifth block rejected before allocation");
        auto duplicate=State(1);duplicate.blocks.push_back(source.blocks.back());duplicate.blocks.push_back(source.blocks.back());
        Check(!EncodeSnapshot(duplicate),"duplicate Star blocks rejected below total block cap");
        source=ExtendedState(1);std::swap(source.blocks[0],source.blocks[2]);Check(!EncodeSnapshot(source),"SharedProfile must remain first required owner");
        source=ExtendedState(1);source.blocks[2].data[435]=2;Check(!EncodeSnapshot(source),"nested malformed seed rejected");
        source=ExtendedState(1);Put32(source.blocks[3].data,36,Visible);Check(!EncodeSnapshot(source),"nested malformed Star rejected");
        source=ExtendedState(1,1,1,16);source.entities.clear();for(unsigned i=0;i<MaximumEntities;++i)source.entities.push_back(Actor(100+i));
        bytes=EncodeSnapshot(source);Check(bytes&&bytes->size()==9019&&bytes->size()<=MaximumSnapshotBytes,
            "largest known bounded schema2 shape fits unchanged16KiB cap");
    }
    void FragmentBoundaries()
    {
        const auto expected=ExtendedState(1,1,1,16);const auto bytes=EncodeSnapshot(expected);
        Check(bytes.has_value(),"extended fragmented snapshot");Assembly a;Message m;
        m.kind=Kind::Snapshot;m.sessionId=11;m.scenarioId=22;m.connectionToken=33;m.levelEpoch=m.generation=m.syncEpoch=1;m.revision=1;m.totalBytes=static_cast<unsigned>(bytes->size());
        for(std::size_t offset=0;offset<bytes->size();offset+=MaximumChunkBytes)
        {
            m.offset=static_cast<unsigned>(offset);const auto count=(std::min)(std::size_t(MaximumChunkBytes),bytes->size()-offset);
            m.chunk.assign(bytes->begin()+offset,bytes->begin()+offset+count);Check(a.Push(CarrierRoundTrip(m)),"contiguous bounded assembly");
            if(offset+count<bytes->size())Check(!a.Complete(),"partial assembly never native-visible");
        }
        const auto completed=a.Complete();Check(completed.has_value(),"complete typed snapshot accepted");SameBlocks(expected,*completed);
        Check(!a.Push(m),"duplicate fragment rejected");
        a.Clear();m.offset=184;Check(!a.Push(m),"out-of-order first fragment rejected");
        m.offset=0;m.chunk.assign(bytes->begin(),bytes->begin()+184);Check(a.Push(m),"first accepted");m.revision=2;m.offset=184;Check(!a.Push(m),"interleaved revision rejected");
        auto wire=Encode(CarrierRoundTrip(m));Check(wire.has_value(),"valid changed revision envelope");
        winxcoop::networking::SessionMessage carrier{ApplicationClass,static_cast<unsigned>(m.kind),*wire};
        carrier.classId=0x35494357;Check(!Decode(carrier),"Input5 cannot be interpreted asWorldState1");
    }
    bool FeedSnapshot(Session& guest,const std::vector<std::uint8_t>& bytes,const SessionIdentity& identity,std::uint32_t syncEpoch=1)
    {
        Message message;message.kind=Kind::Snapshot;message.sessionId=identity.sessionId;message.scenarioId=identity.scenarioId;
        message.connectionToken=identity.hostToken;message.levelEpoch=identity.levelEpoch;message.generation=identity.generation;
        message.syncEpoch=syncEpoch;message.revision=1;message.totalBytes=static_cast<std::uint32_t>(bytes.size());
        for(std::size_t at=0;at<bytes.size();at+=MaximumChunkBytes)
        {
            message.offset=static_cast<std::uint32_t>(at);
            const auto count=(std::min)(std::size_t(MaximumChunkBytes),bytes.size()-at);
            message.chunk.assign(bytes.begin()+at,bytes.begin()+at+count);
            if(!guest.Receive(CarrierRoundTrip(message))) return false;
        }
        return true;
    }
    void ExtendedSessionAndSeedIdentity()
    {
        Pair pair;const auto first=ExtendedState(1),third=ExtendedState(3);
        Check(pair.host.OfferSnapshot(first),"extended initial full state offered");Flush(pair.host,pair.guest);
        const auto delivery=pair.guest.TakeReceivedSnapshot();Check(delivery&&delivery->revision==1,"extended full delivery waits for native owner");
        SameBlocks(first,*delivery);
        Check(pair.host.AppliedRevision()==0&&pair.guest.AppliedSnapshotCount()==0,"extended blocks do not create an implicit apply ACK");
        Check(pair.host.OfferSnapshot(ExtendedState(2))&&pair.host.OfferSnapshot(third),"unpublished extended snapshots coalesce absolutely");
        Check(pair.guest.ObserveApplied(1,1)&&pair.guest.ObserveApplied(1,1),"repeated exact pending native ACK remains one control");
        Flush(pair.host,pair.guest);
        Check(pair.host.AppliedSnapshotCount()==1&&pair.guest.AppliedSnapshotCount()==1,"repeated ACK counts only once on both owners");
        const auto next=pair.guest.TakeReceivedSnapshot();Check(next&&next->revision==3,"latest extended full state replaces only unpublished revision2");
        SameBlocks(third,*next);
        Check(pair.guest.ObserveApplied(1,3),"second complete extended state explicitly applied");Flush(pair.host,pair.guest);
        Check(pair.host.OfferSnapshot(third)&&!pair.host.PeekOutbound(),"same revision identical extended payload is a no-op");
        Check(pair.guest.ObserveApplied(1,3)&&!pair.guest.PeekOutbound()&&pair.guest.AppliedSnapshotCount()==2,
            "cached repeated ACK emits no additional ACK; native effects remain consumer-owned");
        auto changed=third;changed.blocks[0].data[48]=1;
        Check(!pair.host.OfferSnapshot(changed)&&pair.host.Failure()==Terminal::Sequence,"same revision changed bytes cannot replace accepted state");
        for(const auto field:std::vector<std::size_t>{8,12})
        {
            auto wrong=ExtendedState(1);Put32(wrong.blocks[2].data,field,2);
            Pair hostReject;
            Check(!hostReject.host.OfferSnapshot(wrong)&&hostReject.host.Failure()==Terminal::Identity&&!hostReject.host.PeekOutbound(),
                "Host seed epoch/generation must match fixed session identity before forwarding");
            const auto bytes=EncodeSnapshot(wrong);Check(bytes.has_value(),"wrong identity is structurally well-formed seed");
            Pair guestReject;
            Check(!FeedSnapshot(guestReject.guest,*bytes,guestReject.id)&&guestReject.guest.Failure()==Terminal::Identity&&
                !guestReject.guest.TakeReceivedSnapshot()&&!guestReject.guest.PeekOutbound()&&guestReject.guest.AppliedSnapshotCount()==0,
                "Guest rejects seed identity mismatch before owner admission or native ACK");
        }
        Pair legacy;auto bytes=EncodeSnapshot(ExtendedState(1));Check(bytes.has_value(),"legacy corruption fixture encodes first");
        Put32(*bytes,0,1);
        Check(!FeedSnapshot(legacy.guest,*bytes,legacy.id)&&legacy.guest.Failure()==Terminal::InvalidRecord&&!legacy.guest.TakeReceivedSnapshot(),
            "a schema1 payload inside valid envelope1 never reaches the schema2 owner");
    }
    void FreshSessionFullResynchronization()
    {
        Pair old;const auto oldState=ExtendedState(1);
        Check(old.host.OfferSnapshot(oldState),"old session offers full state");Flush(old.host,old.guest);
        Check(old.guest.TakeReceivedSnapshot().has_value()&&old.guest.ObserveApplied(1,1),"old state explicitly applied");Flush(old.host,old.guest);
        old.host.Stop(Terminal::PeerClosed);old.guest.Stop(Terminal::PeerClosed);
        Check(!old.host.OfferSnapshot(ExtendedState(2))&&!old.guest.ObserveActor(Availability::Ready,2),"terminal old session cannot be revived for reconnect");
        const SessionIdentity identity{101,202,303,2,2};Session host(Role::Host,identity),guest(Role::Guest,identity);
        Check(host.ObserveActor(Availability::Ready,1)&&guest.ObserveActor(Availability::Ready,1),"fresh owners declare readiness");Flush(host,guest);
        const auto full=ExtendedState(1,2,2);
        Check(host.OfferSnapshot(full),"fresh identity accepts a complete current entry/profile/Star state");Flush(host,guest);
        const auto received=guest.TakeReceivedSnapshot();Check(received&&host.AppliedRevision()==0,"fresh connection requires initial native apply again");
        SameBlocks(full,*received);
        Check(guest.ObserveApplied(1,1),"sync epoch1 is separate from native level epoch2");Flush(host,guest);
        Check(host.AppliedSnapshotCount()==1&&guest.AppliedSnapshotCount()==1,"fresh session acceptance counts one explicit full apply");
        Session wrongHost(Role::Host,identity),wrongGuest(Role::Guest,identity);
        Check(wrongHost.ObserveActor(Availability::Ready,1)&&wrongGuest.ObserveActor(Availability::Ready,1),"separate fresh replay test owners");Flush(wrongHost,wrongGuest);
        const auto oldBytes=EncodeSnapshot(oldState);Check(oldBytes.has_value(),"old full state bytes preserved for replay test");
        Check(!FeedSnapshot(wrongGuest,*oldBytes,identity)&&wrongGuest.Failure()==Terminal::Identity&&!wrongGuest.TakeReceivedSnapshot(),
            "old native entry generation cannot enter a fresh identity even under a current outer envelope");
    }
    void NativeAckAndAbsoluteCoalescing()
    {
        { Pair invalid;Check(!invalid.guest.ObserveApplied(1,0)&&invalid.guest.Failure()==Terminal::Sequence,
            "zero revision with nonzero native epoch fails closed"); }
        Pair p;Check(p.host.State()==Phase::Connected&&p.guest.State()==Phase::Connected,"network ready is not world applied ready");
        Check(p.host.OfferSnapshot(State(1)),"host absolute capture1");
        const auto first=p.host.PeekOutbound(),retry=p.host.PeekOutbound();Check(first&&retry&&first->chunk==retry->chunk&&first->offset==retry->offset,"backpressure retries exact retained fragment");
        Flush(p.host,p.guest);auto received=p.guest.TakeReceivedSnapshot();Check(received&&received->revision==1,"first complete revision delivered");
        Check(p.host.SentRevision()==1&&!p.host.AppliedRevision()&&p.host.State()!=Phase::Ready,"no automatic ACK on receipt");
        Check(p.host.OfferSnapshot(State(2))&&p.host.OfferSnapshot(State(3)),"unpublished snapshots coalesce");
        Check(!p.host.PeekOutbound()&&p.host.CoalescedUnpublished()==1,"one unacknowledged revision blocksnewflight");
        Check(p.guest.ObserveApplied(1,1),"native apply exactrevision");Flush(p.host,p.guest);
        Check(p.host.AppliedRevision()==1&&p.host.State()==Phase::Ready,"native ACK qualifies initialready");
        auto next=p.guest.TakeReceivedSnapshot();Check(next&&next->revision==3,"absolute replacement skips only unpublished2");
        Check(p.guest.ObserveApplied(1,3),"native applies3");Flush(p.host,p.guest);Check(p.host.AppliedRevision()==3,"ACK3 exact");
        Check(p.host.ObserveActor(Availability::Waiting,2),"actualworldwaiting");Flush(p.host,p.guest);
        Check(p.guest.ObserveApplied(1,3),"cached priorACK afterpause harmless");
        Check(p.host.ObserveActor(Availability::Ready,3),"world rearm");Flush(p.host,p.guest);
        Check(p.host.Epoch()==2&&p.guest.Epoch()==2&&p.host.State()!=Phase::Ready,"new epoch requiresfullapplyagain");
        auto full=p.guest.TakeReceivedSnapshot();Check(full&&full->revision==3,"latest absolute full resend afterworldrearm");
        Check(p.guest.ObserveApplied(1,3),"old epochACK ignored");Check(!p.host.AppliedRevision(),"old epochdoesnotqualify");
    }
    void FailClosedAndWaitingHello()
    {
        SessionIdentity id{11,22,33,1,1};Session host(Role::Host,id),guest(Role::Guest,id);
        Check(host.ObserveActor(Availability::Ready,1)&&guest.ObserveActor(Availability::Waiting,1),"initial waiting");Flush(host,guest);
        Check(guest.ObserveActor(Availability::Ready,2),"sourceReady afterHello waiting");Flush(host,guest);Check(host.Epoch()==1,"laterready arms epoch");
        Pair p;Check(p.host.OfferSnapshot(State(1)),"offer");Flush(p.host,p.guest);Check(!p.guest.ObserveApplied(1,1),"ACK before owner takes delivery rejected");
        Check(p.guest.Failure()==Terminal::NativeApply,"native apply fence remains terminal");
        Pair q;auto fake=Message{};fake.kind=Kind::AppliedAck;fake.sessionId=11;fake.scenarioId=22;fake.connectionToken=33;fake.levelEpoch=fake.generation=fake.syncEpoch=1;fake.revision=55;
        Check(!q.host.Receive(fake)&&q.host.State()==Phase::Closed,"unoffered revision ACK failsclosed");
        Check(!q.host.ObserveActor(Availability::Ready,2),"closed session cannotbe reused");
    }
}
int main()
{
    try {ScalarRoundTripAndValidation();EntrySeedValidation();StarPresentationValidation();ExtendedSnapshotValidation();
        FragmentBoundaries();ExtendedSessionAndSeedIdentity();FreshSessionFullResynchronization();
        NativeAckAndAbsoluteCoalescing();FailClosedAndWaitingHello();
        std::cout<<"WorldState: schema2 entry/Star bounds, fragmentation, explicit ACK, full resync and identity fences PASS\n";return 0;}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
