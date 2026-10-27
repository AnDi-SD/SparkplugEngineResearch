#include "Code/Sparkplug/spProjectionSerializer.h"
#include "Code/Sparkplug/spBoxProjectionSerializer.h"
#include "Code/Sparkplug/spPyramidProjectionSerializer.h"
#include "Code/Sparkplug/spBoxProjection.h"
#include "Code/Sparkplug/spPyramidProjection.h"
#include "Code/Sparkplug/spRenderNode.h"
#include "Code/Sparkplug/spFog.h"
#include "Code/SparkplugDX/spDXMaterial.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceFATSerializer.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <cstring>
#include <array>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace sparkplug::reconstruction;
namespace
{
    unsigned checks=0,nativeCases=0,writes=0,reads=0,wireBytes=0,ioEvents=0,stateWords=0;
    void Check(bool v,const char* why){++checks;if(!v)throw std::runtime_error(why);}
    using State=std::array<std::uint32_t,20>;
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
    State Capture(const spBoxProjection& object)
    {
        State r{};const auto& s=object.GetProjectionStateForAnalysis();
        for(unsigned i=0;i<3;++i)r[i]=s.flags58[i];for(unsigned i=0;i<4;++i)r[3+i]=s.words5C[i];r[7]=s.word6C;
        std::memcpy(r.data()+8,s.sphere70.data(),16);r[12]=object.IsAlphaSortEnabledForAnalysis();r[13]=object.GetPriorityForAnalysis();
        std::memcpy(r.data()+14,object.GetCenterForAnalysis().data(),12);std::memcpy(r.data()+17,object.GetSizeForAnalysis().data(),12);return r;
    }
    void Match(const State& a,const State& b)
    {for(unsigned i=0;i<20;++i){++stateWords;if(a[i]!=b[i])std::cerr<<"State word "<<i<<" got="<<std::hex<<a[i]<<" expected="<<b[i]<<std::dec<<'\n';Check(a[i]==b[i],"Native writer/readback state");}}
    void Prepare(spBoxProjection& object,const State& values,spSerializerManager& manager,spResourceManager& resources)
    {
        // Use the actual shared raw geometry reader, rather than extending the
        // finite geometry fixture setter to nonfinite/unqualified math inputs.
        std::vector<std::uint8_t> bytes{0,0,0xA0,24};
        const auto* geometry=reinterpret_cast<const std::uint8_t*>(values.data()+14);bytes.insert(bytes.end(),geometry,geometry+24);bytes.push_back(0);
        spMemoryStream stream;OpenData(stream,bytes);spSerializerReadContextForAnalysis context(manager,resources);std::string error;
        Check(spBoxProjectionSerializer{}.ReadPayloadForAnalysis(context,stream,static_cast<unsigned>(bytes.size()),object,&error)&&!context.failed,error.c_str());
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
        Reader in(path);Check(in.U32()==0x31575350,"PSW1 fixture");const auto count=in.U32();Check(count<=64,"Writer fixture count");
        for(unsigned row=0;row<count;++row)
        {
            const auto label=in.Blob();const bool box=in.U32()!=0,hasState=in.U32()!=0;const auto input=in.Words();const auto calls=in.U32();Check(calls&&calls<=2,"Writer call sequence bound");
            try
            {
                auto node=std::make_shared<spRenderNode>();const NodeState oldNode(*node);const auto oldRefs=node.use_count();
                spSerializerManager manager;spResourceManager resources;auto* fat=manager.GetFATForAnalysis();
                Check(fat&&fat->SetNextResourceIDForAnalysis(7)&&fat->IndexObjectForAnalysis(spRenderNode::ClassID,*node),"Captured retained Node FAT input");
                auto* entry=fat->FindByObjectForAnalysis(*node);Check(entry&&entry->id==7,"Known written Node role");entry->payloadWritten=true;
                const auto oldID=entry->id,oldClass=entry->classID,oldOffset=entry->offset,oldSize=entry->size;auto* oldObject=entry->object;
                spBoxProjection object(spProjectionConstructorDataForAnalysis{0});Prepare(object,input,manager,resources);if(hasState)Match(Capture(object),input);
                Output output;std::string error;
                for(unsigned call=0;call<calls;++call)
                {
                    const bool bound=in.U32()!=0;const auto expected=in.Blob();State expectedRead{};if(hasState)expectedRead=in.Words();
                    const auto eventCount=in.U32();Check(eventCount<=256,"Native writer event bound");std::vector<Event> expectedEvents(eventCount);for(auto& e:expectedEvents)for(auto& v:e)v=in.U32();
                    object.SetBoundRenderNodeForAnalysis(bound?node.get():nullptr);const auto before=Capture(object);const auto eventStart=output.events.size();std::uint32_t start=0;Check(output.GetCurrentPosition(start),"Locate writer output");
                    spProjectionSerializer projectionSerializer;spBoxProjectionSerializer boxSerializer;
                    const spSerializer& serializer=box?static_cast<const spSerializer&>(boxSerializer):static_cast<const spSerializer&>(projectionSerializer);
                    // Both adapters share the qualified path; bound input must
                    // use the context form and common SaveReference operation.
                    const bool ok=bound||row%2?serializer.WritePayloadWithContextForAnalysis(manager,output,object,&error):serializer.WritePayloadForAnalysis(output,object,&error);
                    Check(ok,error.c_str());++writes;if(hasState)Match(Capture(object),input);else Check(Capture(object)==before,"Writer preserves captured source state");
                    std::uint32_t end=0;Check(output.GetCurrentPosition(end)&&end>=start&&end-start==expected.size(),"Native writer payload length");
                    Check(!std::memcmp(static_cast<const std::uint8_t*>(output.data.GetBuffer())+start,expected.data(),expected.size()),"Exact original writer bytes");wireBytes+=static_cast<unsigned>(expected.size());
                    const std::vector<Event> events(output.events.begin()+eventStart,output.events.end());Check(events==expectedEvents,"Exact successful native write/seek order");ioEvents+=static_cast<unsigned>(events.size());
                    spBoxProjection destination(spProjectionConstructorDataForAnalysis{0});spMemoryStream source;OpenData(source,expected);spSerializerReadContextForAnalysis context(manager,resources);
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
        spBoxProjection object(spProjectionConstructorDataForAnalysis{0});spRenderNode node;Output output;std::string error;
        object.SetBoundRenderNodeForAnalysis(&node);
        Check(!spProjectionSerializer{}.WritePayloadForAnalysis(output,object,&error)&&!error.empty()&&output.events.empty(),"Missing written reference context refuses before output");
        spSerializerManager manager;Check(!spBoxProjectionSerializer{}.WritePayloadWithContextForAnalysis(manager,output,object,&error)&&output.events.empty(),"Unindexed reference refuses before output");
        object.SetBoundRenderNodeForAnalysis(nullptr);Output failed;failed.failWrite=1;
        Check(!spBoxProjectionSerializer{}.WritePayloadForAnalysis(failed,object,&error),"Source-only failed stream is not reported successful");
        struct UnknownPyramidWriter final : spPyramidProjectionSerializer
        {spClassID GetTargetClassIDForAnalysis()const noexcept override{return 0xFFFFFFFFu;}};
        spPyramidProjection pyramid(spProjectionConstructorDataForAnalysis{0});Output unsupported;
        Check(!UnknownPyramidWriter{}.WritePayloadForAnalysis(unsupported,pyramid,&error)&&unsupported.events.empty(),"Unqualified derived writer refuses before base-only output");
    }
}
namespace relations_profile
{
    using Words=std::array<std::uint32_t,21>;
    using Counts=std::array<std::uint32_t,5>;
    unsigned countChecks=0;
    Words Capture(spProjection& object)
    {
        Words r{};const auto& s=object.GetProjectionStateForAnalysis();
        for(unsigned i=0;i<3;++i)r[i]=s.flags58[i];for(unsigned i=0;i<4;++i)r[3+i]=s.words5C[i];r[7]=s.word6C;
        std::memcpy(r.data()+8,s.sphere70.data(),16);r[12]=object.IsAlphaSortEnabledForAnalysis();r[13]=object.GetPriorityForAnalysis();
        if(auto* pyramid=dynamic_cast<spPyramidProjection*>(&object))
        {auto fields=pyramid->GetSerializedFieldsForAnalysis();std::memcpy(r.data()+14,fields.centerA0.data(),12);std::memcpy(r.data()+17,fields.sizeAC.data(),12);r[20]=fields.flip9C;}
        else
        {auto& box=dynamic_cast<spBoxProjection&>(object);std::memcpy(r.data()+14,box.GetCenterForAnalysis().data(),12);std::memcpy(r.data()+17,box.GetSizeForAnalysis().data(),12);}
        return r;
    }
    void Match(const Words& a,const Words& b)
    {for(unsigned i=0;i<a.size();++i){++stateWords;Check(a[i]==b[i],"Prewritten relationship writer/readback raw state");}}
    std::unique_ptr<spProjection> Object(unsigned kind)
    {if(kind==2)return std::make_unique<spPyramidProjection>(spProjectionConstructorDataForAnalysis{0});return std::make_unique<spBoxProjection>(spProjectionConstructorDataForAnalysis{0});}
    std::unique_ptr<spSerializer> Codec(unsigned kind)
    {if(kind==2)return std::make_unique<spPyramidProjectionSerializer>();if(kind==1)return std::make_unique<spBoxProjectionSerializer>();return std::make_unique<spProjectionSerializer>();}
    std::vector<std::uint32_t> RelationState(const spBaseObject& object)
    {
        std::vector<std::uint32_t> r;
        if(const auto* material=dynamic_cast<const spDXMaterial*>(&object))
        {
            const auto& states=material->GetRenderStatesForAnalysis();r.insert(r.end(),states.begin(),states.end());
            r.push_back(material->GetVertexAlphaByteForAnalysis());r.push_back(material->GetRenderOverrideByteForAnalysis());
            r.push_back(static_cast<std::uint32_t>(material->GetPassCountForAnalysis()));r.push_back(material->GetOpaqueRuntimeFieldForAnalysis());
            r.push_back(material->HasInitializedSpecularPowerForAnalysis());
            for(const auto* color:{&material->GetAmbientColorForAnalysis(),&material->GetDiffuseColorForAnalysis(),&material->GetSpecularColorForAnalysis(),&material->GetEmissiveColorForAnalysis()})
            {std::array<std::uint32_t,4> words{};std::memcpy(words.data(),color->data(),16);r.insert(r.end(),words.begin(),words.end());}
            Check(material->GetMaterialColorControllerForAnalysis()==nullptr,"Default material controller stays null");
        }
        else
        {
            const auto& fog=dynamic_cast<const spFog&>(object);r={std::uint32_t(fog.GetTypeForAnalysis()),fog.GetColorARGBForAnalysis()};
            const std::array<float,3> values{fog.GetStartForAnalysis(),fog.GetEndForAnalysis(),fog.GetDensityForAnalysis()};
            std::array<std::uint32_t,3> raw{};std::memcpy(raw.data(),values.data(),12);r.insert(r.end(),raw.begin(),raw.end());
        }
        return r;
    }
    void Replay(const char* path)
    {
        Reader in(path);Check(in.U32()==0x31525750,"PWR1 prewritten relationships fixture");const auto rows=in.U32();Check(rows==6,"Six whole prewritten cases");
        for(unsigned row=0;row<rows;++row)
        {
            const unsigned kind=in.U32(),mode=in.U32(),flags=in.U32();Check(kind<=2&&mode<=2,"Known relationship profile");
            Words input{},expectedRead{};for(auto& v:input)v=in.U32();for(auto& v:expectedRead)v=in.U32();const auto bytes=in.Blob();
            const auto eventCount=in.U32();Check(eventCount<=256,"Relationship native event bound");std::vector<Event> expectedEvents(eventCount);for(auto& e:expectedEvents)for(auto& v:e)v=in.U32();
            std::array<Counts,5> expectedCounts{};for(auto& stage:expectedCounts)for(auto& v:stage)v=in.U32();
            spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
            auto source=Object(kind),destination=Object(kind);auto codec=Codec(kind);auto* fat=manager.GetFATForAnalysis();Check(fat!=nullptr,"Existing FAT for relationships");
            std::array<std::shared_ptr<spBaseObject>,4> owners{};
            if(flags&1)owners[0]=std::make_shared<spDXMaterial>();if(flags&2)owners[1]=std::make_shared<spFog>();
            if(mode==2){if(flags&1)owners[2]=std::make_shared<spDXMaterial>();if(flags&2)owners[3]=std::make_shared<spFog>();}
            source->SetMaterialForAnalysis(owners[0]);source->SetFogForAnalysis(owners[1]);
            if(mode){destination->SetMaterialForAnalysis(owners[mode==1?0:2]);destination->SetFogForAnalysis(owners[mode==1?1:3]);}
            std::vector<std::pair<const spResourceFATEntryForAnalysis*,spResourceFATEntryForAnalysis>> entries;
            const auto index=[&](spBaseObject& object,unsigned id)
            {
                Check(fat->SetNextResourceIDForAnalysis(id)&&fat->IndexObjectForAnalysis(object.vfunc_18().classID,object),"Publish actual typed prewritten FAT role");
                auto* entry=fat->FindByObjectForAnalysis(object);Check(entry&&entry->id==id,"Known relationship resource ID");entry->payloadWritten=true;entries.push_back({entry,*entry});
            };
            for(unsigned i=0;i<2;++i)if(owners[i]){index(*owners[i],i?9:7);context.externalOwners.push_back(owners[i]);}
            auto node=std::make_shared<spRenderNode>();NodeState nodeState(*node);
            if(flags&4){index(*node,11);source->SetBoundRenderNodeForAnalysis(node.get());}
            auto& state=source->ProjectionStateForFixture();for(unsigned i=0;i<3;++i)state.flags58[i]=std::uint8_t(input[i]);for(unsigned i=0;i<4;++i)state.words5C[i]=input[3+i];state.word6C=input[7];std::memcpy(state.sphere70.data(),input.data()+8,16);
            source->SetAlphaSortEnabledForAnalysis(input[12]!=0);source->SetPriorityForAnalysis(input[13]);
            if(kind==2){auto view=dynamic_cast<spPyramidProjection&>(*source).GetSerializedFieldsForAnalysis();std::memcpy(view.centerA0.data(),input.data()+14,12);std::memcpy(view.sizeAC.data(),input.data()+17,12);view.flip9C=std::uint8_t(input[20]);}
            std::array<long,5> baseline{};for(unsigned i=0;i<4;++i)if(owners[i])baseline[i]=owners[i].use_count();baseline[4]=node.use_count();
            std::array<std::vector<std::uint32_t>,4> oldState{};for(unsigned i=0;i<4;++i)if(owners[i])oldState[i]=RelationState(*owners[i]);
            const auto guards=[&](unsigned stage)
            {
                for(unsigned i=0;i<5;++i)
                {
                    const bool present=i==4?(flags&4)!=0:bool(owners[i]);const long current=i==4?node.use_count():(owners[i]?owners[i].use_count():0);
                    const auto normalized=present?std::uint32_t(4+current-baseline[i]):0;++countChecks;
                    Check(normalized==expectedCounts[stage][i],"Native owning count delta versus portable shared owner roles");
                    if(i<4&&owners[i])Check(RelationState(*owners[i])==oldState[i],"Retained Material/Fog state preserved");
                }
                Check(nodeState.Same(*node),"Borrowed Node state remains unchanged");
                for(const auto& pair:entries){const auto& a=*pair.first;const auto& b=pair.second;Check(a.id==b.id&&a.fileID==b.fileID&&a.name==b.name&&a.nameIsNullForAnalysis==b.nameIsNullForAnalysis&&a.classID==b.classID&&a.offset==b.offset&&a.size==b.size&&a.payloadWritten==b.payloadWritten&&a.object==b.object,"Prewritten FAT entry preserved");}
            };
            Match(Capture(*source),input);guards(0);Output output;std::string error;
            Check(codec->WritePayloadWithContextForAnalysis(manager,output,*source,&error),error.c_str());++writes;
            std::uint32_t size=0;Check(output.GetSize(&size)&&size==bytes.size()&&!std::memcmp(output.data.GetBuffer(),bytes.data(),size),"Exact native prewritten relationship payload");wireBytes+=size;
            Check(output.events==expectedEvents,"Exact prewritten relationship write/seek order");ioEvents+=static_cast<unsigned>(output.events.size());Check(Capture(*source)==input,"Writer keeps source data");guards(1);
            spMemoryStream stream;OpenData(stream,bytes);Check(codec->ReadPayloadForAnalysis(context,stream,size,*destination,&error)&&!context.failed,error.c_str());++reads;
            Match(Capture(*destination),expectedRead);Check(destination->GetMaterialForAnalysis()==owners[0]&&destination->GetFogForAnalysis()==owners[1],"Real shared owning readback from external owner cache");
            Check(destination->GetBoundRenderNodeForAnalysis()==((flags&4)?node.get():nullptr),"Readback84 remains borrowed");guards(2);
            destination.reset();guards(3);source.reset();guards(4);++nativeCases;
        }
        Check(in.file.peek()==std::char_traits<char>::eof(),"Complete prewritten relationship fixture");
    }
    void Basic()
    {
        spBoxProjection object(spProjectionConstructorDataForAnalysis{0});object.SetMaterialForAnalysis(std::make_shared<spDXMaterial>());Output missing;std::string error;
        Check(!spProjectionSerializer{}.WritePayloadForAnalysis(missing,object,&error)&&missing.events.empty(),"Material relationship lacks context before output");
        spSerializerManager manager;Output unwritten;auto* fat=manager.GetFATForAnalysis();Check(fat&&fat->IndexObjectForAnalysis(spDXMaterial::ClassID,*object.GetMaterialForAnalysis()),"Prepare unwritten material input");
        Check(!spBoxProjectionSerializer{}.WritePayloadWithContextForAnalysis(manager,unwritten,object,&error)&&unwritten.events.empty(),"Unwritten material refuses before output");
        object.SetMaterialForAnalysis(std::make_shared<spRenderNode>());Output wrong;Check(!spProjectionSerializer{}.WritePayloadWithContextForAnalysis(manager,wrong,object,&error)&&wrong.events.empty(),"Wrong material type refuses before output");
    }
}
int main(int argc,char** argv)
{
    try{Basic();relations_profile::Basic();if(argc==3&&std::string(argv[1])=="--relations")relations_profile::Replay(argv[2]);else for(int i=1;i<argc;++i)Replay(argv[i]);std::cout<<"PASS ProjectionWrite nativeCases="<<nativeCases<<" writes="<<writes<<" reads="<<reads<<" wireBytes="<<wireBytes<<" ioEvents="<<ioEvents<<" stateWords="<<stateWords<<" countComparisons="<<relations_profile::countChecks<<" checks="<<checks<<'\n';return 0;}
    catch(const std::exception& e){std::cerr<<"FAIL ProjectionWrite "<<e.what()<<'\n';return 1;}
}
