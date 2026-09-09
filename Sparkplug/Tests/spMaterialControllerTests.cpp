#include "Code/Sparkplug/spAnimTexController.h"
#include "Code/Sparkplug/spAnimTexControllerSerializer.h"
#include "Code/Sparkplug/spAnimationManager.h"
#include "Code/Sparkplug/spMaterialTexture.h"
#include "Code/Sparkplug/spMaterialData.h"
#include "Code/Sparkplug/spMaterialDataSerializer.h"
#include "Code/Sparkplug/spMaterialPassLayer.h"
#include "Code/Sparkplug/spStdLayer.h"
#include "Code/SparkplugDX/spDXTexture.h"
#include "Code/SparkplugDX/spDXTextureSerializer.h"
#include "Code/Sparkplug/spTextureData.h"
#include "Code/Sparkplug/spTextureDataSerializer.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/Sparkplug/spResourceFATSerializer.h"
#include "Code/SparkBase/spMemoryStream.h"
#include "Analysis/PC/spMaterialControllerAbi.h"
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace
{
    using Bytes=std::vector<std::uint8_t>;int checks=0;
    void Check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
    template<class T>void Add(Bytes& out,const T& value){const auto* p=reinterpret_cast<const std::uint8_t*>(&value);out.insert(out.end(),p,p+sizeof(value));}
    void Field(Bytes& out,std::uint8_t id,const Bytes& bytes){Check(bytes.size()<256&&!bytes.empty(),"tiny field");out.push_back(0xa0+id);out.push_back(static_cast<std::uint8_t>(bytes.size()));out.insert(out.end(),bytes.begin(),bytes.end());}
    template<class T>void Field(Bytes& out,std::uint8_t id,const T& value){Bytes bytes;Add(bytes,value);Field(out,id,bytes);}
    void Open(spMemoryStream& stream,const Bytes& bytes={}){Check(stream.ResizeAndSetSize(static_cast<std::uint32_t>(bytes.size())),"capacity");if(!bytes.empty())std::memcpy(stream.GetBuffer(),bytes.data(),bytes.size());Check(stream.Seek(spStream::SeekSource::essStart,0),"rewind");}
    Bytes Data(spMemoryStream& stream){std::uint32_t size=0;Check(stream.GetSize(&size),"size");const auto* p=static_cast<const std::uint8_t*>(stream.GetBuffer());return size?Bytes(p,p+size):Bytes{};}
    std::string Hex(const Bytes& bytes){constexpr char d[]="0123456789abcdef";std::string result;for(auto b:bytes){result+=d[b>>4];result+=d[b&15];}return result;}
    template<class T>void Array(std::ostream& out,const std::vector<T>& values){out<<'[';for(std::size_t i=0;i<values.size();++i){if(i)out<<',';out<<values[i];}out<<']';}
    class FileReadContract final:public spStream
    {
    public:
        spMemoryStream storage;
        bool Open(const char*) override{return false;}
        bool Open(std::uint32_t,const char*) override{return false;}
        bool Close() override{return false;}
        bool Seek(SeekSource source,std::int32_t offset) override{return storage.Seek(source,offset);}
        bool GetCurrentPosition(std::uint32_t& position) const override{return storage.GetCurrentPosition(position);}
        bool GetSize(std::uint32_t* size) const override{return storage.GetSize(size);}
        bool ReadData(void* target,std::uint32_t size) override{return size&&storage.ReadData(target,size);}
        bool WriteData(const void*,std::uint32_t) override{return false;}
        bool vfunc_WriteFromStream(spStream*,std::uint32_t) override{return false;}
    };
    std::string Track(const std::string& mode)
    {
        const std::vector<float> times=mode=="empty"?std::vector<float>{}:mode=="duplicate"?std::vector<float>{1,1,3}:std::vector<float>{1,2,3};
        const std::vector<std::uint32_t> ids=mode=="empty"?std::vector<std::uint32_t>{}:mode=="alias"?std::vector<std::uint32_t>{7,7,7}:mode=="null"?std::vector<std::uint32_t>{7,0,9}:std::vector<std::uint32_t>{7,8,9};
        Bytes track;Add(track,static_cast<std::uint32_t>(times.size()));for(auto t:times)Add(track,t);
        for(auto id:ids){Add(track,id);if(id)Add(track,0u);}Bytes input;Field(input,0,track);input.push_back(0);
        spAnimationManager animationManager;spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
        auto textureSerializer=std::make_shared<spTextureDataSerializer>();auto serializer=std::make_shared<spAnimTexControllerSerializer>();
        Check(manager.RegisterForAnalysis(spTextureData::ClassID,textureSerializer,0xff,3),"CPU texture registry");
        Check(manager.RegisterForAnalysis(spAnimTexController::ClassID,serializer,0xff,3),"controller registry");manager.SetDispatchContextForAnalysis(2,1);
        std::map<std::uint32_t,std::shared_ptr<spTextureData>> textures;
        for(auto id:ids)if(id&&!textures.count(id))textures[id]=std::make_shared<spTextureData>();
        // Host directory loader provides the same prebound lookup input as
        // native staged466FA0 entries; this does not compare directory loading.
        Bytes directory;Add(directory,static_cast<std::uint32_t>(textures.size()));
        for(auto& [id,texture]:textures){Add(directory,id);Add(directory,std::uint16_t(0));Add(directory,spTextureData::ClassID);Add(directory,0u);Add(directory,0u);}
        spMemoryStream index;Open(index,directory);auto* fat=manager.GetFATForAnalysis();Check(fat->LoadIndexForAnalysis(index),"prebound host directory");
        std::string error;
        for(auto& [id,texture]:textures)
        {
            Bytes raw;Add(raw,2u);Add(raw,1u);Add(raw,0u);Add(raw,4u);raw.insert(raw.end(),8,static_cast<std::uint8_t>(id));
            Bytes nested;Field(nested,5,raw);nested.push_back(0);Bytes data;Field(data,2,std::uint8_t(0));data.push_back(0);Field(data,6,1u);Field(data,0,nested);data.push_back(0);
            spMemoryStream source;Open(source,data);Check(textureSerializer->ReadPayloadForAnalysis(context,source,static_cast<std::uint32_t>(data.size()),*texture,&error),error.c_str());
            fat->FindByIDForAnalysis(id)->object=texture.get();context.externalOwners.push_back(texture);
        }
        auto controller=std::make_shared<spAnimTexController>();FileReadContract source;Open(source.storage,input);
        const bool read=serializer->ReadPayloadForAnalysis(context,source,static_cast<std::uint32_t>(input.size()),*controller,&error);
        Check(read==(mode!="empty"),error.empty()?"FileStream zero-byte read result matches native":error.c_str());
        Check(controller->GetTextureTrackForAnalysis().GetTimesForAnalysis()==times,"stored exact times");
        spAnimTexController partial;spAnimTexControllerSerializer::InspectionForAnalysis observation;
        spMemoryStream inspectedInput;Open(inspectedInput,input);
        if(!serializer->InspectPayloadForAnalysis(inspectedInput,static_cast<std::uint32_t>(input.size()),partial,observation,&error))throw std::runtime_error(error);
        Check(observation.hasTrack&&partial.GetTextureTrackForAnalysis().GetTimesForAnalysis()==times,"memory-backed inspection preserves original time array including empty");
        Check(observation.textures.size()==ids.size(),"inspected reference count");
        for(std::size_t i=0;i<ids.size();++i)
            Check(observation.textures[i].id==ids[i]&&!partial.GetTextureTrackForAnalysis().GetTexturesForAnalysis()[i],"inspected ID without substitute Texture object");
        spMaterialTexture holder;holder.SetOwnedAnimTextureControllerForAnalysis(controller);std::ostringstream states;states<<std::setprecision(17)<<'[';
        if(!times.empty())
        {
            const std::vector<float> steps=mode=="negative"?std::vector<float>{-1,.5,.5,1,1,1,3,6.5}:mode=="boundary"?std::vector<float>{0,.999F,.001F,.999F,.001F,1,.5,3,6}:std::vector<float>{0,.5,.5,1,1,.5,3,6};
            for(std::size_t step=0;step<steps.size();++step)
            {
                controller->ApplyForAnalysis(steps[step]);Check(holder.UpdateTextureAnimationForAnalysis(),"actual positive track update");
                std::uint32_t selected=0;for(const auto& [id,texture]:textures)if(holder.GetTextureForAnalysis()==texture.get())selected=id;
                std::optional<std::size_t> key;
                Check(partial.GetTextureTrackForAnalysis().SelectKeyIndexForAnalysis(controller->GetPlaybackTimeForAnalysis(),key)
                    &&key&&ids[*key]==selected,"same key-selection algorithm for original resolved runtime and inspected timeline");
                if(step)states<<',';states<<'['<<steps[step]<<','<<controller->GetAppliedTimeForAnalysis()<<','<<controller->GetAccumulatedTimeForAnalysis()<<','<<controller->GetPlaybackTimeForAnalysis()<<','<<selected<<']';
            }
        }
        states<<']';fat->ClearResourceEntriesForAnalysis();manager.SetDispatchContextForAnalysis(2,2);
        Check(spSerializer::IndexReferenceForAnalysis(manager,controller.get()),"recursive native-order controller indexing");
        spMemoryStream output;Open(output);Check(serializer->WritePayloadWithContextForAnalysis(manager,output,*controller,&error),"controller/common texture writer");
        std::ostringstream row;row<<std::setprecision(17)<<"[\""<<mode<<"\",\""<<Hex(input)<<"\",";Array(row,times);row<<',';Array(row,ids);row<<','<<states.str()<<",\""<<Hex(Data(output))<<"\"]";return row.str();
    }
    std::string MaterialGraph(const std::string& mode)
    {
        Bytes texture;Add(texture,spDXTexture::ClassID);Add(texture,0x4f4f4253u);Add(texture,1u);Add(texture,1u);Add(texture,3u);Add(texture,std::uint8_t(0));Add(texture,1u);Add(texture,0x04030201u);
        Bytes frames;Add(frames,3u);Add(frames,std::array<float,3>{1,2,3});Add(frames,9u);Add(frames,static_cast<std::uint32_t>(texture.size()));frames.insert(frames.end(),texture.begin(),texture.end());Add(frames,std::array<std::uint32_t,4>{9,0,9,0});
        Bytes controllerBytes;Add(controllerBytes,spAnimTexController::ClassID);Add(controllerBytes,0x4f4f4253u);Field(controllerBytes,0,frames);controllerBytes.push_back(0);
        Bytes ref;Add(ref,7u);Add(ref,static_cast<std::uint32_t>(controllerBytes.size()));ref.insert(ref.end(),controllerBytes.begin(),controllerBytes.end());
        Bytes input;Field(input,3,2u);Field(input,4,spStdLayer::ClassID);Field(input,11,ref);
        if(mode=="repeat"||mode=="two-layers")
        {if(mode=="two-layers")Field(input,4,spStdLayer::ClassID);ref.clear();Add(ref,7u);Add(ref,0u);Field(input,11,ref);}
        if(mode=="null-after")Field(input,11,0u);input.push_back(0);
        Bytes directory;Add(directory,2u);
        Add(directory,7u);Add(directory,std::uint16_t(0));Add(directory,spAnimTexController::ClassID);Add(directory,0u);Add(directory,static_cast<std::uint32_t>(controllerBytes.size()));
        Add(directory,9u);Add(directory,std::uint16_t(0));Add(directory,spDXTexture::ClassID);Add(directory,0u);Add(directory,static_cast<std::uint32_t>(texture.size()));
        spAnimationManager animations;spSerializerManager manager;spResourceManager resources;
        Check(manager.RegisterForAnalysis(spMaterialData::ClassID,std::make_shared<spMaterialDataSerializer>(),0xff,3),"material serializer");
        Check(manager.RegisterForAnalysis(spAnimTexController::ClassID,std::make_shared<spAnimTexControllerSerializer>(),0xff,3),"animation serializer");
        Check(manager.RegisterForAnalysis(spDXTexture::ClassID,std::make_shared<spDXTextureSerializer>(),0xff,3),"runtime texture serializer");
        manager.SetDispatchContextForAnalysis(2,1);auto* fat=manager.GetFATForAnalysis();spMemoryStream index;Open(index,directory);Check(fat->LoadIndexForAnalysis(index),"controller directory");
        spSerializerReadContextForAnalysis context(manager,resources);context.pcTexturePitchForAnalysis=[](void*,std::uint32_t,std::uint32_t row)noexcept{return row+4;};
        spMaterialData material;spMaterialDataSerializer serializer;spMemoryStream source;Open(source,input);std::string error;
        Check(serializer.ReadPayloadForAnalysis(context,source,static_cast<std::uint32_t>(input.size()),material,&error),error.c_str());
        auto* controller=dynamic_cast<spAnimTexController*>(fat->FindByIDForAnalysis(7)->object);auto* tex=dynamic_cast<spDXTexture*>(fat->FindByIDForAnalysis(9)->object);
        Check(controller&&tex&&controller->GetTextureTrackForAnalysis().GetTexturesForAnalysis().size()==3,"actual controller and runtime leaf with three aliases");
        auto* pass=dynamic_cast<spMaterialPassLayer*>(material.GetPassForAnalysis(0));Check(pass&&pass->GetLayerCountForAnalysis()==(mode=="two-layers"?2u:1u),"expected pass/layers");
        for(std::size_t i=0;i<pass->GetLayerCountForAnalysis();++i)
            Check(pass->GetLayerForAnalysis(i)->GetMaterialTextureForAnalysis()->GetAnimTextureControllerForAnalysis()==controller,"same canonical controller including NULL preserve");
        controller->ApplyForAnalysis(1);Check(pass->GetLayerForAnalysis(0)->GetMaterialTextureForAnalysis()->UpdateTextureAnimationForAnalysis(),"first layer render drives shared controller");
        std::ostringstream state;state<<'[';
        for(std::size_t i=0;i<pass->GetLayerCountForAnalysis();++i)
        {
            const bool active=pass->GetLayerForAnalysis(i)->GetMaterialTextureForAnalysis()->GetTextureForAnalysis()==tex;
            Check(active==(i+1==pass->GetLayerCountForAnalysis()),"only last-bound holder receives fallback");if(i)state<<',';state<<(active?"true":"false");
        }
        state<<']';fat->ClearResourceEntriesForAnalysis();manager.SetDispatchContextForAnalysis(2,2);
        Check(spSerializer::IndexReferenceForAnalysis(manager,&material),"whole material/controller/texture graph indexing");
        Check(fat->GetResourceCountForAnalysis()==3,"shared texture never duplicated by fallback/frame graph");
        spMemoryStream output;Open(output);Check(serializer.WritePayloadWithContextForAnalysis(manager,output,material,&error),error.c_str());
        std::ostringstream row;row<<"[\""<<mode<<"\",\""<<Hex(input)<<"\","<<state.str()<<",\""<<Hex(Data(output))<<"\"]";return row.str();
    }
    class RenderClockFixture final:public spRenderController
    {
    public:
        float consumed=0;
        bool UpdateForRenderForAnalysis() override{consumed=ConsumeElapsedForAnalysis();return true;}
        std::unique_ptr<spBaseObject> vfunc_10(spCloneManager& manager) const override
        {auto result=std::make_unique<RenderClockFixture>();return spRenderController::vfunc_14(*result,manager)?std::move(result):nullptr;}
    };
    void Guards()
    {
        RenderClockFixture clock;for(float delta:{.5F,.25F,-1.F,2.F})clock.ApplyForAnalysis(delta);
        Check(clock.GetAppliedTimeForAnalysis()==0&&clock.GetAccumulatedTimeForAnalysis()==1.75F,"manager accumulates separately from render consumer");
        Check(clock.UpdateForRenderForAnalysis()&&clock.consumed==1.75F&&clock.GetAppliedTimeForAnalysis()==1.75F,"consume returns delta and updates previous time");
        clock.ApplyForAnalysis(.5F);clock.SetEnabledForAnalysis(false);auto copyOwner=clock.Clone();const auto* copy=dynamic_cast<RenderClockFixture*>(copyOwner.get());
        Check(copy&&copy->GetAppliedTimeForAnalysis()==1.75F&&copy->GetAccumulatedTimeForAnalysis()==2.25F&&!copy->IsEnabledForAnalysis(),"actual base copy includes enabled and both clocks, not manager links");
        auto controller=std::make_shared<spAnimTexController>();spMaterialTexture holder;
        Check(!controller->UpdateForRenderForAnalysis(),"unbound safe refusal");holder.SetOwnedAnimTextureControllerForAnalysis(controller);
        Check(!holder.UpdateTextureAnimationForAnalysis(),"empty/zero duration cannot enter native infinite loop");
        auto texture=std::make_shared<spTextureData>();Check(controller->GetTextureTrackForAnalysis().SetKeysForAnalysis({1,2},{texture,texture}),"two owner slots");
        controller->ApplyForAnalysis(.5F);Check(holder.UpdateTextureAnimationForAnalysis()&&holder.GetTextureForAnalysis()==texture.get(),"initial fallback");
        holder.SetOwnedAnimTextureControllerForAnalysis(controller);Check(controller->GetMaterialForAnalysis()==&holder,"alias keeps binding");
        {spMaterialTexture other;other.SetOwnedAnimTextureControllerForAnalysis(controller);Check(controller->GetMaterialForAnalysis()==&other,"shared last binding wins");}
        Check(!controller->GetMaterialForAnalysis()&&!holder.UpdateTextureAnimationForAnalysis(),"host clears last-bound destroyed holder instead of stale backlink");
        holder.SetOwnedAnimTextureControllerForAnalysis(controller);controller->ApplyForAnalysis(1e20F);
        Check(!holder.UpdateTextureAnimationForAnalysis(),"bounded wrap work");holder.SetOwnedAnimTextureControllerForAnalysis(nullptr);
        Check(!controller->GetMaterialForAnalysis(),"safe explicit clear differs from unsafe native NULL setter");
        spTextureTrack track;std::shared_ptr<spTexture> value;
        Check(!track.SetKeysForAnalysis({1},{}),"shape mismatch rejected");
        Check(track.SetKeysForAnalysis({2,1},{texture,texture})&&!track.EvaluateForAnalysis(0,value),"raw unsorted wire can be retained but runtime rejects");
        Check(track.SetKeysForAnalysis({std::numeric_limits<float>::quiet_NaN()},{texture})&&!track.EvaluateForAnalysis(0,value),"NaN runtime safety");
        track.ReleaseKeysForAnalysis();Check(track.GetTimesForAnalysis().empty()&&track.EvaluateForAnalysis(0,value)&&!value,"empty track evaluation itself returns NULL");
        for(unsigned kind=0;kind<4;++kind)
        {
            Bytes raw;Add(raw,kind==0?4097u:1u);if(kind!=1)Add(raw,1.F);
            if(kind==2){Add(raw,7u);Add(raw,0xffffffffu);}else{Add(raw,0u);if(kind==3)Add(raw,0u);}
            Bytes bytes;Field(bytes,0,raw);bytes.push_back(0);spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
            spAnimTexController object;spAnimTexControllerSerializer serializer;spMemoryStream source;Open(source,bytes);std::string error;
            Check(!serializer.ReadPayloadForAnalysis(context,source,static_cast<std::uint32_t>(bytes.size()),object,&error)&&context.failed&&!error.empty(),"bounded count/time/inline/trailing failures poison context without allocation expansion");
        }
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--anim-track"){std::cout<<Track(argv[2])<<'\n';return 0;}
        if(argc==3&&std::string(argv[1])=="--material-animation"){std::cout<<MaterialGraph(argv[2])<<'\n';return 0;}
        for(const auto* mode:{"three","alias","null","empty","duplicate","negative","boundary"})(void)Track(mode);
        for(const auto* mode:{"inline","repeat","null-after","two-layers"})(void)MaterialGraph(mode);
        Guards();std::cout<<"PASS "<<checks<<'/'<<checks<<": PC animated texture shared codec, endpoint playback and owners\n";return 0;
    }
    catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
