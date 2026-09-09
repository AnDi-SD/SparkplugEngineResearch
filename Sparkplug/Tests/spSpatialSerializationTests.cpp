#include "Code/Sparkplug/spPartitionNodeSerializer.h"
#include "Code/Sparkplug/spBSPNodeSerializer.h"
#include "Code/Sparkplug/spPartitionSystemSerializer.h"
#include "Code/Sparkplug/spZoneSerializer.h"
#include "Code/Sparkplug/spZonePortalSerializer.h"
#include "Code/Sparkplug/spZonePortalNodeSerializer.h"
#include "Code/Sparkplug/spPartitionRenderableSerializer.h"
#include "Code/Sparkplug/spPartitionSystem.h"
#include "Code/Sparkplug/spBSPNode.h"
#include "Code/Sparkplug/spZone.h"
#include "Code/Sparkplug/spZonePortalNode.h"
#include "Code/Sparkplug/spStaticRenderObject.h"
#include "Code/SparkplugPC/spPCPartitionRenderable.h"
#include "Code/Sparkplug/spCollisionInfo.h"
#include "Code/Sparkplug/spOBBBV.h"
#include "Code/Sparkplug/spModel.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceFATSerializer.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>

using namespace sparkplug::reconstruction;
namespace
{
    using Bytes=std::vector<std::uint8_t>;
    unsigned checks=0;
    void Check(bool value,const char* text){++checks;if(!value)throw std::runtime_error(text);}
    template<class T>void Add(Bytes& bytes,const T& value)
    {const auto* p=reinterpret_cast<const std::uint8_t*>(&value);bytes.insert(bytes.end(),p,p+sizeof(value));}
    void Open(spMemoryStream& stream,const Bytes& bytes)
    {
        Check(stream.ResizeAndSetSize(static_cast<std::uint32_t>(bytes.size())),"bounded test stream");
        if(!bytes.empty())std::memcpy(stream.GetBuffer(),bytes.data(),bytes.size());
    }
    Bytes FromHex(const std::string& value)
    {
        Check(value.size()%2==0,"even hex input");Bytes result;
        for(std::size_t i=0;i<value.size();i+=2)result.push_back(static_cast<std::uint8_t>(std::stoul(value.substr(i,2),nullptr,16)));
        return result;
    }
    template<class T>std::string Hex(const T& value)
    {
        const auto* p=reinterpret_cast<const std::uint8_t*>(&value);std::string result;
        for(std::size_t i=0;i<sizeof(value);++i){result+="0123456789abcdef"[p[i]>>4];result+="0123456789abcdef"[p[i]&15];}
        return result;
    }
    template<class T>std::unique_ptr<spBaseObject> Make()
    {(void)T::StaticRTTI();return std::make_unique<T>();}
    std::unique_ptr<spBaseObject> Reference(unsigned id)
    {
        switch(id)
        {
        case 7:return Make<spZone>();case 9:return Make<spPartitionSystem>();
        case 11:return Make<spStaticRenderObject>();case 13:return Make<spZonePortal>();
        case 15:return Make<spPCPartitionRenderable>();case 17:case 19:return Make<spPartitionNode>();
        case 21:case 23:return Make<spModel>();
        case 25:
        {
            auto collision=std::make_unique<spCollisionInfo>();(void)spCollisionInfo::StaticRTTI();
            collision->SetPrimitiveForAnalysis(std::make_shared<spOBBBV>());return collision;
        }
        default:throw std::runtime_error("unknown reference fixture");
        }
    }
    std::string Capture(const std::string& mode,const Bytes& wire)
    {
        const auto kind=mode.substr(0,mode.find(':'));
        std::unique_ptr<spBaseObject> target;std::unique_ptr<spSerializer> serializer;
        if(kind=="partition"){target=Make<spPartitionNode>();serializer=std::make_unique<spPartitionNodeSerializer>();}
        else if(kind=="bsp"){target=Make<spBSPNode>();serializer=std::make_unique<spBSPNodeSerializer>();}
        else if(kind=="system"){target=Make<spPartitionSystem>();serializer=std::make_unique<spPartitionSystemSerializer>();}
        else if(kind=="zone"){target=Make<spZone>();serializer=std::make_unique<spZoneSerializer>();}
        else if(kind=="portal"){target=Make<spZonePortal>();serializer=std::make_unique<spZonePortalSerializer>();}
        else if(kind=="portal-node"){target=Make<spZonePortalNode>();serializer=std::make_unique<spZonePortalNodeSerializer>();}
        else if(kind=="payload"){target=Make<spPCPartitionRenderable>();serializer=std::make_unique<spPartitionRenderableSerializer>();}
        else throw std::runtime_error("unknown spatial mode");
        spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
        context.directOwnedClassIDsForAnalysis={spPartitionNode::ClassID,spPartitionRenderable::ClassID};
        auto* object=context.PublishObjectForAnalysis(std::move(target));
        std::vector<unsigned> needed;
        if(mode.find(":values")!=std::string::npos)
        {
            if(kind=="partition")needed={7,9,11,13,15,25};
            else if(kind=="bsp"||kind=="zone")needed={17,19};
            else if(kind=="system")needed={17};
            else if(kind=="portal")needed={7};
            else if(kind=="portal-node")needed={13};
            else if(kind=="payload")needed={21,23};
        }
        std::map<unsigned,spBaseObject*> refs;Bytes directory;Add(directory,static_cast<unsigned>(needed.size()));
        for(auto id:needed)
        {
            auto value=Reference(id);const auto type=value->vfunc_18().classID;
            refs.emplace(id,context.PublishObjectForAnalysis(std::move(value)));
            Add(directory,id);Add(directory,std::uint16_t(0));Add(directory,type);Add(directory,0U);Add(directory,8U);
        }
        spMemoryStream directoryStream;Open(directoryStream,directory);
        Check(manager.GetFATForAnalysis()->LoadIndexForAnalysis(directoryStream),"actual FAT grammar with explicit published reference owners");
        for(const auto& [id,value]:refs)manager.GetFATForAnalysis()->FindByIDForAnalysis(id)->object=value;
        spMemoryStream input;Open(input,wire);std::string error;
        if(!serializer->ReadPayloadForAnalysis(context,input,static_cast<std::uint32_t>(wire.size()),*object,&error))
            throw std::runtime_error(error.empty()?"spatial reader failed":error);
        std::uint32_t position=0;
        Check(!context.failed&&input.GetCurrentPosition(position)&&position==wire.size(),"exact spatial payload consumed");
        const auto idOf=[&](const spBaseObject* pointer)->unsigned
        {
            if(!pointer)return 0;
            for(const auto& [id,value]:refs)if(value==pointer)return id;
            throw std::runtime_error("capture pointer outside fixture");
        };
        std::ostringstream out;
        const auto writeIDs=[&](const auto& values,const auto& pointer)
        {
            out<<'[';bool first=true;
            for(const auto& value:values){if(!first)out<<',';first=false;out<<idOf(pointer(value));}
            out<<']';
        };
        if(auto* partition=dynamic_cast<spPartitionNode*>(object))
        {
            out<<"{\"color\":"<<partition->GetDebugColorForAnalysis()<<",\"zone\":"<<idOf(partition->GetZoneForAnalysis())
                <<",\"system\":"<<idOf(partition->GetPartitionSystemForAnalysis())<<",\"payload\":"<<idOf(partition->GetPartitionRenderableForAnalysis())
                <<",\"portals\":";
            writeIDs(partition->GetPortalsForAnalysis(),[](const auto& v){return v.get();});out<<",\"statics\":";
            writeIDs(partition->GetStaticObjectsForAnalysis(),[](const auto& v){return v.get();});out<<",\"collisions\":";
            writeIDs(partition->GetCollisionsForAnalysis(),[](auto* v){return v;});out<<",\"children\":[";
            for(std::size_t i=0;i<partition->GetChildCountForAnalysis();++i)
            {
                if(i)out<<',';auto* child=partition->GetChildForAnalysis(i);out<<idOf(child);
                Check(!child||child->GetParentForAnalysis()==partition,"actual child parent back pointer");
            }
            out<<']';
            if(auto* bsp=dynamic_cast<spBSPNode*>(object))
                out<<",\"plane\":"<<(bsp->HasPlaneForAnalysis()?"\""+Hex(bsp->GetPlane())+"\"":"null")
                    <<",\"polygon_count\":"<<bsp->GetPolygonVertexCount();
            if(refs.count(25))
            {
                const auto& roots=static_cast<spCollisionInfo*>(refs.at(25))->GetPartitionsForAnalysis();
                Check(roots.size()==2&&roots[0]==partition&&roots[1]==partition,"reciprocal borrowed collision duplicates");
            }
            out<<'}';
        }
        else if(auto* system=dynamic_cast<spPartitionSystem*>(object))
        {
            Check(system->IsKindOf(spNode::ClassID)&&!system->IsKindOf(spRenderNode::ClassID),"RTTI skips physical RenderNode parent");
            out<<"{\"root\":"<<idOf(system->GetPartitionRootForAnalysis())<<",\"flags\":"<<system->GetFlagsForAnalysis()<<'}';
        }
        else if(auto* zone=dynamic_cast<spZone*>(object))
        {out<<"{\"roots\":";writeIDs(zone->GetRootsForAnalysis(),[](auto* v){return v;});out<<'}';}
        else if(auto* node=dynamic_cast<spZonePortalNode*>(object))
        {out<<"{\"portals\":";writeIDs(node->GetPortalsForAnalysis(),[](auto* v){return v;});out<<'}';}
        else if(auto* portal=dynamic_cast<spZonePortal*>(object))
            out<<"{\"destination\":"<<idOf(portal->GetDestinationZone())<<",\"open\":"<<unsigned(portal->GetOpenByteForAnalysis())
                <<",\"polygon_count\":"<<portal->GetPolygonVertexCount()<<",\"plane\":"
                <<(portal->HasPlaneForAnalysis()?"\""+Hex(portal->GetPlaneForAnalysis())+"\"":"null")<<'}';
        else if(auto* payload=dynamic_cast<spPartitionRenderable*>(object))
        {
            Check(payload->IsExactly(spPCPartitionRenderable::ClassID)&&payload->IsKindOf(spPartitionRenderable::ClassID),"base family has actual concrete PC object");
            out<<"{\"color\":"<<payload->GetDebugColorForAnalysis()<<",\"renderables\":";
            writeIDs(payload->GetRenderablesForAnalysis(),[](const auto& v){return v.get();});out<<",\"model_colors\":{";
            bool first=true;for(const auto& [id,value]:refs)
            {if(!first)out<<',';first=false;out<<'"'<<id<<"\":"<<static_cast<spModel*>(value)->GetField28ForAnalysis();}
            out<<"}}";
        }
        return out.str();
    }
    void Guards()
    {
        for(const auto* kind:{"partition","bsp","system","zone","portal","portal-node","payload"})
        {
            const std::string mode=std::string(kind)+":empty";
            const auto count=std::string(kind)=="system"?3:std::string(kind)=="bsp"||std::string(kind)=="zone"||std::string(kind)=="portal-node"?2:1;
            Check(!Capture(mode,Bytes(count,0)).empty(),"original permits empty spatial sections");
        }
        const std::pair<const char*,const char*> invalid[]{
            {"system:values","0000a0040000000000"},{"zone:values","00a0040000000000"},
            {"portal-node:values","00a0040000000000"},{"payload:values","a0040000000000"},
            {"bsp:values","a20c0200000011000000000000000000"},
            {"bsp:values","a20c000000001100000000000000a20c0100000011000000000000000000"},
            {"portal:values","a1040200000000"}};
        for(const auto& [mode,hex]:invalid)
        {
            bool rejected=false;
            try{(void)Capture(mode,FromHex(hex));}catch(const std::exception&){rejected=true;}
            Check(rejected,"null/extent/slot/direct alias host guard rejects unsafe spatial input");
        }
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==4&&std::string(argv[1])=="--capture")
        {std::cout<<Capture(argv[2],FromHex(argv[3]))<<'\n';return 0;}
        Guards();std::cout<<"PASS "<<checks<<'/'<<checks<<": spatial shared readers and ownership guards\n";return 0;
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
