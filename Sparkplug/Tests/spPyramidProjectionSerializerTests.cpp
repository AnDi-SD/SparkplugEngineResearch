#include "Code/Sparkplug/spPyramidProjectionSerializer.h"
#include "Code/Sparkplug/spPyramidProjection.h"
#include "Code/Sparkplug/spRenderNode.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceFATSerializer.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace sparkplug::reconstruction;
namespace
{
    unsigned checks=0,comparisons=0,cases=0;
    void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
    void Match(std::uint32_t actual,std::uint32_t expected,const char* message)
    {++comparisons;if(actual!=expected)std::cerr<<message<<" actual="<<actual<<" expected="<<expected<<'\n';Check(actual==expected,message);}
    struct Reader
    {
        std::ifstream file;
        explicit Reader(const char* path):file(path,std::ios::binary){Check(bool(file),"Open Projection serializer fixture");}
        std::uint32_t U32(){std::uint32_t v=0;file.read(reinterpret_cast<char*>(&v),4);Check(bool(file),"Read fixture UInt32");return v;}
        std::vector<std::uint8_t> Blob()
        {const auto n=U32();Check(n<=65536,"Fixture blob bound");std::vector<std::uint8_t> b(n);if(n)file.read(reinterpret_cast<char*>(b.data()),n);Check(bool(file),"Read fixture blob");return b;}
    };
    void Open(spMemoryStream& stream,const std::vector<std::uint8_t>& bytes)
    {
        Check(stream.ResizeAndSetSize(static_cast<unsigned>(bytes.size())),"Allocate source input");
        if(!bytes.empty())std::memcpy(stream.GetBuffer(),bytes.data(),bytes.size());
        Check(stream.Seek(spStream::SeekSource::essStart,0),"Rewind source input");
    }
    template<class T>void Add(std::vector<std::uint8_t>& bytes,const T& value)
    {const auto* p=reinterpret_cast<const std::uint8_t*>(&value);bytes.insert(bytes.end(),p,p+sizeof(value));}
    std::vector<std::uint8_t> Directory()
    {
        std::vector<std::uint8_t> bytes;Add(bytes,2u);
        for(auto id:{7u,9u}){Add(bytes,id);Add(bytes,std::uint16_t(0));Add(bytes,spRenderNode::ClassID);Add(bytes,0u);Add(bytes,0u);}
        return bytes;
    }
    unsigned Role(const spRenderNode* p,const std::array<std::shared_ptr<spRenderNode>,2>& nodes)
    {if(!p)return 0;if(p==nodes[0].get())return 1;if(p==nodes[1].get())return 2;throw std::logic_error("Known borrowed RenderNode role");}
    void State(Reader& in,const spPyramidProjection& object,const std::array<std::shared_ptr<spRenderNode>,2>& nodes)
    {
        const auto& s=object.GetProjectionStateForAnalysis();
        for(auto v:s.flags58)Match(v,in.U32(),"Projection raw flag");
        for(auto v:s.words5C)Match(v,in.U32(),"Projection raw word");
        Match(s.word6C,in.U32(),"Projection zero-extended word");
        Match(Role(object.GetBoundRenderNodeForAnalysis(),nodes),in.U32(),"Projection typed borrowed role");
        Match(object.IsAlphaSortEnabledForAnalysis(),in.U32(),"Inherited alpha sort");
        Match(object.GetPriorityForAnalysis(),in.U32(),"Inherited priority");
        for(const auto* vector:{&object.GetCenterForAnalysis(),&object.GetSizeForAnalysis()})
        {
            std::array<std::uint32_t,3> raw{};std::memcpy(raw.data(),vector->data(),12);
            for(auto word:raw)Match(word,in.U32(),"Pyramid raw geometry word");
        }
        Match(object.GetPyramidStateForAnalysis().flip9C,in.U32(),"Pyramid raw flip byte");
        Check(object.GetBound84ForAnalysis()==object.GetBoundRenderNodeForAnalysis(),"Compatible raw accessor");
    }
    struct NodeState
    {
        spNode::Vector3 position,scale;spNode::Matrix3 orientation;std::uint32_t flags;
        explicit NodeState(const spRenderNode& node):position(node.GetWorldPositionForAnalysis()),scale(node.GetWorldScaleForAnalysis()),orientation(node.GetWorldOrientationForAnalysis()),flags(node.GetFlagsForAnalysis()){}
        bool Same(const spRenderNode& node)const
        {return position==node.GetWorldPositionForAnalysis()&&scale==node.GetWorldScaleForAnalysis()&&orientation==node.GetWorldOrientationForAnalysis()&&flags==node.GetFlagsForAnalysis()&&node.GetRenderableCountForAnalysis()==0;}
    };
    void Replay(const char* path)
    {
        Reader in(path);Check(in.U32()==0x31525359,"YSR1 fixture");const auto count=in.U32();Check(count<=64,"Fixture count bound");
        (void)spRenderNode::StaticRTTI();
        for(unsigned row=0;row<count;++row)
        {
            const auto label=in.Blob();const auto pyramid=in.U32(),initialBound=in.U32(),alias=in.U32();auto payload=in.Blob();
            Check(pyramid<2&&initialBound<=2&&alias<2,"Fixture input roles");
            try
            {
                std::array<std::shared_ptr<spRenderNode>,2> nodes{std::make_shared<spRenderNode>(),std::make_shared<spRenderNode>()};
                const std::array<NodeState,2> before{NodeState(*nodes[0]),NodeState(*nodes[1])};
                spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
                spMemoryStream index;Open(index,Directory());auto* fat=manager.GetFATForAnalysis();Check(fat->LoadIndexForAnalysis(index),"Typed RenderNode FAT inputs");
                fat->FindByIDForAnalysis(7)->object=nodes[0].get();fat->FindByIDForAnalysis(9)->object=nodes[alias?0:1].get();
                Check(pyramid==1,"Pyramid-only fixture receiver");
                auto object=std::make_unique<spPyramidProjection>(spProjectionConstructorDataForAnalysis{0});
                object->SetBoundRenderNodeForAnalysis(initialBound?nodes[initialBound-1].get():nullptr);
                const auto oldSphere=object->GetProjectionStateForAnalysis().sphere70;const auto old98=object->GetProjectionStateForAnalysis().word98;const auto ownBefore=object->GetPyramidStateForAnalysis();
                const auto refs=std::array<long,2>{nodes[0].use_count(),nodes[1].use_count()};
                State(in,*object,nodes);spMemoryStream source;Open(source,payload);std::string error;
                const bool ok=spPyramidProjectionSerializer{}.ReadPayloadForAnalysis(context,source,static_cast<unsigned>(payload.size()),*object,&error);
                Check(ok&&!context.failed,error.c_str());State(in,*object,nodes);Match(ok,in.U32(),"Whole reader return");
                std::uint32_t position=0;Check(source.GetCurrentPosition(position),"Read source position");Match(position,in.U32(),"Whole reader consumed payload");
                Match(before[0].Same(*nodes[0])&&before[1].Same(*nodes[1]),in.U32(),"Borrowed Node state preserved");
                Match(refs==std::array<long,2>{nodes[0].use_count(),nodes[1].use_count()},in.U32(),"Borrowed portable ownership roles preserved");
                Check(oldSphere==object->GetProjectionStateForAnalysis().sphere70&&old98==object->GetProjectionStateForAnalysis().word98,"Reader preserves other represented Projection state");
                Check(object->GetPyramidStateForAnalysis().wordB8==ownBefore.wordB8&&object->GetPyramidStateForAnalysis().byteCC==ownBefore.byteCC,"Reader preserves other Pyramid own fields");
                object.reset();Match(before[0].Same(*nodes[0])&&before[1].Same(*nodes[1])&&refs==std::array<long,2>{nodes[0].use_count(),nodes[1].use_count()},in.U32(),"Projection deletion leaves borrowed Nodes alive");
                ++cases;
            }
            catch(const std::exception& error){throw std::runtime_error(std::string(label.begin(),label.end())+": "+error.what());}
        }
        Check(in.file.peek()==std::char_traits<char>::eof(),"Complete YSR1 fixture");
    }
    void Basic()
    {
        (void)spPyramidProjectionSerializer::StaticRTTI();auto value=spRTTIManager::Instance().Create(spPyramidProjectionSerializer::ClassID);
        auto* typed=dynamic_cast<spPyramidProjectionSerializer*>(value.get());
        Check(typed&&typed->IsKindOf(spProjectionSerializer::ClassID)&&typed->GetTargetClassIDForAnalysis()==spPyramidProjection::ClassID,"Concrete Pyramid serializer hierarchy/target");
        spCloneManager clones;auto clone=typed->vfunc_10(clones);
        Check(dynamic_cast<spPyramidProjectionSerializer*>(clone.get())&&clones.FindClone(*typed)==clone.get(),"Concrete serializer clone and shared map");
        spPyramidProjection object(spProjectionConstructorDataForAnalysis{0});spMemoryStream output;
        std::string error;spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
        Check(output.Open("pyramid-writer-check"),"Open qualified Pyramid writer stream");
        Check(typed->WritePayloadForAnalysis(output,object,&error)&&error.empty()
            &&!typed->IndexRelationshipsForAnalysis(object),"Qualified own writer retains separate index refusal");
        struct VectorRead final : spStream
        {
            spMemoryStream data;spPyramidProjection& target;bool firstCommitted=false;
            explicit VectorRead(spPyramidProjection& object):target(object)
            {std::vector<std::uint8_t> bytes{0,0,0xA0,25};for(float v:{3.f,4.f,5.f,6.f,7.f,8.f})Add(bytes,v);bytes.push_back(0xA5);bytes.push_back(0);::Open(data,bytes);}
            bool Open(const char*)override{return false;}
            bool Open(std::uint32_t,const char*)override{return false;}
            bool Close()override{return data.Close();}
            bool Seek(SeekSource s,std::int32_t o)override{return data.Seek(s,o);}
            bool GetCurrentPosition(std::uint32_t& v)const override{return data.GetCurrentPosition(v);}
            bool ReadData(void* p,std::uint32_t n)override
            {std::uint32_t at=0;if(!data.GetCurrentPosition(at))return false;if(at==16&&n==12){firstCommitted=target.GetCenterForAnalysis()==spPyramidProjection::Vector3{3,4,5};return false;}return data.ReadData(p,n);}
            bool WriteData(const void*,std::uint32_t)override{return false;}
            bool vfunc_WriteFromStream(spStream*,std::uint32_t)override{return false;}
            bool GetSize(std::uint32_t* v)const override{return data.GetSize(v);}
        } vectorRead(object);
        Check(!typed->ReadPayloadForAnalysis(context,vectorRead,30,object,&error)&&context.failed&&vectorRead.firstCommitted
            &&object.GetSizeForAnalysis()==spPyramidProjection::Vector3{1,1,1},"Source-only second-read refusal preserves the committed center and old size");
        // Source-only reference-view witnesses. Original selected cases use
        // the real stable4331B0 getter, not an overridden native callback.
        struct Reloaded final : spPyramidProjection
        {
            struct Block{std::uint8_t flip=0;Vector3 center{},size{1,1,1};};
            std::array<Block,3> blocks{};unsigned calls=0;
            Reloaded():spPyramidProjection(spProjectionConstructorDataForAnalysis{0}){}
            SerializedFieldsForAnalysis GetSerializedFieldsForAnalysis() noexcept override
            {const auto i=calls<3?calls:2;++calls;return {blocks[i].flip,blocks[i].center,blocks[i].size};}
        } reloaded;
        std::vector<std::uint8_t> bytes{0,0,0xA0,25};for(float v:{3.f,4.f,5.f,6.f,7.f,8.f})Add(bytes,v);bytes.push_back(0xA5);bytes.push_back(0);
        spMemoryStream input;Open(input,bytes);spSerializerManager reloadManager;spResourceManager reloadResources;spSerializerReadContextForAnalysis reloadContext(reloadManager,reloadResources);
        Check(typed->ReadPayloadForAnalysis(reloadContext,input,30,reloaded,&error)&&reloaded.calls==3,"Reacquire typed parameter view after each successful12/12/1 read");
        Check(reloaded.blocks[0].center==spPyramidProjection::Vector3{3,4,5}&&reloaded.blocks[0].size==spPyramidProjection::Vector3{1,1,1}&&reloaded.blocks[0].flip==0
            &&reloaded.blocks[1].center==spPyramidProjection::Vector3{}&&reloaded.blocks[1].size==spPyramidProjection::Vector3{6,7,8}&&reloaded.blocks[1].flip==0
            &&reloaded.blocks[2].center==spPyramidProjection::Vector3{}&&reloaded.blocks[2].size==spPyramidProjection::Vector3{1,1,1}&&reloaded.blocks[2].flip==0xA5,
            "Each stage writes only its freshly resolved typed fields");
    }
}
namespace writer_profile
{
    unsigned checks=0,nativeCases=0,writes=0,reads=0,wireBytes=0,ioEvents=0,stateWords=0;
    void Check(bool v,const char* why){++checks;if(!v)throw std::runtime_error(why);}
    using State=std::array<std::uint32_t,21>;
    using Event=std::array<std::uint32_t,4>;
    struct Reader
    {
        std::ifstream file;
        explicit Reader(const char* path):file(path,std::ios::binary){Check(bool(file),"Open native writer fixture");}
        std::uint32_t U32(){std::uint32_t v=0;file.read(reinterpret_cast<char*>(&v),4);Check(bool(file),"Read writer fixture UInt32");return v;}
        std::vector<std::uint8_t> Blob(){const auto n=U32();Check(n<=65536,"Writer fixture blob bound");std::vector<std::uint8_t> v(n);if(n)file.read(reinterpret_cast<char*>(v.data()),n);Check(bool(file),"Read writer fixture blob");return v;}
        State Words(){State v{};for(auto& word:v)word=U32();return v;}
    };
    void OpenData(spMemoryStream& stream,const std::vector<std::uint8_t>& bytes)
    {
        Check(stream.ResizeAndSetSize(static_cast<unsigned>(bytes.size())),"Allocate writer input stream");
        if(!bytes.empty())std::memcpy(stream.GetBuffer(),bytes.data(),bytes.size());
        Check(stream.Seek(spStream::SeekSource::essStart,0),"Rewind writer input stream");
    }
    struct Output final : spStream
    {
        spMemoryStream data;std::vector<Event> events;unsigned failWrite=0,attempts=0;
        Output(){Check(data.Open("projection-writer-fixture"),"Open writer output stream");}
        bool Open(const char*)override{return false;}
        bool Open(std::uint32_t,const char*)override{return false;}
        bool Close()override{return data.Close();}
        bool Seek(SeekSource mode,std::int32_t offset)override
        {
            if(!data.Seek(mode,offset))return false;std::uint32_t position=0;
            if(!data.GetCurrentPosition(position))return false;
            events.push_back({2,std::uint32_t(mode),std::uint32_t(offset),position});return true;
        }
        bool GetCurrentPosition(std::uint32_t& position)const override{return data.GetCurrentPosition(position);}
        bool ReadData(void* p,std::uint32_t n)override{return data.ReadData(p,n);}
        bool WriteData(const void* p,std::uint32_t n)override
        {
            std::uint32_t position=0;if(!data.GetCurrentPosition(position)||++attempts==failWrite)return false;
            if(!data.WriteData(p,n))return false;events.push_back({1,position,n,0});return true;
        }
        bool vfunc_WriteFromStream(spStream*,std::uint32_t)override{return false;}
        bool GetSize(std::uint32_t* size)const override{return data.GetSize(size);}
    };
    State Capture(const spPyramidProjection& object)
    {
        State r{};const auto& s=object.GetProjectionStateForAnalysis();
        for(unsigned i=0;i<3;++i)r[i]=s.flags58[i];for(unsigned i=0;i<4;++i)r[3+i]=s.words5C[i];r[7]=s.word6C;
        std::memcpy(r.data()+8,s.sphere70.data(),16);r[12]=object.IsAlphaSortEnabledForAnalysis();r[13]=object.GetPriorityForAnalysis();
        std::memcpy(r.data()+14,object.GetCenterForAnalysis().data(),12);std::memcpy(r.data()+17,object.GetSizeForAnalysis().data(),12);r[20]=object.GetPyramidStateForAnalysis().flip9C;return r;
    }
    void Match(const State& a,const State& b)
    {for(unsigned i=0;i<21;++i){++stateWords;if(a[i]!=b[i])std::cerr<<"State word "<<i<<" got="<<std::hex<<a[i]<<" expected="<<b[i]<<std::dec<<'\n';Check(a[i]==b[i],"Native writer/readback state");}}
    void Prepare(spPyramidProjection& object,const State& values,spSerializerManager& manager,spResourceManager& resources)
    {
        // Use the actual shared raw geometry reader, rather than extending the
        // finite geometry fixture setter to nonfinite/unqualified math inputs.
        std::vector<std::uint8_t> bytes{0,0,0xA0,25};
        const auto* geometry=reinterpret_cast<const std::uint8_t*>(values.data()+14);bytes.insert(bytes.end(),geometry,geometry+24);bytes.push_back(static_cast<std::uint8_t>(values[20]));bytes.push_back(0);
        spMemoryStream stream;OpenData(stream,bytes);spSerializerReadContextForAnalysis context(manager,resources);std::string error;
        Check(spPyramidProjectionSerializer{}.ReadPayloadForAnalysis(context,stream,static_cast<unsigned>(bytes.size()),object,&error)&&!context.failed,error.c_str());
        auto& s=object.ProjectionStateForFixture();for(unsigned i=0;i<3;++i)s.flags58[i]=std::uint8_t(values[i]);
        for(unsigned i=0;i<4;++i)s.words5C[i]=values[3+i];s.word6C=values[7];std::memcpy(s.sphere70.data(),values.data()+8,16);
        object.SetAlphaSortEnabledForAnalysis(values[12]!=0);object.SetPriorityForAnalysis(values[13]);
    }
    struct NodeState
    {
        spNode::Vector3 local,world,scale;spNode::Matrix3 orientation;std::uint32_t flags;
        explicit NodeState(const spRenderNode& n):local(n.GetPositionForAnalysis()),world(n.GetWorldPositionForAnalysis()),scale(n.GetWorldScaleForAnalysis()),orientation(n.GetWorldOrientationForAnalysis()),flags(n.GetFlagsForAnalysis()){}
        bool Same(const spRenderNode& n)const{return local==n.GetPositionForAnalysis()&&world==n.GetWorldPositionForAnalysis()&&scale==n.GetWorldScaleForAnalysis()&&orientation==n.GetWorldOrientationForAnalysis()&&flags==n.GetFlagsForAnalysis()&&n.GetRenderableCountForAnalysis()==0;}
    };
    void Replay(const char* path)
    {
        Reader in(path);Check(in.U32()==0x31575059,"YPW1 fixture");const auto count=in.U32();Check(count<=64,"Writer fixture count");
        for(unsigned row=0;row<count;++row)
        {
            const auto label=in.Blob();const bool hasState=true;const auto input=in.Words();const auto calls=in.U32();Check(calls&&calls<=2,"Writer call sequence bound");
            try
            {
                auto node=std::make_shared<spRenderNode>();const NodeState oldNode(*node);const auto oldRefs=node.use_count();
                spSerializerManager manager;spResourceManager resources;auto* fat=manager.GetFATForAnalysis();
                Check(fat&&fat->SetNextResourceIDForAnalysis(7)&&fat->IndexObjectForAnalysis(spRenderNode::ClassID,*node),"Captured retained Node FAT input");
                auto* entry=fat->FindByObjectForAnalysis(*node);Check(entry&&entry->id==7,"Known written Node role");entry->payloadWritten=true;
                const auto oldID=entry->id,oldClass=entry->classID,oldOffset=entry->offset,oldSize=entry->size;auto* oldObject=entry->object;
                spPyramidProjection object(spProjectionConstructorDataForAnalysis{0});Prepare(object,input,manager,resources);if(hasState)Match(Capture(object),input);
                Output output;std::string error;
                for(unsigned call=0;call<calls;++call)
                {
                    const bool bound=in.U32()!=0;const auto expected=in.Blob();State expectedRead{};if(hasState)expectedRead=in.Words();
                    const auto eventCount=in.U32();Check(eventCount<=256,"Native writer event bound");std::vector<Event> expectedEvents(eventCount);for(auto& e:expectedEvents)for(auto& v:e)v=in.U32();
                    object.SetBoundRenderNodeForAnalysis(bound?node.get():nullptr);const auto before=Capture(object);const auto eventStart=output.events.size();std::uint32_t start=0;Check(output.GetCurrentPosition(start),"Locate writer output");
                    spPyramidProjectionSerializer serializer;
                    // Both adapters share the qualified path; bound input must
                    // use the context form and common SaveReference operation.
                    const bool ok=bound||row%2?serializer.WritePayloadWithContextForAnalysis(manager,output,object,&error):serializer.WritePayloadForAnalysis(output,object,&error);
                    Check(ok,error.c_str());Check(object.GetPyramidStateForAnalysis().wordB8==0&&object.GetPyramidStateForAnalysis().byteCC==1,"Writer preserves B8/CC");++writes;if(hasState)Match(Capture(object),input);else Check(Capture(object)==before,"Writer preserves captured source state");
                    std::uint32_t end=0;Check(output.GetCurrentPosition(end)&&end>=start&&end-start==expected.size(),"Native writer payload length");
                    Check(!std::memcmp(static_cast<const std::uint8_t*>(output.data.GetBuffer())+start,expected.data(),expected.size()),"Exact original writer bytes");wireBytes+=static_cast<unsigned>(expected.size());
                    const std::vector<Event> events(output.events.begin()+eventStart,output.events.end());Check(events==expectedEvents,"Exact successful native write/seek order");ioEvents+=static_cast<unsigned>(events.size());
                    spPyramidProjection destination(spProjectionConstructorDataForAnalysis{0});spMemoryStream source;OpenData(source,expected);spSerializerReadContextForAnalysis context(manager,resources);
                    Check(serializer.ReadPayloadForAnalysis(context,source,static_cast<unsigned>(expected.size()),destination,&error)&&!context.failed,error.c_str());++reads;
                    if(hasState)Match(Capture(destination),expectedRead);
                    Check(destination.GetBoundRenderNodeForAnalysis()==(bound?node.get():nullptr),"Native readback borrowed Node role");
                    Check(oldNode.Same(*node)&&node.use_count()==oldRefs,"External Node lifetime/state stays unchanged");
                    Check(entry->id==oldID&&entry->classID==oldClass&&entry->offset==oldOffset&&entry->size==oldSize&&entry->object==oldObject&&entry->payloadWritten,"Already written FAT entry remains unchanged");
                }
                ++nativeCases;
            }
            catch(const std::exception& e){throw std::runtime_error(std::string(label.begin(),label.end())+": "+e.what());}
        }
        Check(in.file.peek()==std::char_traits<char>::eof(),"Complete writer fixture");
    }
    void Basic()
    {
        spPyramidProjection object(spProjectionConstructorDataForAnalysis{0});spRenderNode node;Output output;std::string error;
        object.SetBoundRenderNodeForAnalysis(&node);
        Check(!spPyramidProjectionSerializer{}.WritePayloadForAnalysis(output,object,&error)&&!error.empty()&&output.events.empty(),"Pyramid missing prewritten context refuses before output");
        object.SetBoundRenderNodeForAnalysis(nullptr);Output failed;failed.failWrite=1;
        Check(!spPyramidProjectionSerializer{}.WritePayloadForAnalysis(failed,object,&error),"Source-only failed Pyramid stream is not reported successful");
        struct Unsupported final:spPyramidProjectionSerializer
        {spClassID GetTargetClassIDForAnalysis()const noexcept override{return 0xDEADBEEF;}} unsupported;
        Output untouched;Check(!unsupported.WritePayloadForAnalysis(untouched,object,&error)&&untouched.events.empty(),"Unknown descendant adapter refuses before base-only bytes");
        struct Reloaded final:spPyramidProjection
        {
            struct Block{std::uint8_t flip=0;Vector3 center{},size{1,1,1};};
            std::array<Block,3> blocks{};unsigned calls=0;
            Reloaded():spPyramidProjection(spProjectionConstructorDataForAnalysis{0})
            {blocks[0].center={3,4,5};blocks[1].size={6,7,8};blocks[2].flip=0xA5;}
            SerializedFieldsForAnalysis GetSerializedFieldsForAnalysis()noexcept override
            {const auto i=calls<3?calls:2;++calls;return {blocks[i].flip,blocks[i].center,blocks[i].size};}
        } reloaded;
        Output encoded;Check(spPyramidProjectionSerializer{}.WritePayloadForAnalysis(encoded,reloaded,&error)&&reloaded.calls==3,"Source-only writer reacquires three typed views");
        std::uint32_t length=0;Check(encoded.GetSize(&length),"Source-only witness size");
        std::vector<std::uint8_t> bytes(length);std::memcpy(bytes.data(),encoded.data.GetBuffer(),length);spMemoryStream input;OpenData(input,bytes);
        spPyramidProjection destination(spProjectionConstructorDataForAnalysis{0});spSerializerManager readerManager;spResourceManager readerResources;spSerializerReadContextForAnalysis context(readerManager,readerResources);
        Check(spPyramidProjectionSerializer{}.ReadPayloadForAnalysis(context,input,length,destination,&error)&&!context.failed
            &&destination.GetCenterForAnalysis()==spPyramidProjection::Vector3{3,4,5}&&destination.GetSizeForAnalysis()==spPyramidProjection::Vector3{6,7,8}
            &&destination.GetPyramidStateForAnalysis().flip9C==0xA5,"Source-only writer uses center/size/flip from their separately resolved views");
    }
}
int main(int argc,char** argv)
{
    try{Basic();writer_profile::Basic();if(argc==3&&std::string(argv[1])=="--write")
        {writer_profile::Replay(argv[2]);std::cout<<"PASS PyramidWrite nativeCases="<<writer_profile::nativeCases<<" writes="<<writer_profile::writes<<" reads="<<writer_profile::reads<<" wireBytes="<<writer_profile::wireBytes<<" ioEvents="<<writer_profile::ioEvents<<" stateWords="<<writer_profile::stateWords<<" checks="<<writer_profile::checks<<'\n';return 0;}
        for(int i=1;i<argc;++i)Replay(argv[i]);std::cout<<"PASS PyramidProjectionSerializer bounded PC reader nativeCases="<<cases<<" scalarComparisons="<<comparisons<<" checks="<<checks<<'\n';return 0;}
    catch(const std::exception& error){std::cerr<<"FAIL PyramidProjectionSerializer "<<error.what()<<'\n';return 1;}
}
