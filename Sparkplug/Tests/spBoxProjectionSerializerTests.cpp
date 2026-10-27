#include "Code/Sparkplug/spBoxProjectionSerializer.h"
#include "Code/Sparkplug/spBoxProjection.h"
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
    void State(Reader& in,const spBoxProjection& object,const std::array<std::shared_ptr<spRenderNode>,2>& nodes)
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
            for(auto word:raw)Match(word,in.U32(),"Box raw geometry word");
        }
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
        Reader in(path);Check(in.U32()==0x31525342,"BSR1 fixture");const auto count=in.U32();Check(count<=64,"Fixture count bound");
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
                Check(!pyramid,"Box-only fixture receiver");
                auto object=std::make_unique<spBoxProjection>(spProjectionConstructorDataForAnalysis{0});
                object->SetBoundRenderNodeForAnalysis(initialBound?nodes[initialBound-1].get():nullptr);
                const auto oldSphere=object->GetProjectionStateForAnalysis().sphere70;const auto old98=object->GetProjectionStateForAnalysis().word98;
                const auto refs=std::array<long,2>{nodes[0].use_count(),nodes[1].use_count()};
                State(in,*object,nodes);spMemoryStream source;Open(source,payload);std::string error;
                const bool ok=spBoxProjectionSerializer{}.ReadPayloadForAnalysis(context,source,static_cast<unsigned>(payload.size()),*object,&error);
                Check(ok&&!context.failed,error.c_str());State(in,*object,nodes);Match(ok,in.U32(),"Whole reader return");
                std::uint32_t position=0;Check(source.GetCurrentPosition(position),"Read source position");Match(position,in.U32(),"Whole reader consumed payload");
                Match(before[0].Same(*nodes[0])&&before[1].Same(*nodes[1]),in.U32(),"Borrowed Node state preserved");
                Match(refs==std::array<long,2>{nodes[0].use_count(),nodes[1].use_count()},in.U32(),"Borrowed portable ownership roles preserved");
                Check(oldSphere==object->GetProjectionStateForAnalysis().sphere70&&old98==object->GetProjectionStateForAnalysis().word98,"Reader preserves other represented Projection state");
                object.reset();Match(before[0].Same(*nodes[0])&&before[1].Same(*nodes[1])&&refs==std::array<long,2>{nodes[0].use_count(),nodes[1].use_count()},in.U32(),"Projection deletion leaves borrowed Nodes alive");
                ++cases;
            }
            catch(const std::exception& error){throw std::runtime_error(std::string(label.begin(),label.end())+": "+error.what());}
        }
        Check(in.file.peek()==std::char_traits<char>::eof(),"Complete BSR1 fixture");
    }
    void Basic()
    {
        (void)spBoxProjectionSerializer::StaticRTTI();auto value=spRTTIManager::Instance().Create(spBoxProjectionSerializer::ClassID);
        auto* typed=dynamic_cast<spBoxProjectionSerializer*>(value.get());
        Check(typed&&typed->IsKindOf(spProjectionSerializer::ClassID)&&typed->GetTargetClassIDForAnalysis()==spBoxProjection::ClassID,"Concrete Box serializer hierarchy/target");
        spCloneManager clones;auto clone=typed->vfunc_10(clones);
        Check(dynamic_cast<spBoxProjectionSerializer*>(clone.get())&&clones.FindClone(*typed)==clone.get(),"Concrete serializer clone and shared map");
        spBoxProjection object(spProjectionConstructorDataForAnalysis{0});spMemoryStream output;
        Check(output.Open("box-writer-check"),"Open qualified writer stream");
        std::string error;spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
        Check(typed->WritePayloadForAnalysis(output,object,&error)&&error.empty()
            &&!typed->IndexRelationshipsForAnalysis(object),"Qualified whole raw sections retain separate index refusal");
        struct VectorRead final : spStream
        {
            spMemoryStream data;spBoxProjection& target;bool firstCommitted=false;
            explicit VectorRead(spBoxProjection& object):target(object)
            {std::vector<std::uint8_t> bytes{0,0,0xA0,24};for(float v:{3.f,4.f,5.f,6.f,7.f,8.f})Add(bytes,v);bytes.push_back(0);::Open(data,bytes);}
            bool Open(const char*)override{return false;}
            bool Open(std::uint32_t,const char*)override{return false;}
            bool Close()override{return data.Close();}
            bool Seek(SeekSource s,std::int32_t o)override{return data.Seek(s,o);}
            bool GetCurrentPosition(std::uint32_t& v)const override{return data.GetCurrentPosition(v);}
            bool ReadData(void* p,std::uint32_t n)override
            {std::uint32_t at=0;if(!data.GetCurrentPosition(at))return false;if(at==16&&n==12){firstCommitted=target.GetCenterForAnalysis()==spBoxProjection::Vector3{3,4,5};return false;}return data.ReadData(p,n);}
            bool WriteData(const void*,std::uint32_t)override{return false;}
            bool vfunc_WriteFromStream(spStream*,std::uint32_t)override{return false;}
            bool GetSize(std::uint32_t* v)const override{return data.GetSize(v);}
        } vectorRead(object);
        Check(!typed->ReadPayloadForAnalysis(context,vectorRead,29,object,&error)&&context.failed&&vectorRead.firstCommitted
            &&object.GetSizeForAnalysis()==spBoxProjection::Vector3{1,1,1},"Source-only second-read refusal preserves the committed center and old size");
    }
}
int main(int argc,char** argv)
{
    try{Basic();for(int i=1;i<argc;++i)Replay(argv[i]);std::cout<<"PASS BoxProjectionSerializer bounded PC reader nativeCases="<<cases<<" scalarComparisons="<<comparisons<<" checks="<<checks<<'\n';return 0;}
    catch(const std::exception& error){std::cerr<<"FAIL BoxProjectionSerializer "<<error.what()<<'\n';return 1;}
}
