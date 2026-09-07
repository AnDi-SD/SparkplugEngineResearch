#include "Code/Sparkplug/spLightSerializer.h"
#include "Code/Sparkplug/spLightDataSerializer.h"
#include "Code/Sparkplug/spLightManager.h"
#include "Code/SparkplugDX/spDXLight.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/Sparkplug/spResourceFATSerializer.h"
#include "Code/SparkBase/spMemoryStream.h"
#include "Analysis/PC/spAnimationMath.h"
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace
{
    using Bytes=std::vector<std::uint8_t>;int checks=0;
    void Check(bool value,const char* text){++checks;if(!value)throw std::runtime_error(text);}
    std::uint32_t Bits(float value){std::uint32_t bits;std::memcpy(&bits,&value,4);return bits;}
    Bytes Unhex(const std::string& text)
    {Check(text.size()%2==0&&text.size()<4096,"bounded input hex");Bytes out;for(std::size_t i=0;i<text.size();i+=2)out.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i,2),nullptr,16)));return out;}
    std::string Hex(const Bytes& bytes)
    {const char* digits="0123456789abcdef";std::string out;for(auto v:bytes){out+=digits[v>>4];out+=digits[v&15];}return out;}
    void Open(spMemoryStream& stream,const Bytes& bytes={})
    {Check(stream.ResizeAndSetSize(static_cast<std::uint32_t>(bytes.size())),"buffer");if(!bytes.empty())std::memcpy(stream.GetBuffer(),bytes.data(),bytes.size());Check(stream.Seek(spStream::SeekSource::essStart,0),"rewind");}
    std::uint32_t Position(spStream& stream){std::uint32_t value=0;Check(stream.GetCurrentPosition(value),"tell");return value;}
    Bytes Data(spMemoryStream& stream)
    {std::uint32_t size=0;Check(stream.GetSize(&size),"size");const auto* p=static_cast<const std::uint8_t*>(stream.GetBuffer());return size?Bytes(p,p+size):Bytes{};}
    void Prepare(spLight& light)
    {Check(bool(light.GetFlagsForAnalysis()&8u),"native constructor dirty8");light.SetEnabledForAnalysis(false);light.SetAnimatedForAnalysis(false);light.SetInheritanceForAnalysis(false,false,false);light.SetOpaqueRuntimeFieldBitsForAnalysis(0xa1b2c3d4);Check(light.spLight::UpdateWorldForAnalysis(),"explicit prepared base clear");Check(light.GetFlagsForAnalysis()==0,"same prepared native flag word");}
    std::string NodeState(const spNode& node)
    {
        Bytes bytes;const auto append=[&](const auto& values){const auto* p=reinterpret_cast<const std::uint8_t*>(values.data());bytes.insert(bytes.end(),p,p+sizeof(values));};
        append(node.GetPositionForAnalysis());append(node.GetScaleForAnalysis());append(node.GetOrientationForAnalysis());
        append(node.GetWorldPositionForAnalysis());append(node.GetWorldScaleForAnalysis());append(node.GetWorldOrientationForAnalysis());return Hex(bytes);
    }
    std::string State(const spLight& light,bool node=false,bool omitOpaque=false)
    {
        std::ostringstream out;out<<'['<<static_cast<std::uint32_t>(light.GetTypeForAnalysis());
        for(float v:light.GetColorForAnalysis())out<<','<<Bits(v);
        out<<','<<light.ProjectsShadowVolumeForAnalysis()<<','<<light.UsesAttenuationForAnalysis()<<','<<light.IsLightEnabledForAnalysis();
        for(float v:{light.GetIntensityForAnalysis(),light.GetRangeForAnalysis(),light.GetHotspotAngleForAnalysis(),light.GetFalloffAngleForAnalysis()})out<<','<<Bits(v);
        if(!omitOpaque)out<<','<<light.GetOpaqueRuntimeFieldBitsForAnalysis();out<<','<<light.GetFlagsForAnalysis()<<']';
        if(!node)return out.str();
        return '['+out.str()+",\""+NodeState(light)+"\"]";
    }
    std::string Run(const std::string& kind,const std::string& mode,const Bytes& input)
    {
        spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
        spLightSerializer base;spLightDataSerializer data;spSerializer* serializer=kind=="data"?static_cast<spSerializer*>(&data):&base;
        data.defaultWhiteARGBForAnalysis=base.defaultWhiteARGBForAnalysis=mode=="global-white"?0xff804020u:0xffffffffu;
        std::unique_ptr<spBaseObject> owner;
        if(kind=="data")
        {
            spMemoryStream header;Bytes bytes=mode=="header-junk"?Bytes{'J','U','N','K','H','E','A','D'}:Bytes{0xdf,2,0x64,0x5e,'S','B','O','O'};
            if(mode=="header-eof")bytes.clear();Open(header,bytes);owner=data.ReadObjectHeaderAndCreateForAnalysis(header);
            Check(bool(owner)==(mode!="header-eof"),"Data header returns concrete DXLight or EOF failure");
        }
        else owner=std::make_unique<spDXLight>();
        std::ostringstream out;out<<"[\""<<kind<<"\",\""<<mode<<"\",\""<<Hex(input)<<"\",";
        if(!owner){out<<"0,0,[],null,[]]";return out.str();}
        auto* light=dynamic_cast<spDXLight*>(owner.get());Check(light!=nullptr,"actual source Data header selects DXLight");Prepare(*light);
        spMemoryStream stream;Open(stream,input);std::string error;
        const bool ok=serializer->ReadPayloadForAnalysis(context,stream,static_cast<std::uint32_t>(input.size()),*light,&error);
        Check(ok==(mode!="failed-color")&&context.failed==!ok,"read result/poison");
        const bool node=mode.rfind("corpus-",0)==0;
        const auto state=State(*light,node);out<<ok<<','<<Position(stream)<<','<<state<<',';
        if(!ok){Check(static_cast<std::uint32_t>(light->GetTypeForAnalysis())==1,"prior field survives host envelope failure");out<<"null,[]]";return out.str();}
        spMemoryStream written;Open(written);Check(serializer->WritePayloadForAnalysis(written,*light,&error),error.c_str());
        Check(state==State(*light,node),"writer preserves all represented state");const auto bytes=Data(written);out<<'"'<<Hex(bytes)<<"\",";
        spDXLight next;Prepare(next);spSerializerReadContextForAnalysis again(manager,resources);Check(written.Seek(spStream::SeekSource::essStart,0),"rewind output");
        Check(serializer->ReadPayloadForAnalysis(again,written,static_cast<std::uint32_t>(bytes.size()),next,&error),error.c_str());
        if(mode=="disabled")Check(!light->IsLightEnabledForAnalysis()&&next.IsLightEnabledForAnalysis(),"original false-enabled omission is intentionally not repaired");
        if(mode=="raw-bits")Check(Bits(next.GetHotspotAngleForAnalysis())==0&&Bits(light->GetHotspotAngleForAnalysis())==0x80000000u,"negative zero angle omission");
        out<<State(next,node)<<']';return out.str();
    }
    void Guards()
    {
        {
            spLightDataSerializer serializer;spResourceFATHelperForAnalysis fat;spMemoryStream directory;
            Open(directory,Unhex("01000000010000000000df02645e000000000a000000"));
            Check(fat.LoadIndexForAnalysis(directory),
                "wire LightData FAT entry remains loadable when its serializer creates runtime DXLight");
        }
        for(const auto* text:{"00a00401000000","00a101","000000"})
        {
            spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
            spDXLight light;spMemoryStream stream;const auto bytes=Unhex(text);Open(stream,bytes);std::string error;
            Check(!spLightDataSerializer{}.ReadPayloadForAnalysis(context,stream,static_cast<std::uint32_t>(bytes.size()),light,&error)&&context.failed&&!error.empty(),"strict malformed host guard");
        }
    }
    std::string Matrix(const Bytes& bytes)
    {
        Check(bytes.size()==36,"nine input words");sparkplug::evidence::pc::animation_math::Matrix3 matrix;
        std::memcpy(matrix.data(),bytes.data(),bytes.size());const auto q=sparkplug::evidence::pc::animation_math::FromMatrix(matrix);
        std::ostringstream out;out<<'[';for(std::size_t i=0;i<4;++i){if(i)out<<',';out<<Bits(q[i]);}out<<']';return out.str();
    }
    std::string GraphRoundtrip(const std::string& item,const Bytes& bytes,const Bytes& directory)
    {
        spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis initial(manager,resources);
        auto serializer=std::make_shared<spLightDataSerializer>();auto nodeSerializer=std::make_shared<spNodeSerializer>();
        Check(manager.RegisterForAnalysis(spDXLight::ClassID,serializer,0xff,3)&&manager.RegisterForAnalysis(spNode::ClassID,nodeSerializer,0xff,3),"explicit runtime DXLight and Node dispatch");
        manager.SetDispatchContextForAnalysis(2,2);spMemoryStream input;Open(input,bytes);auto owner=serializer->ReadObjectHeaderAndCreateForAnalysis(input);
        auto* light=dynamic_cast<spDXLight*>(owner.get());Check(light!=nullptr,"same decoded light root");std::string error;
        Check(serializer->ReadPayloadForAnalysis(initial,input,static_cast<unsigned>(bytes.size()-8),*light,&error),error.c_str());
        auto childOwner=std::make_shared<spNode>();auto* child=childOwner.get();child->SetPositionForAnalysis({1,2,3});child->MarkLocalTransformDirtyForAnalysis();
        Check(light->AttachChildForAnalysis(std::move(childOwner)),"decoded light owns new child");
        Check(spSerializer::IndexReferenceForAnalysis(manager,light),"whole graph relationship indexing");auto* fat=manager.GetFATForAnalysis();Check(fat->GetNextResourceIDForAnalysis()==3,"DFS light and child IDs");
        spMemoryStream written;Open(written);Check(spSerializer::WriteReferenceForAnalysis(manager,written,light,&error),error.c_str());
        std::ostringstream metadata;metadata<<'[';
        for(unsigned i=0;i<2;++i){const auto* entry=fat->FindByObjectForAnalysis(i?static_cast<spBaseObject&>(*child):static_cast<spBaseObject&>(*light));Check(entry!=nullptr,"actual saved FAT entry");if(i)metadata<<',';metadata<<'['<<entry->id<<','<<entry->classID<<','<<entry->offset<<','<<entry->size<<','<<entry->payloadWritten<<']';}metadata<<']';
        Check(spSerializer::WriteReferenceForAnalysis(manager,written,light,&error)&&spSerializer::WriteReferenceForAnalysis(manager,written,nullptr,&error),"repeat and null reference writes");
        const auto output=Data(written);
        const auto graph=[](const spLight& root)
        {std::ostringstream out;out<<'['<<State(root,true,true)<<",[";for(std::size_t i=0;i<root.GetChildCountForAnalysis();++i){if(i)out<<',';out<<'"'<<NodeState(*root.GetChildForAnalysis(i))<<'"';}out<<"]]";return out.str();};
        const auto before=graph(*light);fat->ClearResourceEntriesForAnalysis();spMemoryStream index;Open(index,directory);Check(fat->LoadIndexForAnalysis(index),"declared saved-entry directory input");
        manager.SetDispatchContextForAnalysis(2,1);spSerializerReadContextForAnalysis freshContext(manager,resources);spMemoryStream source;Open(source,output);
        auto* fresh=dynamic_cast<spDXLight*>(spSerializer::ReadReferenceForAnalysis(freshContext,spNode::ClassID,source,source,&error));Check(fresh&&fresh!=light,"fresh full reference/header/light/child read");
        const auto after=graph(*fresh);Check(fresh->GetChildCountForAnalysis()==1,"fresh owning child graph");
        Check(spSerializer::ReadReferenceForAnalysis(freshContext,spNode::ClassID,source,source,&error)==fresh,"repeated reference canonical identity");
        Check(spSerializer::ReadReferenceForAnalysis(freshContext,spNode::ClassID,source,source,&error)==nullptr&&!freshContext.failed&&Position(source)==output.size(),"null and exact final stream position");
        return "[\""+item+"\",\""+Hex(bytes)+"\",\""+Hex(output)+"\","+metadata.str()+",\""+Hex(directory)+"\","+before+','+after+']';
    }
    std::string SceneWorld(const std::string& mode,const Bytes& bytes)
    {
        spSerializerManager serializers;spResourceManager resources;spSerializerReadContextForAnalysis context(serializers,resources);
        spMemoryStream stream;Open(stream,bytes);spLightDataSerializer serializer;auto owner=serializer.ReadObjectHeaderAndCreateForAnalysis(stream);
        auto light=std::dynamic_pointer_cast<spDXLight>(std::shared_ptr<spBaseObject>(std::move(owner)));Check(bool(light),"actual DXLight header");
        light->SetWorldDeviceInputsForAnalysis({.125F,.25F,.5F},0x7f234567);light->SetOpaqueRuntimeFieldBitsForAnalysis(0xa1b2c3d4);
        std::string error;Check(serializer.ReadPayloadForAnalysis(context,stream,static_cast<std::uint32_t>(bytes.size()-8),*light,&error),error.c_str());
        spLightManager manager;spLightManager::CacheForAnalysis caches[2];spLightManager::SphereForAnalysis spheres[2]={{0,0,0,2},{100,0,0,2}};
        for(unsigned i=0;i<2;++i)Check(manager.RegisterRenderTargetForAnalysis(caches[i],spheres[i],i==0),"borrowed actual render target view");
        spNode root;root.SetHierarchyActiveForAnalysis(true);Check(root.AttachChildForAnalysis(light),"actual source root owns decoded light");
        Check(manager.RegisterLightForAnalysis(*light),"explicit scene light membership");light->SetSceneLightManagerForAnalysis(&manager);
        if(mode=="shadow")light->SetProjectsShadowVolumeForAnalysis(true);
        std::ostringstream out;out<<"[\""<<mode<<"\",\""<<Hex(bytes)<<"\",[";
        const auto capture=[&](const char* label)
        {
            out<<"[\""<<label<<"\","<<State(*light,true)<<",[";
            const auto& payload=light->GetDevicePayloadForAnalysis();for(std::size_t i=0;i<payload.size();++i){if(i)out<<',';out<<payload[i].value_or(0xcccccccc);}
            out<<"],[";for(unsigned i=0;i<2;++i){if(i)out<<',';out<<'[';for(auto* value:caches[i].GetRawSlots())out<<(value==light.get()?7:0)<<',';out<<(caches[i].GetAmbient()==light.get()?7:0)<<','<<caches[i].GetCount()<<']';}out<<"]]";
        };
        capture("attached");
        for(const auto* label:{"world","far-position","dirty","disabled","enabled","inactive","active","sphere-change","inherited"})
        {
            const std::string step=label;
            if(step=="far-position"){light->SetPositionForAnalysis({1000,0,0});light->MarkLocalTransformDirtyForAnalysis();}
            if(step=="dirty")light->MarkLightDataDirtyForAnalysis();
            if(step=="disabled"||step=="enabled"){light->SetLightEnabledForAnalysis(step=="enabled");light->MarkLightDataDirtyForAnalysis();}
            if(step=="inactive"||step=="active"){light->SetHierarchyActiveForAnalysis(step=="active");light->MarkLightDataDirtyForAnalysis();}
            if(step=="sphere-change"){spheres[1]={1000,0,0,2};light->MarkLightDataDirtyForAnalysis();}
            Check(light->UpdateWorldForAnalysis(step=="inherited"?8u:0u),"whole source world drives both scene cache targets");
            out<<',';capture(label);
        }
        light->SetSceneLightManagerForAnalysis(nullptr);Check(manager.UnregisterLightForAnalysis(*light),"borrowed scene light unlink");
        for(auto& cache:caches)Check(manager.UnregisterRenderTargetForAnalysis(cache),"borrowed render target unlink");
        out<<"]]";return out.str();
    }
    std::string World(const std::string& mode,const Bytes& bytes)
    {
        spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
        spMemoryStream stream;Open(stream,bytes);spLightDataSerializer serializer;auto owner=serializer.ReadObjectHeaderAndCreateForAnalysis(stream);
        auto light=std::dynamic_pointer_cast<spDXLight>(std::shared_ptr<spBaseObject>(std::move(owner)));Check(bool(light),"actual DXLight header");
        light->SetWorldDeviceInputsForAnalysis({.125F,.25F,.5F},0x7f234567);light->SetOpaqueRuntimeFieldBitsForAnalysis(0xa1b2c3d4);
        std::string error;Check(serializer.ReadPayloadForAnalysis(context,stream,static_cast<std::uint32_t>(bytes.size()-8),*light,&error),error.c_str());
        std::ostringstream out;out<<"[\""<<mode<<"\",\""<<Hex(bytes)<<"\",[";
        const auto capture=[&](const char* label)
        {
            out<<"[\""<<label<<"\","<<State(*light,true)<<",[";
            const auto& payload=light->GetDevicePayloadForAnalysis();for(std::size_t i=0;i<payload.size();++i){if(i)out<<',';out<<payload[i].value_or(0xcccccccc);}
            out<<"]]";
        };
        capture("read");std::unique_ptr<spNode> parent;
        if(mode=="parent"){parent=std::make_unique<spNode>();parent->SetPositionForAnalysis({10,-20,30});parent->MarkLocalTransformDirtyForAnalysis();Check(parent->AttachChildForAnalysis(light),"same source parent owns decoded light");}
        for(const auto* label:{"world","raw-change","dirty","inherited","disabled-position"})
        {
            const std::string step=label;const auto previous=light->GetDevicePayloadForAnalysis();
            if(step=="raw-change")light->SetIntensityForAnalysis(3);
            if(step=="dirty")light->MarkLightDataDirtyForAnalysis();
            if(step=="disabled-position"){light->SetLightEnabledForAnalysis(false);light->SetPositionForAnalysis({7,8,9});light->MarkLocalTransformDirtyForAnalysis();}
            const bool ok=parent&&step=="world"?parent->UpdateWorldForAnalysis(1):light->UpdateWorldForAnalysis(step=="inherited"?8u:0u);
            Check(ok,"actual source complete light world chain");Check(!(light->GetFlagsForAnalysis()&15u),"complete dirty bits clear");
            if(step=="raw-change"||step=="disabled-position"||mode=="disabled")Check(previous==light->GetDevicePayloadForAnalysis(),"suppressed refresh retains old payload");
            out<<',';capture(label);
        }
        if(parent)Check(light->GetWorldPositionForAnalysis()==spNode::Vector3{17,-12,39},"same parent-local world inheritance");
        out<<"]]";return out.str();
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==5&&std::string(argv[1])=="--case"){std::cout<<Run(argv[2],argv[3],Unhex(argv[4]))<<'\n';return 0;}
        if(argc==3&&std::string(argv[1])=="--matrix"){std::cout<<Matrix(Unhex(argv[2]))<<'\n';return 0;}
        if(argc==4&&std::string(argv[1])=="--world"){std::cout<<World(argv[2],Unhex(argv[3]))<<'\n';return 0;}
        if(argc==4&&std::string(argv[1])=="--scene-world"){std::cout<<SceneWorld(argv[2],Unhex(argv[3]))<<'\n';return 0;}
        if(argc==5&&std::string(argv[1])=="--graph"){std::cout<<GraphRoundtrip(argv[2],Unhex(argv[3]),Unhex(argv[4]))<<'\n';return 0;}
        for(const auto* kind:{"data","base"})
        {
            (void)Run(kind,"default",{0,0});(void)Run(kind,"disabled",Unhex("00a8010000"));
            (void)Run(kind,"values",Unhex("00a00402000000a10101a20410204080a30101a40400002040a5040000f642a6040000003fa7040000803fa8010100"));
            (void)Run(kind,"raw-bits",Unhex("00a004ffffffffa4044523c17fa504000080ffa60400000080a7042143c57f00"));
            (void)Run(kind,"failed-color",Unhex("00a00401000000a204"));
        }
        Guards();std::cout<<"PASS "<<checks<<'/'<<checks<<": Light serialization\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
