#include "Code/Sparkplug/spNavigationGraph.h"
#include "Code/Sparkplug/spNavigationPortal.h"
#include "Code/Sparkplug/spMeshNavigationSet.h"
#include "Code/Sparkplug/spNavigationGraphSerializer.h"
#include "Code/Sparkplug/spNavigationPortalSerializer.h"
#include "Code/Sparkplug/spMeshNavigationSetSerializer.h"
#include "Code/Sparkplug/spMeshBVSerializer.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/Sparkplug/spResourceFATSerializer.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace
{
    using Bytes=std::vector<std::uint8_t>;int checks=0;
    void Check(bool value,const char* label){++checks;if(!value)throw std::runtime_error(label);}
    void Open(spMemoryStream& stream,const Bytes& bytes)
    {Check(stream.ResizeAndSetSize(static_cast<std::uint32_t>(bytes.size())),"bounded test stream");if(!bytes.empty())std::memcpy(stream.GetBuffer(),bytes.data(),bytes.size());}
    template<class T>void Add(Bytes& bytes,const T& value){const auto* p=reinterpret_cast<const std::uint8_t*>(&value);bytes.insert(bytes.end(),p,p+sizeof(value));}
    std::string Hex(const void* pointer,std::size_t size)
    {const auto* p=static_cast<const std::uint8_t*>(pointer);static const char digits[]="0123456789abcdef";std::string result;for(std::size_t i=0;i<size;++i){result+=digits[p[i]>>4];result+=digits[p[i]&15];}return result;}
    Bytes Parse(const std::string& text)
    {Bytes bytes;Check(text.size()%2==0,"hex extent");for(std::size_t i=0;i<text.size();i+=2)bytes.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i,2),nullptr,16)));return bytes;}
    template<class T>void Numbers(std::ostream& out,const T& values)
    {out<<'[';bool comma=false;for(auto value:values){if(comma)out<<',';comma=true;out<<static_cast<unsigned>(value);}out<<']';}
    void Matrix(std::ostream& out,const spNavigationSet::Table* table)
    {
        if(!table){out<<"null";return;}
        out<<"{\"rows\":"<<table->rows<<",\"columns\":"<<table->columns<<",\"stride\":"<<table->stride<<",\"packed\":\""<<Hex(table->packed.data(),table->packed.size())<<"\",\"values\":[";
        bool comma=false;for(std::uint32_t r=0;r<table->rows;++r)for(std::uint32_t c=0;c<table->columns;++c)
        {if(comma)out<<',';comma=true;out<<table->Get(r,c);}out<<"]}";
    }
    std::string State(const spBaseObject& object)
    {
        std::ostringstream out;
        if(const auto* graph=dynamic_cast<const spNavigationGraph*>(&object))
        {
            out<<"{\"paths\":[";bool rowComma=false;
            for(const auto& row:graph->GetPathsForAnalysis())
            {if(rowComma)out<<',';rowComma=true;out<<'[';bool cellComma=false;
                for(const auto& cell:row){if(cellComma)out<<',';cellComma=true;out<<'['<<unsigned(cell.nextPortal)<<",\""<<Hex(cell.alternatives.data(),cell.alternatives.size()*2)<<"\"]";}out<<']';}
            out<<"]}";
        }
        else if(const auto* portal=dynamic_cast<const spNavigationPortal*>(&object))
        {
            out<<"{\"first_nodes\":";Numbers(out,portal->GetFirstNodesForAnalysis());out<<",\"second_nodes\":";Numbers(out,portal->GetSecondNodesForAnalysis());
            Bytes paths;for(const auto& path:portal->GetPathsForAnalysis())paths.insert(paths.end(),path.begin(),path.end());
            out<<",\"paths\":";Numbers(out,paths);const std::uint8_t flags[]{portal->GetNavigationEnabledForAnalysis(),portal->GetGraphIndexForAnalysis()};
            out<<",\"enabled_index\":\""<<Hex(flags,2)<<"\"}";
        }
        else
        {
            const auto& set=dynamic_cast<const spNavigationSet&>(object);
            out<<"{\"node_count\":"<<set.GetNodeCountForAnalysis()<<",\"transitions\":";Matrix(out,set.GetTransitionsForAnalysis());out<<",\"portal_transitions\":";Matrix(out,set.GetPortalTransitionsForAnalysis());out<<",\"neighbours\":[";
            bool comma=false;for(const auto& row:set.GetNeighboursForAnalysis()){if(comma)out<<',';comma=true;Numbers(out,row);}out<<']';
            const std::uint8_t flags[]{set.GetGraphIndexForAnalysis(),set.GetNavigationEnabledForAnalysis()};out<<",\"index_enabled\":\""<<Hex(flags,2)<<"\"}";
        }
        return out.str();
    }
    std::unique_ptr<spBaseObject> Object(const std::string& kind)
    {if(kind=="graph")return std::make_unique<spNavigationGraph>();if(kind=="portal")return std::make_unique<spNavigationPortal>();return std::make_unique<spMeshNavigationSet>();}
    std::unique_ptr<spSerializer> Serializer(const std::string& kind)
    {if(kind=="graph")return std::make_unique<spNavigationGraphSerializer>();if(kind=="portal")return std::make_unique<spNavigationPortalSerializer>();return std::make_unique<spMeshNavigationSetSerializer>();}
    std::string Capture(const std::string& kind,const Bytes& bytes,bool reject=false)
    {
        auto object=Object(kind);auto serializer=Serializer(kind);
        spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);spMemoryStream input;Open(input,bytes);std::string error;
        const bool result=serializer->ReadPayloadForAnalysis(context,input,static_cast<std::uint32_t>(bytes.size()),*object,&error);
        Check(result!=reject,error.c_str());Check(context.failed==reject,"context failure matches result");
        return reject?error:State(*object);
    }
    std::string Links(const std::string& kind,const Bytes& bytes)
    {
        (void)spNavigationGraph::StaticRTTI();(void)spNavigationPortal::StaticRTTI();(void)spMeshNavigationSet::StaticRTTI();
        spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
        std::vector<std::pair<std::uint32_t,std::shared_ptr<spBaseObject>>> refs;
        if(kind=="graph")refs={{7,std::make_shared<spMeshNavigationSet>()},{9,std::make_shared<spMeshNavigationSet>()},{11,std::make_shared<spNavigationPortal>()}};
        else if(kind=="portal")refs={{7,std::make_shared<spNavigationGraph>()},{9,std::make_shared<spMeshNavigationSet>()},{11,std::make_shared<spMeshNavigationSet>()}};
        else refs={{7,std::make_shared<spNavigationPortal>()}};
        Bytes directory;Add(directory,static_cast<std::uint32_t>(refs.size()));
        for(const auto& pair:refs){Add(directory,pair.first);Add(directory,std::uint16_t(0));Add(directory,pair.second->vfunc_18().classID);Add(directory,0u);Add(directory,0u);}
        spMemoryStream index;Open(index,directory);auto* fat=manager.GetFATForAnalysis();Check(fat->LoadIndexForAnalysis(index),"actual typed source FAT");
        for(const auto& pair:refs){fat->FindByIDForAnalysis(pair.first)->object=pair.second.get();context.externalOwners.push_back(pair.second);}
        auto object=Object(kind);auto serializer=Serializer(kind);spMemoryStream input;Open(input,bytes);std::string error;
        if(!serializer->ReadPayloadForAnalysis(context,input,static_cast<std::uint32_t>(bytes.size()),*object,&error))throw std::runtime_error(error);
        const auto key=[&](const spBaseObject* target){if(!target)return 0u;if(target==object.get())return 1u;for(const auto& pair:refs)if(pair.second.get()==target)return pair.first;throw std::runtime_error("unknown test reference");};
        std::ostringstream out;out<<"{\"refs\":{";bool comma=false;
        for(const auto& pair:refs){if(comma)out<<',';comma=true;out<<'"'<<pair.first<<"\":1";Check(pair.second.use_count()==2,"navigation relations remain borrowed");}out<<'}';
        const auto ids=[&](const auto& list){std::vector<std::uint32_t> result;for(const auto* item:list)result.push_back(key(item));return result;};
        if(const auto* graph=dynamic_cast<spNavigationGraph*>(object.get()))
        {
            out<<",\"sets\":";Numbers(out,ids(graph->GetSetsForAnalysis()));out<<",\"portals\":";Numbers(out,ids(graph->GetPortalsForAnalysis()));
            out<<",\"set_ids\":["<<unsigned(dynamic_cast<spNavigationSet*>(refs[0].second.get())->GetGraphIndexForAnalysis())<<','<<unsigned(dynamic_cast<spNavigationSet*>(refs[1].second.get())->GetGraphIndexForAnalysis())<<']';
            const auto* portal=dynamic_cast<spNavigationPortal*>(refs[2].second.get());out<<",\"portal_id\":"<<unsigned(portal->GetGraphIndexForAnalysis())<<",\"portal_graph_untouched\":"<<(!portal->IsGraphKnownForAnalysis()?"true":"false");
        }
        else if(const auto* portal=dynamic_cast<spNavigationPortal*>(object.get()))
        {out<<",\"graph\":"<<key(portal->GetGraphForAnalysis())<<",\"sets\":";Numbers(out,ids(portal->GetEndpointsForAnalysis()));}
        else{out<<",\"portals\":";Numbers(out,ids(dynamic_cast<spNavigationSet*>(object.get())->GetPortalsForAnalysis()));}
        out<<'}';return out.str();
    }
    std::string MeshBinding(const Bytes& bytes)
    {
        auto mesh=std::make_shared<spMeshBV>();spMeshBVSerializer serializer;spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);spMemoryStream input;Open(input,bytes);std::string error;
        if(!serializer.ReadPayloadForAnalysis(context,input,static_cast<std::uint32_t>(bytes.size()),*mesh,&error))throw std::runtime_error(error);
        spMeshNavigationSet node;std::ostringstream out;out<<'[';
        for(int i=0;i<2;++i)
        {
            if(i){node.SetPositionForAnalysis({1,2,3});node.SetScaleForAnalysis({2,-3,4});out<<',';}
            node.MarkLocalTransformDirtyForAnalysis();Check(node.UpdateWorldForAnalysis()&&node.SetMeshForAnalysis(mesh),"actual MeshNavigationSet binding");
            out<<"{\"bounds\":\""<<Hex(node.GetMinimumForAnalysis().data(),12)<<Hex(node.GetMaximumForAnalysis().data(),12)<<"\",\"sphere\":\""<<Hex(node.GetNavigationSphereForAnalysis().data(),16)<<"\",\"mesh_borrowed\":true,\"embedded_primitive\":true,\"references\":"<<mesh.use_count()<<'}';
            Check(node.GetMeshForAnalysis()==mesh.get()&&node.GetCollisionForAnalysis().GetPrimitiveForAnalysis()==mesh.get(),"one actual embedded primitive owner");
        }
        out<<']';return out.str();
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==4&&std::string(argv[1])=="--capture"){std::cout<<Capture(argv[2],Parse(argv[3]))<<'\n';return 0;}
        if(argc==4&&std::string(argv[1])=="--links"){std::cout<<Links(argv[2],Parse(argv[3]))<<'\n';return 0;}
        if(argc==3&&std::string(argv[1])=="--mesh-binding"){std::cout<<MeshBinding(Parse(argv[2]))<<'\n';return 0;}
        for(const auto* kind:{"graph","portal","set"}){(void)Capture(kind,Bytes(std::string(kind)=="portal"?2:3));(void)Capture(kind,Bytes{0},true);}
        (void)Capture("graph",Parse("0000a2040000000000"),true);
        (void)Capture("graph",Parse("0000a20401000000a2040100000000"),true);
        (void)Capture("portal",Parse("00a20301020300"),true);
        (void)Capture("set",Parse("00a504010000000000"),true);
        std::cout<<"PASS "<<checks<<'/'<<checks<<": bounded navigation readers\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
