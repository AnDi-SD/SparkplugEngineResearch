#include "Code/Sparkplug/spRenderNodeSerializer.h"
#include "Code/Sparkplug/spRenderableSerializer.h"
#include "Code/Sparkplug/spModelSerializer.h"
#include "Code/Sparkplug/spRenderNode.h"
#include "Code/Sparkplug/spModel.h"
#include "Code/Sparkplug/spSkinSerializer.h"
#include "Code/Sparkplug/spSkin.h"
#include "Code/Sparkplug/spFog.h"
#include "Code/Sparkplug/spFogSerializer.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceFATSerializer.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/SparkBase/spMemoryStream.h"
#include "spSceneFileCapture.h"
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
using namespace sparkplug::reconstruction;
namespace
{
    using Bytes=std::vector<std::uint8_t>;int checks=0;
    void Check(bool ok,const char* text){++checks;if(!ok)throw std::runtime_error(text);}
    template<class T>void Add(Bytes& bytes,const T& value){const auto* p=reinterpret_cast<const std::uint8_t*>(&value);bytes.insert(bytes.end(),p,p+sizeof(value));}
    void Append(Bytes& bytes,const Bytes& value){bytes.insert(bytes.end(),value.begin(),value.end());}
    void Field(Bytes& bytes,std::uint8_t id,const Bytes& data){Check(!data.empty()&&data.size()<256,"tiny field bound");bytes.push_back(0xa0+id);bytes.push_back(static_cast<std::uint8_t>(data.size()));Append(bytes,data);}
    template<class T>void Field(Bytes& bytes,std::uint8_t id,const T& value){Bytes data;Add(data,value);Field(bytes,id,data);}
    std::string Hex(const Bytes& bytes){constexpr char d[]="0123456789abcdef";std::string out;for(auto v:bytes){out+=d[v>>4];out+=d[v&15];}return out;}
    void Open(spMemoryStream& stream,const Bytes& bytes={}){Check(stream.ResizeAndSetSize(static_cast<std::uint32_t>(bytes.size())),"fixture capacity");if(!bytes.empty())std::memcpy(stream.GetBuffer(),bytes.data(),bytes.size());Check(stream.Seek(spStream::SeekSource::essStart,0),"fixture rewind");}
    std::uint32_t Position(spStream& stream){std::uint32_t v=0;Check(stream.GetCurrentPosition(v),"tell");return v;}
    Bytes Data(spMemoryStream& stream){std::uint32_t size=0;Check(stream.GetSize(&size),"size");const auto* p=static_cast<const std::uint8_t*>(stream.GetBuffer());return size?Bytes(p,p+size):Bytes{};}
    Bytes ScalarInput(const std::string& kind,const std::string& mode)
    {
        Bytes data;
        if(kind=="render-node")
        {if(mode=="values")Field(data,0,spNode::Vector3{1,2,3});data.push_back(0);if(mode=="values")Field(data,9,std::uint8_t('X'));data.push_back(0);return data;}
        if(mode=="values"){Field(data,2,0xa500u);Field(data,3,0xdeadbeefu);}
        if(mode=="repeat"){Field(data,2,1u);Field(data,2,0u);Field(data,3,17u);Field(data,3,0xffffffffu);Field(data,9,std::uint8_t('X'));}
        if(mode=="null-links"){Field(data,0,0u);Field(data,1,0u);}
        if(mode=="failed-scalar")return {0xa2,4};
        data.push_back(0);
        if(kind=="model"){if(mode=="values")Field(data,1,0x89abcdefu);data.push_back(0);}
        return data;
    }
    std::string Scalar(const std::string& kind,const std::string& mode)
    {
        spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
        std::unique_ptr<spBaseObject> object;std::unique_ptr<spSerializer> serializer;
        if(kind=="render-node"){object=std::make_unique<spRenderNode>();serializer=std::make_unique<spRenderNodeSerializer>();}
        else {object=std::make_unique<spModel>();if(kind=="model")serializer=std::make_unique<spModelSerializer>();else serializer=std::make_unique<spRenderableSerializer>();}
        const auto bytes=ScalarInput(kind,mode);spMemoryStream input;Open(input,bytes);std::string error;
        const auto ok=serializer->ReadPayloadForAnalysis(context,input,static_cast<std::uint32_t>(bytes.size()),*object,&error);
        Check(ok==(mode!="failed-scalar")&&context.failed==!ok,error.c_str());Check(Position(input)==bytes.size(),"full source scalar extent");
        std::ostringstream state;
        if(auto* node=dynamic_cast<spRenderNode*>(object.get()))
        {Bytes world;Add(world,node->GetWorldPositionForAnalysis());Add(world,node->GetWorldScaleForAnalysis());Add(world,node->GetWorldOrientationForAnalysis());state<<'['<<node->GetFlagsForAnalysis()<<",\""<<Hex(world)<<"\","<<node->GetRenderableCountForAnalysis()*4<<']';}
        else
        {const auto& model=dynamic_cast<const spModel&>(*object);state<<'['<<model.IsAlphaSortEnabledForAnalysis()<<','<<model.GetPriorityForAnalysis()<<','<<model.GetProjectionGroupForAnalysis()<<",0,0,0]";Check(!model.GetMaterialForAnalysis()&&!model.GetFogForAnalysis()&&!model.GetBaseMeshForAnalysis(),"no invented empty-resource objects");}
        std::string output="null";
        if(ok){spMemoryStream stream;Open(stream);Check(serializer->WritePayloadForAnalysis(stream,*object,&error),error.c_str());output='"'+Hex(Data(stream))+'"';}
        std::ostringstream row;row<<"[\""<<kind<<"\",\""<<mode<<"\",\""<<Hex(bytes)<<"\","<<ok<<','<<Position(input)<<','<<state.str()<<','<<output<<']';return row.str();
    }
    std::string Graph(const std::string& mode)
    {
        Bytes body;Add(body,spModel::ClassID);Add(body,0x4f4f4253u);Field(body,2,0u);Field(body,3,17u);body.push_back(0);Field(body,1,9u);body.push_back(0);
        Bytes reference;Add(reference,7u);Add(reference,mode=="prebound"?0u:static_cast<std::uint32_t>(body.size()));if(mode!="prebound")Append(reference,body);
        Bytes payload{0};Field(payload,0,reference);if(mode=="repeat"){reference.clear();Add(reference,7u);Add(reference,0u);Field(payload,0,reference);}payload.push_back(0);
        Bytes directory;Add(directory,1u);Add(directory,7u);Add(directory,std::uint16_t(0));Add(directory,spModel::ClassID);Add(directory,0u);Add(directory,static_cast<std::uint32_t>(body.size()));
        spSerializerManager manager;spResourceManager resources;spMemoryStream index;Open(index,directory);
        auto* fat=manager.GetFATForAnalysis();Check(fat->LoadIndexForAnalysis(index),"Model FAT");
        Check(manager.RegisterForAnalysis(spRenderNode::ClassID,std::make_shared<spRenderNodeSerializer>(),0xff,3),"register RenderNode");
        Check(manager.RegisterForAnalysis(spModel::ClassID,std::make_shared<spModelSerializer>(),0xff,3),"register Model");
        manager.SetDispatchContextForAnalysis(2,1);spSerializerReadContextForAnalysis context(manager,resources);
        if(mode=="prebound"){auto model=std::make_shared<spModel>();fat->FindByIDForAnalysis(7)->object=model.get();context.externalOwners.push_back(model);}
        spRenderNode node;spRenderNodeSerializer serializer;spMemoryStream input;Open(input,payload);std::string error;
        Check(serializer.ReadPayloadForAnalysis(context,input,static_cast<std::uint32_t>(payload.size()),node,&error),error.c_str());
        const auto count=node.GetRenderableCountForAnalysis();Check(count==(mode=="repeat"?2u:1u),"native per-occurrence renderable aliases");
        auto* model=dynamic_cast<spModel*>(node.GetRenderableForAnalysis(0));Check(model!=nullptr,"real common Model factory/reader");
        if(count==2)Check(node.GetRenderableForAnalysis(1)==model,"one object retained twice, not cloned");
        Check(bool(context.ShareObjectForAnalysis(model)),"canonical owner resolves owning graph edge");
        Bytes spheres;Add(spheres,node.GetLocalBoundingSphereForAnalysis());Add(spheres,node.GetWorldBoundingSphereForAnalysis());
        std::ostringstream state;state<<'['<<model->IsAlphaSortEnabledForAnalysis()<<','<<model->GetPriorityForAnalysis()<<','<<model->GetProjectionGroupForAnalysis()<<','<<count<<",\""<<Hex(spheres)<<"\"]";
        // Native Read only publishes object; save must rebuild by-object index.
        fat->ClearResourceEntriesForAnalysis();manager.SetDispatchContextForAnalysis(2,2);
        Check(spSerializer::IndexReferenceForAnalysis(manager,&node),"recursive graph index");Check(fat->GetResourceCountForAnalysis()==2,"aliases index only once");
        spMemoryStream stream;Open(stream);Check(serializer.WritePayloadWithContextForAnalysis(manager,stream,node,&error),error.c_str());
        std::ostringstream row;row<<"[\""<<mode<<"\",\""<<Hex(payload)<<"\","<<state.str()<<",\""<<Hex(Data(stream))<<"\"]";return row.str();
    }
    Bytes FogState(const spFog& fog)
    {Bytes data;Add(data,static_cast<std::uint32_t>(fog.GetTypeForAnalysis()));Add(data,fog.GetColorARGBForAnalysis());Add(data,fog.GetStartForAnalysis());Add(data,fog.GetEndForAnalysis());Add(data,fog.GetDensityForAnalysis());return data;}
    std::string Fog(const std::string& mode)
    {
        Bytes bytes;std::array<std::uint32_t,5> values{3,0x12345678,0xc0000000,0x42f60000,0x3e800000};
        if(mode=="raw-bits")values={0xffffffff,0x01020304,0x80000000,0x7fc12345,0x7f800000};
        if(mode=="logo-field")values={0,0xff000000,0,0x447a0000,0};
        if(mode=="repeat"){Field(bytes,0,std::array<std::uint32_t,5>{});Field(bytes,9,std::uint8_t('X'));}
        if(mode!="empty")Field(bytes,0,values);bytes.push_back(0);
        spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
        spFog fog;spFogSerializer serializer;spMemoryStream input;Open(input,bytes);std::string error;
        Check(serializer.ReadPayloadForAnalysis(context,input,static_cast<std::uint32_t>(bytes.size()),fog,&error),error.c_str());
        Check(Position(input)==bytes.size()&&!context.failed,"Fog full bounded read");const auto state=FogState(fog);
        spMemoryStream output;Open(output);Check(serializer.WritePayloadForAnalysis(output,fog,&error),error.c_str());
        Check(state==FogState(fog),"Fog write preserves raw type/IEEE bits");
        std::ostringstream row;row<<"[\""<<mode<<"\",\""<<Hex(bytes)<<"\",1,"<<Position(input)<<",\""<<Hex(state)<<"\",\""<<Hex(Data(output))<<"\"]";return row.str();
    }
    std::string FogGraph(const std::string& mode)
    {
        Bytes body;Add(body,spFog::ClassID);Add(body,0x4f4f4253u);Field(body,0,std::array<std::uint32_t,5>{3,0x12345678,0xc0000000,0x42f60000,0x3e800000});body.push_back(0);
        Bytes reference;Add(reference,7u);Add(reference,mode=="prebound"?0u:static_cast<std::uint32_t>(body.size()));if(mode!="prebound")Append(reference,body);
        Bytes payload;Field(payload,1,reference);
        if(mode=="repeat"){reference.clear();Add(reference,7u);Add(reference,0u);Field(payload,1,reference);}
        if(mode=="clear")Field(payload,1,0u);payload.push_back(0);payload.push_back(0);
        const std::string name=mode=="named"?"fog-prefix":"";
        Bytes directory;Add(directory,1u);Add(directory,7u);Add(directory,static_cast<std::uint16_t>(name.size()));directory.insert(directory.end(),name.begin(),name.end());Add(directory,spFog::ClassID);Add(directory,0u);Add(directory,static_cast<std::uint32_t>(body.size()));
        spSerializerManager manager;spResourceManager resources;spMemoryStream index;Open(index,directory);auto* fat=manager.GetFATForAnalysis();
        Check(fat->LoadIndexForAnalysis(index),"Fog FAT");Check(manager.RegisterForAnalysis(spModel::ClassID,std::make_shared<spModelSerializer>(),0xff,3),"register Model/Fog consumer");
        Check(manager.RegisterForAnalysis(spFog::ClassID,std::make_shared<spFogSerializer>(),0xff,3),"register Fog codec");manager.SetDispatchContextForAnalysis(2,1);
        spSerializerReadContextForAnalysis context(manager,resources);
        if(mode=="prebound"){auto fog=std::make_shared<spFog>();fat->FindByIDForAnalysis(7)->object=fog.get();context.externalOwners.push_back(fog);}
        spModel model;spModelSerializer serializer;spMemoryStream input;Open(input,payload);std::string error;
        Check(serializer.ReadPayloadForAnalysis(context,input,static_cast<std::uint32_t>(payload.size()),model,&error),error.c_str());
        auto* created=fat->FindByIDForAnalysis(7)->object;Check(created&&bool(context.ShareObjectForAnalysis(created)),"common context owns actual Fog factory result");
        auto* active=dynamic_cast<spFog*>(model.GetFogForAnalysis().get());Check(bool(active)==(mode!="clear"),"native NULL/repeat owning-edge result");
        if(active)Check(active==created,"one canonical Fog pointer, not a clone");
        if(mode=="named")Check(active&&!active->GetName()&&!active->IsKindOf(spNamedObject::ClassID)&&name==fat->FindByIDForAnalysis(7)->GetNameForAnalysis(),"native common reference ignores FAT name for Fog: physical named prefix does not bypass engine RTTI guard");
        // On native clear, FAT retains a freed pointer. Host context ownership
        // deliberately keeps it alive until context teardown; do not emulate UAF.
        const auto state=active?'"'+Hex(FogState(*active))+'"':"null";
        fat->ClearResourceEntriesForAnalysis();manager.SetDispatchContextForAnalysis(2,2);Check(spSerializer::IndexReferenceForAnalysis(manager,&model),"recursive Model/Fog save index");
        Check(fat->GetResourceCountForAnalysis()==(active?2u:1u),"no detached Fog in save index");
        spMemoryStream output;Open(output);Check(serializer.WritePayloadWithContextForAnalysis(manager,output,model,&error),error.c_str());
        std::ostringstream row;row<<"[\""<<mode<<"\",\""<<Hex(payload)<<"\","<<state<<",\""<<Hex(Data(output))<<"\"]";return row.str();
    }
    void FogFailures()
    {
        for(int mode=0;mode<3;++mode)
        {
            Bytes data;if(mode==0)data={0xa0,20};
            if(mode==1){data={0xa0,20};Add(data,3u);Add(data,0x12345678u);Add(data,0xc0000000u);}
            if(mode==2){Field(data,0,std::array<std::uint32_t,4>{});data.push_back(0);}
            spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
            spFog fog;const auto before=FogState(fog);spMemoryStream input;Open(input,data);std::string error;
            Check(!spFogSerializer{}.ReadPayloadForAnalysis(context,input,static_cast<std::uint32_t>(data.size()),fog,&error)&&context.failed,"truncated/wrong Fog envelope rejected");
            Check(FogState(fog)==before,"strict source rejects before mutation, unlike native partial-word updates");
        }
    }
    void Failures()
    {
        for(int mode=0;mode<5;++mode)
        {
            spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
            Bytes bytes{0};if(mode==0)Field(bytes,0,0u);if(mode==1)bytes.push_back(0);if(mode==2){bytes.push_back(0);bytes.push_back(0);}
            if(mode==3){Field(bytes,1,Bytes{1,2});bytes.push_back(0);}if(mode==4){Field(bytes,0,0u);bytes.push_back(0);}
            spMemoryStream stream;Open(stream,bytes);std::string error;
            if(mode<3){spRenderNode node;Check(!spRenderNodeSerializer{}.ReadPayloadForAnalysis(context,stream,static_cast<std::uint32_t>(bytes.size()),node,&error)==(mode!=1),"derived missing/null/trailing checks");}
            else {spModel model;Check(!spModelSerializer{}.ReadPayloadForAnalysis(context,stream,static_cast<std::uint32_t>(bytes.size()),model,&error),"malformed group or explicit NULL mesh rejected");}
        }
        spSkin derived;spSkinSerializer serializer;spMemoryStream stream;Open(stream);std::string error;
        Check(serializer.WritePayloadForAnalysis(stream,derived,&error)&&Hex(Data(stream))=="6201000000630000000000e1040000000300000000e008000000040000000000000000","Skin adapter emits all three inherited sections");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--asset-file"){std::cout<<scene_file_test::Capture(argv[2])<<'\n';return 0;}
        if(argc==3&&std::string(argv[1])=="--asset-file-ids"){std::cout<<scene_file_test::Capture(argv[2],true)<<'\n';return 0;}
        if(argc==4&&std::string(argv[1])=="--capture"){std::cout<<Scalar(argv[2],argv[3])<<'\n';return 0;}
        if(argc==3&&std::string(argv[1])=="--graph"){std::cout<<Graph(argv[2])<<'\n';return 0;}
        if(argc==3&&std::string(argv[1])=="--fog"){std::cout<<Fog(argv[2])<<'\n';return 0;}
        if(argc==3&&std::string(argv[1])=="--fog-graph"){std::cout<<FogGraph(argv[2])<<'\n';return 0;}
        for(const auto* kind:{"render-node","renderable","model"})for(const auto* mode:{"empty","values","repeat","failed-scalar","null-links"})
            if(std::string(kind)!="render-node"||(std::string(mode)=="empty"||std::string(mode)=="values"))(void)Scalar(kind,mode);
        for(const auto* mode:{"inline","repeat","prebound"})(void)Graph(mode);
        for(const auto* mode:{"empty","values","repeat","raw-bits","logo-field"})(void)Fog(mode);
        for(const auto* mode:{"inline","repeat","clear","prebound","named"})(void)FogGraph(mode);
        FogFailures();Failures();std::cout<<"PASS "<<checks<<'/'<<checks<<": derived scene sections, actual common graph interfaces and bounds\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
