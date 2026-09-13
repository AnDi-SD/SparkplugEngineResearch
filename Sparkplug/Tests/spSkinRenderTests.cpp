#include "Analysis/PC/spSkinRenderContext.h"
#include "Analysis/PC/spRenderNodeContext.h"
#include "Analysis/PC/spNodeTransformMath.h"
#include "Code/Sparkplug/spSkin.h"
#include "Code/Sparkplug/spNode.h"
#include "Code/Sparkplug/spStdLayer.h"
#include "Code/Sparkplug/spMaterialPassLayer.h"
#include "Code/Sparkplug/spSkinSerializer.h"
#include "Code/Sparkplug/spNodeSerializer.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceFATSerializer.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/SparkBase/spMemoryStream.h"
#include "Code/SparkplugDX/spDXMaterial.h"
#include "Code/SparkplugDX/spDXMesh.h"
#include "Code/SparkplugDX/spDXSharedMeshData.h"
#include "Code/SparkplugPC/spPCVertexDeclaration.h"
#include "Code/SparkplugPC/spPCShaderManager.h"
#include "pcSanFixture.h"
#include "pcSkinMeshFixture.h"
#include "pcSkinMaterialFixture.h"
#include "pcSkinTextureFixture.h"
#include "pcSkinLightFixture.h"
#include "Code/Sparkplug/spActor.h"
#include "Code/Sparkplug/spSceneManager.h"
#include "Code/Sparkplug/spFog.h"
#include "Code/Sparkplug/spFogSerializer.h"
#include "Code/Sparkplug/spRenderNode.h"
#include "Code/Sparkplug/spVertexBuffer.h"
#include "Code/SparkplugDX/spDXCamera.h"
#include "Code/SparkplugDX/spDXLight.h"
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace
{
    using R=spDXRenderer;using C=sparkplug::evidence::pc::SkinRenderContextForAnalysis;int checks=0;
    void* alphaFlushObserver=nullptr; // explicit test callback observer, no engine singleton
    void* skinDrawObserver=nullptr;
    void Check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
    template<class T>void Array(std::ostream& out,const T& values){out<<'[';bool first=true;for(const auto& v:values){if(!first)out<<',';first=false;out<<v;}out<<']';}
    std::uint32_t Bits(float value){std::uint32_t bits;std::memcpy(&bits,&value,4);return bits;}
    void Quote(std::ostream& out,std::string_view text)
    {out<<'"';for(unsigned char c:text){if(c=='"'||c=='\\')out<<'\\'<<c;else if(c<32)out<<"\\u00"<<"0123456789abcdef"[c>>4]<<"0123456789abcdef"[c&15];else out<<c;}out<<'"';}
    std::string Hex(const void* data,std::size_t size){const auto* p=static_cast<const unsigned char*>(data);constexpr char d[]="0123456789abcdef";std::string text;for(std::size_t i=0;i<size;++i){text+=d[p[i]>>4];text+=d[p[i]&15];}return text;}
    struct Sink
    {
        C state;std::string mode,protocolMode;std::vector<std::string> events,shaderEvents,protocolTrace;bool generatedValid=true;
        std::uint8_t savedRenderByte=0x33;const spDXMaterial* ownedMaterial=nullptr;
        const spDXTexture* textureIdentity=nullptr;
        R::AlphaQueueForAnalysis* alpha=nullptr;std::vector<unsigned> queuedResults;
        sparkplug::evidence::pc::RenderNodeContextForAnalysis supportContext;
        void Protocol(const char* name)
        {
            if(protocolMode.empty())return;
            std::ostringstream out;out<<"[\""<<name<<"\",["<<unsigned(state.renderStateByte)<<','<<unsigned(savedRenderByte)<<','
                <<(state.selectedMaterial?(state.selectedMaterial==ownedMaterial?1:2):0)<<','<<state.packedColor<<','<<state.activeBoneCount<<"]]";
            protocolTrace.push_back(out.str());
        }
        std::string State()const
        {const auto& g=state.submission.geometry;return '['+std::to_string(g.declaration?1:0)+','+std::to_string(g.indices?2:0)+','+std::to_string(g.vertices?3:0)+','+std::to_string(state.activeBoneCount)+']';}
        std::int32_t Event(const std::string& text){events.push_back('['+text+','+State()+']');return mode=="failed-device"?-1:0;}
        static std::int32_t Geometry(void* p,unsigned kind,std::uintptr_t handle,unsigned stride)noexcept
        {auto& s=*static_cast<Sink*>(p);std::ostringstream o;if(kind==0)o<<"\"declaration\","<<handle;else if(kind==1)o<<"\"indices\","<<handle;else o<<"\"stream\",0,"<<handle<<",0,"<<stride;return s.Event(o.str());}
        static std::int32_t Render(void* p,unsigned index,unsigned value)noexcept{return static_cast<Sink*>(p)->Event("\"render\","+std::to_string(index)+','+std::to_string(value));}
        static std::int32_t Material(void* p,const R::LightingStateForAnalysis& v)noexcept
        {std::array<unsigned,17> words{};unsigned i=0;for(const auto* c:{&v.diffuse,&v.ambient,&v.specular,&v.emissive})for(float f:*c)words[i++]=Bits(f);words[16]=Bits(v.specularPower);std::ostringstream o;o<<"\"material\",";Array(o,words);return static_cast<Sink*>(p)->Event(o.str());}
        static std::int32_t TextureState(void* p,bool sampler,unsigned stage,unsigned index,unsigned value)noexcept
        {return static_cast<Sink*>(p)->Event(std::string(sampler?"\"sampler\",":"\"stage\",")+std::to_string(stage)+','+std::to_string(index)+','+std::to_string(value));}
        static std::optional<std::uintptr_t> TextureHandle(void* p,const spDXTexture& texture)noexcept
        {return static_cast<Sink*>(p)->textureIdentity==&texture?std::optional<std::uintptr_t>{9}:std::nullopt;}
        static std::int32_t Texture(void* p,unsigned stage,std::uintptr_t handle)noexcept
        {return static_cast<Sink*>(p)->Event("\"texture\","+std::to_string(stage)+','+std::to_string(handle));}
        static std::int32_t Light(void* p,unsigned index,const std::array<unsigned,26>& words)noexcept
        {std::ostringstream o;o<<"\"light\","<<index<<',';Array(o,words);return static_cast<Sink*>(p)->Event(o.str());}
        static std::int32_t LightEnable(void* p,unsigned index,bool enabled)noexcept
        {return static_cast<Sink*>(p)->Event("\"enable\","+std::to_string(index)+','+std::to_string(enabled));}
        static std::int32_t Shader(void* p,bool pixel,std::uintptr_t handle)noexcept{return static_cast<Sink*>(p)->Event(std::string(pixel?"\"pixel-shader\",":"\"vertex-shader\",")+std::to_string(handle));}
        static std::int32_t Create(void* p,const unsigned* code,std::uintptr_t* output)noexcept
        {auto& s=*static_cast<Sink*>(p);const auto hex=code?Hex(code,5):std::string{};s.generatedValid&=hex=="1020304050";*output=7;s.shaderEvents.push_back("[\"create\",7]");return s.Event("\"create\",\""+hex+"\"");}
        static void Release(void* p,std::uintptr_t handle)noexcept
        {auto& s=*static_cast<Sink*>(p);s.generatedValid&=handle==7;s.shaderEvents.push_back("[\"release\",7]");}
        static std::int32_t Constants(void* p,bool pixel,const std::array<unsigned,4>* rows,std::size_t count)noexcept
        {std::vector<unsigned> words;for(std::size_t i=0;i<count;++i)for(auto v:rows[i])words.push_back(v);std::ostringstream o;o<<(pixel?"\"pixel-constants\",0,":"\"vertex-constants\",0,");Array(o,words);o<<','<<count;return static_cast<Sink*>(p)->Event(o.str());}
        static std::int32_t Draw(void* p,bool indexed,const std::array<unsigned,6>& args)noexcept
        {std::ostringstream o;o<<(indexed?"\"indexed\"":"\"draw\"");for(unsigned i=0;i<(indexed?6u:3u);++i)o<<','<<args[i];return static_cast<Sink*>(p)->Event(o.str());}
        static std::int32_t Matrix(void* p,unsigned type,const R::MatrixStateForAnalysis::RawMatrix& matrix)noexcept
        {auto& s=*static_cast<Sink*>(p);s.Protocol("transform");std::ostringstream o;o<<"\"transform\","<<type<<',';Array(o,matrix);return s.Event(o.str());}
        static std::int32_t Pre(spRenderable*,spCamera*,void* p){auto& s=*static_cast<Sink*>(skinDrawObserver?skinDrawObserver:p);s.Protocol("pre");s.Event("\"pre\"");return s.mode=="pre-false"?0:1;}
        static std::int32_t Post(spRenderable*,spCamera*,void* p){auto& s=*static_cast<Sink*>(skinDrawObserver?skinDrawObserver:p);s.Protocol("post");s.Event("\"post\"");return s.mode=="post-false"?0:1;}
        static void QueueSort(void* p,R::AlphaEntryForAnalysis*,std::size_t count,R::AlphaDispatchForAnalysis::Compare)
        {auto& s=*static_cast<Sink*>(p);Check(count==1,"one-record declared external qsort");s.events.push_back("[\"qsort\",1,"+std::to_string(s.alpha->flushing)+']');}
        static bool QueuePrepare(void* p,spRenderNode& node)
        {return node.PrepareForRenderForAnalysis(static_cast<Sink*>(p)->supportContext);}
        static bool QueueRender(void* p,spRenderable& object,spCamera* camera,spRenderNode* support)
        {auto& s=*static_cast<Sink*>(p);auto* skin=dynamic_cast<spSkin*>(&object);Check(skin!=nullptr,"whole queued Skin type");const bool result=skin->RenderUnlitForAnalysis(s.state,camera,support);s.queuedResults.push_back(result);return result;}
    };
    std::vector<std::shared_ptr<spBaseObject>> LoadSkin(spSkin& skin,const std::string& directory,const std::string& payload)
    {
        const auto open=[](spMemoryStream& stream,const std::string& text)
        {
            Check(text.size()%2==0,"even hexadecimal stream");
            Check(stream.ResizeAndSetSize(static_cast<unsigned>(text.size()/2)),"loaded stream capacity");
            auto* bytes=static_cast<unsigned char*>(stream.GetBuffer());
            for(std::size_t i=0;i<text.size();i+=2)bytes[i/2]=static_cast<unsigned char>(std::stoul(text.substr(i,2),nullptr,16));
            Check(stream.Seek(spStream::SeekSource::essStart,0),"loaded stream rewind");
        };
        spSerializerManager manager;spResourceManager resources;spMemoryStream index;open(index,directory);
        Check(manager.GetFATForAnalysis()->LoadIndexForAnalysis(index),"linked FAT read");
        Check(manager.RegisterForAnalysis(spNode::ClassID,std::make_shared<spNodeSerializer>(),0xff,3),"linked Node serializer");
        Check(manager.RegisterForAnalysis(spMaterialDataSerializer::TargetClassID,std::make_shared<spMaterialDataSerializer>(),0xff,3),"linked material serializer");
        Check(manager.RegisterForAnalysis(spFog::ClassID,std::make_shared<spFogSerializer>(),0xff,3),"linked Fog serializer");
        manager.SetDispatchContextForAnalysis(2,1);spSerializerReadContextForAnalysis context(manager,resources);
        spMemoryStream input;open(input,payload);std::string error;
        Check(spSkinSerializer{}.ReadPayloadForAnalysis(context,input,static_cast<unsigned>(payload.size()/2),skin,&error),error.c_str());
        unsigned cursor=0;Check(input.GetCurrentPosition(cursor)&&cursor==payload.size()/2,"linked reader completed exactly");
        Check(skin.GetBoneCountForAnalysis()==1&&skin.GetBoneBindingsForAnalysis()[0].GetBoneForAnalysis().get()==manager.GetFATForAnalysis()->FindByIDForAnalysis(7)->object,"retained canonical decoded bone");
        // Keep the decoded graph owners in the calling fixture. Skin's loaded
        // bone palette borrows them, matching the native non-retaining edges.
        return context.createdObjects;
    }
    std::string AnimateSkin(spSkin& skin,const char* asset,const std::string& delta,bool sceneWorld=false)
    {
        spAnimationManager manager;auto animation=sparkplug::tests::ReadOwnedSan(asset,manager);
        auto bone=skin.GetBoneBindingsForAnalysis().at(0).GetBoneForAnalysis();
        Check(animation->GetTrackCountForAnalysis()==5,"real bbush track count");
        bone->SetName(animation->GetTrackForAnalysis(0)->GetName());spActor actor;std::string error;
        Check(actor.DiscoverNodeForAnalysis(bone,&error),error.c_str());
        spActor::StartRequestForAnalysis request;request.animation=animation.get();request.weight=.25f;
        request.fallbackFadeInRate=.5f;request.fallbackFadeOutRate=.75f;
        std::vector<spActor::ActionForAnalysis> actions;
        Check(actor.StartForAnalysis(request,actions,&error)==0,"same bone bound to real SAN track");
        Check(manager.AdvanceFrameForAnalysis(std::stof(delta)),"actual manager dispatch to actor");
        if(sceneWorld)
        {
            spSceneManager worlds;auto root=std::make_shared<spNode>();
            root->SetPositionForAnalysis({10,-20,30});root->MarkLocalTransformDirtyForAnalysis();
            Check(root->AttachChildForAnalysis(bone),"same SAN bone under actual plain root");
            spSceneManager::SceneForAnalysis view{root.get()};
            Check(worlds.RegisterSceneForAnalysis(view)&&worlds.UpdateWorldForAnalysis(),"scene manager invokes root and same animated bone worlds");
        }
        else Check(bone->UpdateWorldForAnalysis(1),"explicit world-cache endpoint after actor tick");
        std::vector<unsigned> words;
        const auto append=[&](const auto& values){for(float value:values)words.push_back(Bits(value));};
        append(bone->GetPositionForAnalysis());append(bone->GetScaleForAnalysis());append(bone->GetOrientationForAnalysis());
        append(bone->GetWorldPositionForAnalysis());append(bone->GetWorldScaleForAnalysis());append(bone->GetWorldOrientationForAnalysis());
        const auto& state=*actor.GetPlaybackForAnalysis(0);
        std::ostringstream out;out<<"[\""<<delta<<"\","<<Bits(state.sampleTime)<<','<<manager.GetFrameForAnalysis()<<','
            <<state.bindingUseCount<<','<<bone->GetFlagsForAnalysis()<<',';Array(out,words);out<<']';return out.str();
    }
    std::string RunAlpha(const std::string& mode,const std::string* directory=nullptr,const std::string* payload=nullptr)
    {
        struct AlphaSink final
        {
            R::AlphaQueueForAnalysis queue;std::vector<std::string> events;
            static std::int32_t Pre(spRenderable*,spCamera*,void* p)
            {auto& s=*static_cast<AlphaSink*>(p);s.events.push_back("[\"pre\","+std::to_string(s.queue.count)+','+std::to_string(s.queue.flushing)+']');return 0;}
        } sink;
        spSkin skin;
        const auto loadedOwners=directory?LoadSkin(skin,*directory,*payload):std::vector<std::shared_ptr<spBaseObject>>{};
        if(!directory)
        {
            auto material=std::make_shared<spDXMaterial>();auto pass=std::make_shared<spMaterialPassLayer>();
            pass->SetFinalBlendOperationForAnalysis(mode=="zero-blend"?0:1);
            Check(material->SetPassForAnalysis(0,pass),"alpha first pass");skin.SetMaterialForAnalysis(material);
        }
        struct SphereMesh final:spMesh{SphereMesh(){SetBoundingSphereForAnalysis({1,2,3,1});}};
        skin.SetBaseMeshForAnalysis(std::make_shared<SphereMesh>());
        spRenderNode support;support.SetPositionForAnalysis({10,20,30});support.MarkLocalTransformDirtyForAnalysis();
        Check(support.UpdateWorldForAnalysis(1),"alpha support world");support.UpdateRenderMatricesForAnalysis();
        spDXCamera camera;R::AlphaCameraInputForAnalysis view{&camera,{1,0,0,0,0,1,0,0,0,0,1,0,-1,-2,-3,1},mode=="depth-only"};
        auto& queue=sink.queue;queue.enabled=mode!="queue-disabled";queue.flushing=mode=="already-flushing";
        queue.sortTransparent=mode!="alpha-disabled";queue.priorityBase=mode=="priority-wrap"?0xfffffff9u:0;
        queue.count=mode=="capacity"?2048:0;skin.SetAlphaSortEnabledForAnalysis(mode!="object-disabled");skin.SetPriorityForAnalysis(13);
        C context;context.alphaQueue=&queue;context.alphaCamera=&view;context.alphaSupport=&support;
        Check(skin.SetDirectCallbackForAnalysis(spRenderable::CallbackPhaseForAnalysis::Pre,AlphaSink::Pre),"alpha direct callback");
        const bool result=skin.RenderUnlitForAnalysis(context,&camera,&sink);
        Check(!result,"Skin returns false after enqueue or rejected direct callback");
        const bool queued=mode=="queued"||mode=="depth-only"||mode=="queue-disabled"||mode=="capacity"||mode=="priority-wrap";
        Check(sink.events.empty()==queued,"alpha routing precedes pre callback and absent draw dependencies");
        Check(queue.count==(mode=="capacity"?2048u:queued&&mode!="queue-disabled"?1u:0u),"alpha count/capacity/disabled semantics");
        const auto& entry=queue.entries[0];std::ostringstream out;
        out<<"[\""<<mode<<"\","<<result<<','<<queue.count<<','<<queue.flushing<<",[";
        for(std::size_t i=0;i<sink.events.size();++i){if(i)out<<',';out<<sink.events[i];}
        out<<"],["<<(entry.camera==&camera?1:0)<<','<<(entry.support==&support?2:0)<<','<<(entry.renderable==&skin?3:0)<<','
            <<Bits(entry.key.distanceSquared)<<','<<entry.key.priority<<','<<entry.key.exactParticleSystem<<"]]";return out.str();
    }
    std::string RunAlphaFlush(const std::string& mode,const std::string* directory=nullptr,const std::string* payload=nullptr)
    {
        struct FlushSink final
        {
            std::string mode;R::AlphaQueueForAnalysis queue;C skinContext;
            sparkplug::evidence::pc::RenderNodeContextForAnalysis supportContext;
            spLightManager::CacheForAnalysis priorLights;std::array<spRenderNode*,2> nodes{};
            std::vector<std::string> events;unsigned preCount=0,setupCount=0;
            unsigned Lights()const
            {for(unsigned i=0;i<2;++i)if(supportContext.currentLights==&nodes[i]->GetLightCacheForAnalysis())return i+1;return supportContext.currentLights==&priorLights?9:0;}
            std::array<unsigned,4> Sphere()const
            {std::array<unsigned,4> result{};for(unsigned i=0;i<4;++i)result[i]=Bits(supportContext.currentSphere[i]);return result;}
            static void Sort(void* ptr,R::AlphaEntryForAnalysis* entries,std::size_t count,R::AlphaDispatchForAnalysis::Compare compare)
            {
                auto& s=*static_cast<FlushSink*>(ptr);Check(count<=3,"bounded external qsort count");
                for(std::size_t i=1;i<count;++i)Check(compare(entries[i-1],entries[i])==-1&&entries[i-1].key.priority>entries[i].key.priority,"declared already ordered distinct priorities");
                s.events.push_back("[\"qsort\","+std::to_string(count)+','+std::to_string(s.queue.flushing)+']');
            }
            static std::int32_t Matrix(void* ptr,unsigned type,const R::MatrixStateForAnalysis::RawMatrix& matrix)noexcept
            {
                auto& s=*static_cast<FlushSink*>(ptr);++s.setupCount;std::ostringstream out;out<<"[\"matrix\",";Array(out,matrix);
                out<<','<<s.queue.count<<','<<s.queue.flushing<<','<<s.Lights()<<',';Array(out,s.Sphere());out<<']';s.events.push_back(out.str());
                return s.mode=="failed-device"?-1:0;
            }
            static bool Prepare(void* ptr,spRenderNode& node)
            {return node.PrepareForRenderForAnalysis(static_cast<FlushSink*>(ptr)->supportContext);}
            static bool Render(void* ptr,spRenderable& object,spCamera* camera,spRenderNode* support)
            {auto& s=*static_cast<FlushSink*>(ptr);auto* skin=dynamic_cast<spSkin*>(&object);Check(skin!=nullptr,"actual queued Skin dispatch");s.skinContext.alphaSupport=support;return skin->RenderUnlitForAnalysis(s.skinContext,camera,support);}
            static std::int32_t Pre(spRenderable*,spCamera*,void* support)
            {
                auto& s=*static_cast<FlushSink*>(alphaFlushObserver);++s.preCount;
                const unsigned token=s.nodes[0]==support?1:2;
                s.events.push_back("[\"pre\","+std::to_string(token)+','+std::to_string(s.queue.count)+','+std::to_string(s.queue.flushing)+']');
                if(s.mode=="append-during-pre"&&s.preCount==1){s.queue.entries[3]=s.queue.entries[2];s.queue.count=4;}
                return 0;
            }
        } sink;
        sink.mode=mode;alphaFlushObserver=&sink;
        struct ResetObserver final{~ResetObserver(){alphaFlushObserver=nullptr;}} resetObserver;
        auto skin=std::make_shared<spSkin>();
        const auto loadedOwners=directory?LoadSkin(*skin,*directory,*payload):std::vector<std::shared_ptr<spBaseObject>>{};
        if(!directory)
        {
            auto material=std::make_shared<spDXMaterial>();auto pass=std::make_shared<spMaterialPassLayer>();pass->SetFinalBlendOperationForAnalysis(1);
            Check(material->SetPassForAnalysis(0,pass),"queued alpha material");skin->SetMaterialForAnalysis(material);
        }
        struct SphereMesh final:spMesh{SphereMesh(){SetBoundingSphereForAnalysis({1,2,3,1});}};
        skin->SetBaseMeshForAnalysis(std::make_shared<SphereMesh>());
        std::array<spRenderNode,2> nodes;
        for(unsigned i=0;i<2;++i)
        {
            auto& node=nodes[i];sink.nodes[i]=&node;node.SetPositionForAnalysis(i?spNode::Vector3{-5,4,0}:spNode::Vector3{10,20,30});
            node.MarkLocalTransformDirtyForAnalysis();Check(node.UpdateWorldForAnalysis(1),"queued support world");node.UpdateRenderMatricesForAnalysis();
            Check(node.AttachRenderableForAnalysis(skin),"two actual nodes own same Skin");
        }
        spDXCamera camera;R::AlphaCameraInputForAnalysis view{&camera,{1,0,0,0,0,1,0,0,0,0,1,0,-1,-2,-3,1},false};
        auto& c=sink.skinContext;c.alphaQueue=&sink.queue;c.alphaCamera=&view;
        sink.supportContext.matrices=&c.matrices;sink.supportContext.setMatrix=FlushSink::Matrix;sink.supportContext.deviceContext=&sink;
        sink.supportContext.preserveLightSelection=mode=="preserve-lights";
        if(mode=="preserve-lights")sink.supportContext.currentLights=&sink.priorLights;
        Check(skin->SetDirectCallbackForAnalysis(spRenderable::CallbackPhaseForAnalysis::Pre,FlushSink::Pre),"actual queued Skin pre callback");
        if(mode!="empty")for(unsigned i=0;i<3;++i)
        {c.alphaSupport=&nodes[i<2?0:1];skin->SetPriorityForAnalysis(30-i*10);Check(!skin->RenderUnlitForAnalysis(c,&camera,c.alphaSupport),"first call enqueues and returns false");}
        Check(sink.events.empty(),"enqueue has no callback/device events");sink.queue.flushing=mode=="already-active";
        R::AlphaDispatchForAnalysis dispatch{FlushSink::Sort,FlushSink::Prepare,FlushSink::Render,&sink};
        const bool result=R::FlushAlphaForAnalysis(sink.queue,dispatch);
        Check(result&&!sink.queue.count&&!sink.queue.flushing,"native flush clears queue despite Skin refusals");
        Check(sink.preCount==(mode=="empty"?0u:mode=="append-during-pre"?4u:3u)&&sink.setupCount==(mode=="empty"?0u:2u),"live count and adjacent support reuse");
        std::ostringstream records;records<<'[';const unsigned count=mode=="empty"?0:mode=="append-during-pre"?4:3;
        for(unsigned i=0;i<count;++i)
        {const auto& e=sink.queue.entries[i];if(i)records<<',';records<<"[1,"<<(e.support==&nodes[0]?1:2)<<",3,"<<Bits(e.key.distanceSquared)<<','<<e.key.priority<<','<<e.key.exactParticleSystem<<']';}
        records<<']';Check(R::FlushAlphaForAnalysis(sink.queue,dispatch),"empty second flush calls qsort");
        std::ostringstream out;out<<"[\""<<mode<<"\","<<result<<",[";
        for(std::size_t i=0;i<sink.events.size();++i){if(i)out<<',';out<<sink.events[i];}
        out<<"],["<<sink.queue.count<<','<<sink.queue.flushing<<','<<sink.Lights()<<',';Array(out,sink.Sphere());out<<','<<c.matrices.dirty<<"],"<<records.str()<<']';return out.str();
    }
    std::string Run(const std::string& mode,const std::string* directory=nullptr,const std::string* payload=nullptr,
                    const char* asset=nullptr,const std::string& delta={},bool generated=false,bool sceneWorld=false,
                    const std::string* meshInput=nullptr,const std::string* materialInput=nullptr,const std::string& protocolMode={},const std::string& fogMode={},
                    const std::string* textureInput=nullptr,const std::string& textureMode={},bool queued=false,const std::string& lightMode={},const std::string& lightConstants={},
                    const std::string* lightWire=nullptr,const std::string& lightCase={},bool selectedLight=false)
    {
        Sink sink;sink.mode=mode;auto& c=sink.state;auto managerOwner=std::make_unique<spPCShaderManager>();auto& manager=*managerOwner;spPCVertexDeclaration declaration;
        std::unique_ptr<spDXMaterial> materialOwner;std::string materialCapture;
        if(materialInput)materialOwner=sparkplug::tests::skin::ReadMaterial(*materialInput,materialCapture);
        else
        {
            materialOwner=std::make_unique<spDXMaterial>();materialOwner->SetSpecularPowerForAnalysis(0);Check(materialOwner->SetRenderStateForAnalysis(8,2),"unlit material");
            auto pass=std::make_shared<spMaterialPassLayer>();for(unsigned i=0;i<2;++i)Check(pass->SetLayerForAnalysis(i,std::make_unique<spStdLayer>()),"fallback layers");Check(materialOwner->SetPassForAnalysis(0,pass),"fallback pass");
        }
        auto& material=*materialOwner;
        spPCRenderer meshRenderer;std::string meshCapture;std::shared_ptr<spDXMesh> mesh;
        if(meshInput)mesh=sparkplug::tests::skin::ReadMesh(*meshInput,meshRenderer,meshCapture);
        else
        {
            auto shared=std::make_shared<spDXSharedMeshData>();Check(shared->InitializeForAnalysis(std::vector<std::byte>(100),std::vector<std::byte>(1152)),"explicit CPU buffer input");
            mesh=std::make_shared<spDXMesh>();Check(mesh->InitializeSharedForAnalysis(shared,0x803,0,spIndexBuffer::eIndexBufferType::Type2,11,17,39,19,32),"mesh source metadata consumed by same submission core");
        }
        spSkin skin;
        const auto loadedOwners=directory?LoadSkin(skin,*directory,*payload):std::vector<std::shared_ptr<spBaseObject>>{};
        if(!directory)
        {
            auto bone=std::make_shared<spNode>();bone->SetPositionForAnalysis({1,2,3});bone->SetScaleForAnalysis({2,3,4});bone->MarkLocalTransformDirtyForAnalysis();Check(bone->UpdateWorldForAnalysis(),"source bone world cache");
            spSkin::Matrix4 inverse{1,0,0,0,0,1,0,0,0,0,1,0,5,6,7,1};Check(skin.SetPaletteForAnalysis(mode=="weights-zero"?0u:4u,{{bone,inverse}}),"source Skin palette");
        }
        std::string animationCapture;if(asset)animationCapture=AnimateSkin(skin,asset,delta,sceneWorld);
        const auto* ownedMaterial=dynamic_cast<const spDXMaterial*>(skin.GetMaterialForAnalysis().get());
        const auto* ownedFog=dynamic_cast<const spFog*>(skin.GetFogForAnalysis().get());std::string fogCapture;
        if(ownedFog)
        {const std::array<unsigned,5> words{static_cast<unsigned>(ownedFog->GetTypeForAnalysis()),ownedFog->GetColorARGBForAnalysis(),Bits(ownedFog->GetStartForAnalysis()),Bits(ownedFog->GetEndForAnalysis()),Bits(ownedFog->GetDensityForAnalysis())};fogCapture=Hex(words.data(),20);}
        if(ownedMaterial)materialCapture=sparkplug::tests::skin::CaptureMaterial(*ownedMaterial);
        std::string textureCapture;
        if(textureInput)
        {
            Check(ownedMaterial!=nullptr,"texture uses whole decoded owning material");
            if(generated)
            {meshRenderer.ClearVertexDeclarationsForAnalysis();Check(!meshRenderer.GetVertexDeclarationCountForAnalysis()&&mesh->GetVertexDeclarationForAnalysis(),"decoded mesh survives ended declaration map lifetime");}
            auto texture=sparkplug::tests::skin::ReadTexture(*textureInput,textureCapture);sink.textureIdentity=texture.get();
            auto* pass=dynamic_cast<spMaterialPassLayer*>(ownedMaterial->GetPassForAnalysis(0));
            Check(pass&&pass->GetLayerForAnalysis(0),"decoded texture holder");
            pass->GetLayerForAnalysis(0)->GetMaterialTextureForAnalysis()->SetOwnedFallBackTextureForAnalysis(std::move(texture));
        }
        skin.SetBaseMeshForAnalysis(mesh);
        std::string decodedLightCapture;auto lightOwner=lightWire?sparkplug::tests::skin::ReadLight(*lightWire,decodedLightCapture):std::make_unique<spDXLight>();
        auto& light=*lightOwner;R::DeviceLightInputForAnalysis lightInput;R::LightListInputForAnalysis lightList;R::LightSubmissionForAnalysis lighting;
        std::unique_ptr<spRenderNode> lightSelectionNode;std::array<R::DeviceLightInputForAnalysis,9> selectedLightStorage;std::string selectedCacheCapture;
        const unsigned lightKind=lightMode=="point"?1u:lightMode=="spot"?2u:lightMode=="ambient"?3u:0u;
        const bool ordinary=lightMode!="null"&&lightMode!="empty"&&lightMode!="ambient";
        const unsigned rawLightMode=lightMode=="mode-zero"?0u:lightMode=="unlit-list"?2u:lightMode=="mode-three"?3u:lightMode=="mode-four"?4u:lightMode=="mode-five"?5u:lightMode=="mode-six"?6u:lightMode=="mode-seven"?7u:1u;
        if(!lightMode.empty())
        {
            meshRenderer.ClearVertexDeclarationsForAnalysis();
            if(!lightWire)
            {
            light.SetTypeForAnalysis(static_cast<spLight::Type>(lightKind));light.SetColorForAnalysis({.25F,.75F,1.5F,.5F});
            if(lightMode=="mode-six")skin.SetField28ForAnalysis(0x80402010);
            light.SetUsesAttenuationForAnalysis(true);light.SetIntensityForAnalysis(2);light.SetRangeForAnalysis(200);
            light.SetHotspotAngleForAnalysis(.5F);light.SetFalloffAngleForAnalysis(1);light.SetLightEnabledForAnalysis(lightMode!="disabled");
            }
            else Check(static_cast<unsigned>(light.GetTypeForAnalysis())==lightKind,"same decoded type enters shader key");
            if(!lightConstants.empty())
            {
                if(!lightWire)
                {light.SetPositionForAnalysis({4,5,6});light.SetOrientationForAnalysis({1,0,0,0,1,0,1,2,3});light.MarkLocalTransformDirtyForAnalysis();
                Check(light.UpdateWorldForAnalysis(1),"same actual source light world producer");}
                const std::array<float,16> view{2,0,0,0,0,3,0,0,0,0,4,0,5,6,7,1};for(unsigned i=0;i<16;++i)c.matrices.inputs[1][i]=Bits(view[i]);
            }
            const auto& worldDirection=light.GetWorldOrientationForAnalysis();
            if(selectedLight)
            {
                lightSelectionNode=std::make_unique<spRenderNode>();lightSelectionNode->SetPositionForAnalysis(light.GetWorldPositionForAnalysis());
                Check(lightSelectionNode->UpdateWorldForAnalysis(1),"explicit current target sphere input");
                spLightManager sceneLights;Check(sceneLights.RegisterLightForAnalysis(light),"same decoded light in actual source manager");
                Check(sceneLights.RegisterRenderTargetForAnalysis(lightSelectionNode->GetLightCacheForAnalysis(),lightSelectionNode->GetWorldBoundingSphereForAnalysis(),false),"same actual RenderNode cache target");
                light.SetHierarchyActiveForAnalysis(true);light.SetSceneLightManagerForAnalysis(&sceneLights);
                Check(light.UpdateWorldForAnalysis(0),"whole light world selects actual RenderNode cache");light.SetSceneLightManagerForAnalysis(nullptr);
                const auto& cache=lightSelectionNode->GetLightCacheForAnalysis();std::ostringstream capture;capture<<'[';
                for(auto* item:cache.GetRawSlots())capture<<(item==&light?7:0)<<',';capture<<(cache.GetAmbient()==&light?7:0)<<','<<cache.GetCount()<<']';selectedCacheCapture=capture.str();
                Check(sceneLights.UnregisterRenderTargetForAnalysis(lightSelectionNode->GetLightCacheForAnalysis()),"end borrowed scene target binding");
            }
            else if(lightWire)Check(light.UpdateWorldForAnalysis(0),"decoded light virtual world builds device payload");
            else Check(light.RefreshDevicePayloadForAnalysis(lightConstants.empty()?spNode::Vector3{4,5,6}:light.GetWorldPositionForAnalysis(),
                lightConstants.empty()?spNode::Vector3{1,2,3}:spNode::Vector3{worldDirection[6],worldDirection[7],worldDirection[8]},
                {.125F,.25F,.5F},0x7f234567),"actual light device payload consumes produced world cache");
            lightInput.identity=7;lightInput.type=lightKind;lightInput.enabled=light.IsLightEnabledForAnalysis();lightInput.color=light.GetColorForAnalysis();lightInput.sourceObject=&light;
            for(unsigned i=0;i<26;++i)lightInput.deviceWords[i]=light.GetDevicePayloadForAnalysis()[i].value_or(0xcccccccc);
            if(selectedLight)Check(R::ResolveLightCacheForAnalysis(lightSelectionNode->GetLightCacheForAnalysis(),selectedLightStorage,lightList,0xcccccccc,[](const spLight&)noexcept{return std::uintptr_t{7};}),"actual selected cache resolves same objects into renderer");
            else {if(ordinary)lightList.lights.push_back(&lightInput);if(lightMode=="ambient")lightList.ambient=&lightInput;}
            lighting.input=lightMode=="null"?nullptr:&lightList;lighting.fallbackARGB=0x7f234567;lighting.state.previousCount=3;
            lighting.state.borrowedList=lighting.input?9:0;lighting.state.grouped.fill(7);lighting.state.ambient.fill(.25F);c.lights=&lighting;
        }
        spPCEffectTemplate::CompilerStateForAnalysis compilerState;spPCEffectTemplate::CompilerForAnalysis compiler;
        unsigned compilerCalls=0;std::ostringstream compilerRequests;
        const spPCShaderGenerationForAnalysis generation{&compiler,&compilerState,Sink::Create,Sink::Release,&sink};
        if(generated)
        {
            auto effect=std::make_unique<spPCEffectTemplate>(1,"",2);spPCEffectTemplate::PassForAnalysis templatePass;
            templatePass.shaders[0].text[2]="Main";templatePass.shaders[0].text[3]="vs_2_0";effect->AppendPassForAnalysis(templatePass);
            Check(manager.SetFixedTemplateForAnalysis(effect)&&!effect,"same prepared template ownership");
            compiler=[&](const auto& request)
            {
                ++compilerCalls;Check(compilerState.active&&!request.assembly,"actual template compiler request");
                compilerRequests<<"[\"hlsl\",";Quote(compilerRequests,request.source);compilerRequests<<",[";Quote(compilerRequests,request.entry);compilerRequests<<',';Quote(compilerRequests,request.target);compilerRequests<<"],0,[1]]";
                spPCEffectTemplate::CompilerOutputForAnalysis result;result.bytecode=std::vector<std::uint8_t>{0x10,0x20,0x30,0x40,0x50};
                result.reflection=std::vector<spPCEffectTemplate::ParameterForAnalysis>{{"BlendMatrices",0,3},{"MatDiffuse",3,1}};
                if(!lightConstants.empty())
                {
                    const std::array<const char*,4> names=(lightConstants.size()>=8&&lightConstants.compare(lightConstants.size()-8,8,"-special")==0)?std::array<const char*,4>{"AmbientCol","LightAmbientColorDir0","LightDiffuseColorDir0","LightSpecularColorDir0"}:std::array<const char*,4>{"LightMatDiff","LightPos","LightDir","LightAttenuation"};
                    unsigned start=4;for(const auto* name:names)result.reflection->push_back({name,start++,1});
                }
                return result;
            };
            c.shaderGeneration=&generation;
        }
        else
        {
            auto shader=std::make_unique<spPCVertexShader>();
            for(const auto& specification:{std::pair<const char*,unsigned>{"BlendMatrices",3},{"MatDiffuse",1}})
            {spDXShader::ParameterForAnalysis p;p.type=0xdeadbeef;p.startRegister=specification.second==3?0:3;p.registerCount=specification.second;std::memcpy(p.name.data(),specification.first,std::strlen(specification.first)+1);Check(shader->AppendParameterForAnalysis(p),"real parameter producer");}
            if(!lightConstants.empty())
            {
                const std::array<const char*,4> names=(lightConstants.size()>=8&&lightConstants.compare(lightConstants.size()-8,8,"-special")==0)?std::array<const char*,4>{"AmbientCol","LightAmbientColorDir0","LightDiffuseColorDir0","LightSpecularColorDir0"}:std::array<const char*,4>{"LightMatDiff","LightPos","LightDir","LightAttenuation"};
                unsigned start=4;for(const auto* name:names){spDXShader::ParameterForAnalysis p;p.type=0xdeadbeef;p.startRegister=start++;p.registerCount=1;std::memcpy(p.name.data(),name,std::strlen(name)+1);Check(shader->AppendParameterForAnalysis(p),"actual light parameter type producer");}
            }
            const unsigned shaderMask=lightMode.empty()?(ownedMaterial&&protocolMode!="preserve-selection"?0x1020011u:0x20011u):0x1000011u|(rawLightMode<<16)|(unsigned(ordinary)<<20);
            Check(manager.CacheShaderForAnalysis({shaderMask,!lightMode.empty()&&ordinary?lightKind:0},shader),"matching existing shader");
        }
        c.shaders=&manager;c.fallback=(generated||textureInput||queued||!lightMode.empty())&&ownedMaterial?const_cast<spDXMaterial*>(ownedMaterial):&material;c.sharedDeclaration=&declaration;c.activeBoneCount=9;c.constants.blendMatrices.resize(1);c.constants.blendMatrices[0].fill(0xcccccccc);
        if(meshInput)c.geometryHandles={1,2,3};
        c.device.geometry=Sink::Geometry;c.device.render=Sink::Render;c.device.material=Sink::Material;c.device.textureState=Sink::TextureState;c.device.shader=Sink::Shader;c.device.constants=Sink::Constants;c.device.draw=Sink::Draw;c.device.context=&sink;c.setMatrix=Sink::Matrix;
        if(textureInput){c.device.texture=Sink::Texture;c.device.textureHandle=Sink::TextureHandle;}
        if(!lightMode.empty()){c.device.light=Sink::Light;c.device.lightEnable=Sink::LightEnable;}
        Check(skin.SetDirectCallbackForAnalysis(spRenderable::CallbackPhaseForAnalysis::Pre,Sink::Pre)&&skin.SetDirectCallbackForAnalysis(spRenderable::CallbackPhaseForAnalysis::Post,Sink::Post),"actual reconstructed callback dispatch");
        if(!protocolMode.empty())
        {
            Check(ownedMaterial!=nullptr,"protocol uses whole decoded owning material");
            sink.protocolMode=protocolMode;sink.ownedMaterial=ownedMaterial;c.renderStateByte=0x7b;c.sharedSavedRenderStateByte=&sink.savedRenderByte;
            dynamic_cast<spDXMaterial*>(skin.GetMaterialForAnalysis().get())->SetRenderOverrideByteForAnalysis(1);skin.SetField28ForAnalysis(0x80402010);
            if(protocolMode=="preserve-selection"){c.preserveMaterialSelection=true;c.selectedMaterial=&material;c.packedColor=0x10203040;}
        }
        bool result=false;std::string enqueueCapture,flushState;unsigned flushResult=0;
        std::unique_ptr<R::AlphaQueueForAnalysis> alpha;std::unique_ptr<spRenderNode> queueSupport;
        spDXCamera queueCamera;R::AlphaCameraInputForAnalysis queueView;
        if(queued)
        {
            meshRenderer.ClearVertexDeclarationsForAnalysis();
            alpha=std::make_unique<R::AlphaQueueForAnalysis>();queueSupport=std::make_unique<spRenderNode>();
            sink.alpha=alpha.get();c.alphaQueue=alpha.get();c.alphaSupport=queueSupport.get();
            queueView={&queueCamera,{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},false};c.alphaCamera=&queueView;skin.SetPriorityForAnalysis(13);
            sink.supportContext.matrices=&c.matrices;sink.supportContext.setMatrix=Sink::Matrix;sink.supportContext.deviceContext=&sink;
            skinDrawObserver=&sink;struct Reset final{~Reset(){skinDrawObserver=nullptr;}} reset;
            Check(!skin.RenderUnlitForAnalysis(c,&queueCamera,queueSupport.get())&&alpha->count==1&&sink.events.empty(),"same decoded Skin enqueues before callbacks/device");
            const auto& entry=alpha->entries[0];std::ostringstream record;record<<"[1,2,3,"<<Bits(entry.key.distanceSquared)<<','<<entry.key.priority<<','<<entry.key.exactParticleSystem<<']';enqueueCapture=record.str();
            flushResult=R::FlushAlphaForAnalysis(*alpha,{Sink::QueueSort,Sink::QueuePrepare,Sink::QueueRender,&sink});
            Check(flushResult&&sink.queuedResults.size()==1,"outer alpha flush succeeds even for inner Skin refusal");result=sink.queuedResults[0]!=0;
            std::ostringstream state;state<<'['<<alpha->count<<','<<alpha->flushing<<','<<(sink.supportContext.currentLights==&queueSupport->GetLightCacheForAnalysis()?2:0)<<",[";
            for(unsigned i=0;i<4;++i){if(i)state<<',';state<<Bits(sink.supportContext.currentSphere[i]);}state<<"]]";flushState=state.str();
        }
        else result=skin.RenderUnlitForAnalysis(c,nullptr,&sink);
        Check(result==(mode!="pre-false"&&mode!="post-false"),"native return including ignored HRESULT");
        sink.Protocol("complete");
        Check(c.activeBoneCount==(mode=="pre-false"?9u:mode=="post-false"?1u:0u),"native count publication/cleanup");
        if(ownedMaterial&&mode!="pre-false")Check(c.selectedMaterial==(protocolMode=="preserve-selection"?c.fallback:ownedMaterial),"decoded or explicitly preserved material selected");
        std::ostringstream out;out<<"[\""<<mode<<"\","<<result<<','<<skin.GetWeightCountForAnalysis()<<','<<c.activeBoneCount<<",\""<<Hex(c.constants.blendMatrices[0].data(),64)<<"\",";Array(out,c.matrices.inputs[0]);out<<','<<c.matrices.dirty<<','<<c.packedColor<<",[";
        for(std::size_t i=0;i<sink.events.size();++i){if(i)out<<',';out<<sink.events[i];}out<<"]]";
        std::string resolvedDraw=out.str();
        if(generated)
        {
            Check(compilerCalls==1&&manager.GetCacheForAnalysis().size()==1,"whole Skin call generates one owned shader");
            managerOwner.reset();Check(sink.generatedValid&&sink.shaderEvents.size()==2,"generated shader handle created and released");
            std::ostringstream capture;capture<<"[["<<compilerRequests.str()<<"],"<<out.str()<<",[";
            for(std::size_t i=0;i<sink.shaderEvents.size();++i){if(i)capture<<',';capture<<sink.shaderEvents[i];}capture<<"]]";
            resolvedDraw=capture.str();
        }
        const auto drawCapture=asset?'['+animationCapture+','+resolvedDraw+']':resolvedDraw;
        const auto meshResult=meshInput?'['+meshCapture+','+drawCapture+']':drawCapture;
        const auto materialResult=ownedMaterial||materialInput?'['+materialCapture+','+meshResult+']':meshResult;
        if(!lightMode.empty())
        {
            std::ostringstream capture;capture<<"[\""<<lightMode<<"\",";Array(capture,lightInput.deviceWords);
            capture<<",["<<lighting.state.previousCount<<','<<lighting.state.borrowedList<<',';Array(capture,lighting.state.grouped);capture<<",[";
            for(unsigned i=0;i<4;++i){if(i)capture<<',';capture<<Bits(lighting.state.ambient[i]);}
            capture<<"],"<<c.submission.deviceStates[139]<<"],"<<materialResult<<']';
            if(!lightConstants.empty())
            {
                std::ostringstream whole;whole<<"[\""<<lightConstants<<"\",[";bool first=true;
                for(float v:light.GetWorldPositionForAnalysis()){if(!first)whole<<',';first=false;whole<<Bits(v);}
                for(const auto* m:{&light.GetWorldOrientationForAnalysis(),&light.GetOrientationForAnalysis()})for(unsigned i=6;i<9;++i)whole<<','<<Bits((*m)[i]);
                whole<<"],"<<capture.str()<<']';
                if(lightWire)return "[\""+lightCase+"\","+decodedLightCapture+','+(selectedLight?selectedCacheCapture+",":"")+whole.str()+']';
                return whole.str();
            }
            return capture.str();
        }
        if(queued)
        {std::ostringstream capture;capture<<"[\""<<mode<<"\","<<enqueueCapture<<','<<flushResult<<',';Array(capture,sink.queuedResults);capture<<','<<flushState<<','<<materialResult<<']';return capture.str();}
        if(!fogMode.empty()||textureInput)
        {
            Check(ownedFog&&c.fog.current==ownedFog,"same decoded Fog reaches renderer identity cache");
            std::ostringstream capture;
            if(textureInput)capture<<"[\""<<textureMode<<"\","<<textureCapture<<",[";
            else capture<<"[\""<<fogMode<<"\",";
            capture<<'"'<<fogCapture<<"\",[";bool first=true;
            for(unsigned index:{0x1cu,0x22u,0x23u,0x24u,0x25u,0x26u}){if(!first)capture<<',';first=false;capture<<c.submission.deviceStates[index];}
            capture<<"],"<<materialResult<<']';if(textureInput)capture<<']';return capture.str();
        }
        if(!protocolMode.empty())
        {
            std::ostringstream capture;capture<<"[\""<<protocolMode<<"\",[";
            for(std::size_t i=0;i<sink.protocolTrace.size();++i){if(i)capture<<',';capture<<sink.protocolTrace[i];}
            capture<<"],"<<materialResult<<']';return capture.str();
        }
        return materialResult;
    }
}
int main(int argc,char** argv)
{
    try{
        if(argc==2&&std::string(argv[1])=="--palette-math")
        {
            unsigned count=0;Check(bool(std::cin>>count)&&count>0&&count<=256,"bounded palette specimen count");
            std::cout<<'[';
            for(unsigned item=0;item<count;++item)
            {
                spSkin::Matrix4 left{},right{};
                for(auto* matrix:{&left,&right})for(auto& value:*matrix)
                {std::uint32_t bits=0;Check(bool(std::cin>>bits),"palette input bits");std::memcpy(&value,&bits,4);}
                const auto actual=spSkin::ComposePaletteMatrixForAnalysis(left,right);
                const auto shared=sparkplug::evidence::pc::node_math::Multiply4ForAnalysis(left,right);
                if(item)std::cout<<',';std::cout<<"{\"skin\":[";
                for(unsigned i=0;i<16;++i){if(i)std::cout<<',';std::cout<<Bits(actual[i]);}
                std::cout<<"],\"shared\":[";
                for(unsigned i=0;i<16;++i){if(i)std::cout<<',';std::cout<<Bits(shared[i]);}
                std::cout<<"]}";
            }
            std::cout<<"]\n";return 0;
        }
        if(argc==3&&std::string(argv[1])=="--case"){std::cout<<Run(argv[2])<<'\n';return 0;}
        if(argc==3&&std::string(argv[1])=="--mesh-bounds")
        {
            std::string indexInput,vertexInput;std::cin>>indexInput>>vertexInput;
            const auto decode=[](const std::string& hex,spMemoryStream& stream){
                Check(hex.size()%2==0&&stream.ResizeAndSetSize(static_cast<unsigned>(hex.size()/2)),"bounds wire buffer");
                auto* bytes=static_cast<unsigned char*>(stream.GetBuffer());
                for(std::size_t i=0;i<hex.size();i+=2)bytes[i/2]=static_cast<unsigned char>(std::stoul(hex.substr(i,2),nullptr,16));};
            spMemoryStream indexStream,vertexStream;decode(indexInput,indexStream);decode(vertexInput,vertexStream);
            spIndexBuffer indices;spVertexBuffer vertices;spDXMesh mesh;
            Check(indices.ReadForAnalysis(indexStream)&&vertices.ReadForAnalysis(vertexStream),"actual CPU bounds readers");
            Check(mesh.InitializeFromBuffersForAnalysis(indices,vertices,true)&&mesh.HasBoundsForAnalysis(),"whole mesh materialization publishes PC bounds");
            std::cout<<"[\""<<argv[2]<<"\",";
            const auto emit=[](const auto& values){std::cout<<'[';bool first=true;for(float value:values){if(!first)std::cout<<',';first=false;std::cout<<Bits(value);}std::cout<<']';};
            emit(mesh.GetBoundingSphereForAnalysis());std::cout<<',';emit(mesh.GetMinimumForAnalysis());std::cout<<',';emit(mesh.GetMaximumForAnalysis());std::cout<<"]\n";return 0;
        }
        if(argc==3&&std::string(argv[1])=="--alpha-queue"){std::string directory,payload;std::cin>>directory>>payload;std::cout<<RunAlpha(argv[2],&directory,&payload)<<'\n';return 0;}
        if(argc==3&&std::string(argv[1])=="--alpha-flush"){std::string directory,payload;std::cin>>directory>>payload;std::cout<<RunAlphaFlush(argv[2],&directory,&payload)<<'\n';return 0;}
        if(argc==3&&std::string(argv[1])=="--loaded"){std::string directory,payload;std::cin>>directory>>payload;std::cout<<Run(argv[2],&directory,&payload)<<'\n';return 0;}
        if(argc==4&&std::string(argv[1])=="--animated"){std::string directory,payload;std::cin>>directory>>payload;std::cout<<Run("normal",&directory,&payload,argv[2],argv[3])<<'\n';return 0;}
        if(argc==5&&std::string(argv[1])=="--animated-generated"){std::string directory,payload;std::cin>>directory>>payload;std::cout<<Run(argv[4],&directory,&payload,argv[2],argv[3],true)<<'\n';return 0;}
        if(argc==5&&std::string(argv[1])=="--scene-generated"){std::string directory,payload;std::cin>>directory>>payload;std::cout<<Run(argv[4],&directory,&payload,argv[2],argv[3],true,true)<<'\n';return 0;}
        if(argc==3&&std::string(argv[1])=="--mesh-generated"){std::string directory,payload,mesh;std::cin>>directory>>payload>>mesh;std::cout<<Run(argv[2],&directory,&payload,nullptr,{},true,false,&mesh)<<'\n';return 0;}
        if(argc==5&&std::string(argv[1])=="--scene-mesh-generated"){std::string directory,payload,mesh;std::cin>>directory>>payload>>mesh;std::cout<<Run(argv[4],&directory,&payload,argv[2],argv[3],true,true,&mesh)<<'\n';return 0;}
        if(argc==5&&std::string(argv[1])=="--material-mesh-generated"){std::string directory,payload,mesh,material;std::cin>>directory>>payload>>mesh>>material;std::cout<<Run(argv[4],&directory,&payload,argv[2],argv[3],true,true,&mesh,&material)<<'\n';return 0;}
        if(argc==5&&std::string(argv[1])=="--owned-material-mesh"){std::string directory,payload,mesh;std::cin>>directory>>payload>>mesh;std::cout<<Run(argv[4],&directory,&payload,argv[2],argv[3],false,true,&mesh)<<'\n';return 0;}
        if(argc==4&&(std::string(argv[1])=="--queued-mesh"||std::string(argv[1])=="--queued-generated"))
        {std::string directory,payload,mesh;std::cin>>directory>>payload>>mesh;std::cout<<Run(argv[3],&directory,&payload,argv[2],"0.25",std::string(argv[1])=="--queued-generated",true,&mesh,nullptr,{},{},nullptr,{},true)<<'\n';return 0;}
        if(argc==4&&std::string(argv[1])=="--lights-mesh")
        {std::string directory,payload,mesh;std::cin>>directory>>payload>>mesh;const std::string light=argv[3];const auto mode=light=="failed-device"||light=="post-false"?light:"normal";std::cout<<Run(mode,&directory,&payload,argv[2],"0.25",false,true,&mesh,nullptr,{},{},nullptr,{},false,light)<<'\n';return 0;}
        if(argc==4&&std::string(argv[1])=="--light-constants")
        {std::string directory,payload,mesh;std::cin>>directory>>payload>>mesh;const std::string constants=argv[3];const auto light=(constants.size()>=8&&constants.compare(constants.size()-8,8,"-special")==0)?constants.substr(0,constants.size()-8):constants;const auto mode=light=="failed-device"||light=="post-false"?light:"normal";std::cout<<Run(mode,&directory,&payload,argv[2],"0.25",false,true,&mesh,nullptr,{},{},nullptr,{},false,light,constants)<<'\n';return 0;}
        if(argc==5&&(std::string(argv[1])=="--decoded-light"||std::string(argv[1])=="--decoded-light-generated"||std::string(argv[1])=="--selected-light"))
        {
            std::string directory,payload,mesh,wire;std::cin>>directory>>payload>>mesh>>wire;
            const std::string item=argv[3],mode=argv[4];const auto index=std::stoul(item.substr(7));Check(index<9,"pinned light case");
            const unsigned types[]={3,1,1,1,0,0,0,3,3};const std::string light=mode!="normal"?mode:types[index]==3?"ambient":types[index]==1?"point":"directional";
            const auto constants=light=="ambient"?"ambient-special":light;
            std::cout<<Run(mode,&directory,&payload,argv[2],"0.25",std::string(argv[1])=="--decoded-light-generated",true,&mesh,nullptr,{},{},nullptr,{},false,light,constants,&wire,item,std::string(argv[1])=="--selected-light")<<'\n';return 0;
        }
        if(argc==4&&std::string(argv[1])=="--material-protocol")
        {std::string directory,payload,mesh;std::cin>>directory>>payload>>mesh;const std::string protocol=argv[3];const auto mode=(protocol=="pre-false"||protocol=="post-false"||protocol=="failed-device")?protocol:"normal";std::cout<<Run(mode,&directory,&payload,argv[2],"0.25",false,true,&mesh,nullptr,protocol)<<'\n';return 0;}
        if(argc==4&&std::string(argv[1])=="--fog-mesh")
        {std::string directory,payload,mesh;std::cin>>directory>>payload>>mesh;const std::string fog=argv[3];const auto mode=(fog=="post-false"||fog=="failed-device")?fog:"normal";std::cout<<Run(mode,&directory,&payload,argv[2],"0.25",false,true,&mesh,nullptr,{},fog)<<'\n';return 0;}
        if(argc==4&&std::string(argv[1])=="--fog-generated")
        {std::string directory,payload,mesh;std::cin>>directory>>payload>>mesh;const std::string fog=argv[3];const auto mode=(fog=="post-false"||fog=="failed-device")?fog:"normal";std::cout<<Run(mode,&directory,&payload,argv[2],"0.25",true,true,&mesh,nullptr,{},fog)<<'\n';return 0;}
        if(argc==4&&(std::string(argv[1])=="--texture-mesh"||std::string(argv[1])=="--texture-generated"))
        {std::string directory,payload,mesh,texture;std::cin>>directory>>payload>>mesh>>texture;const std::string textureMode=argv[3];const auto mode=(textureMode=="post-false"||textureMode=="failed-device")?textureMode:"normal";std::cout<<Run(mode,&directory,&payload,argv[2],"0.25",std::string(argv[1])=="--texture-generated",true,&mesh,nullptr,{},{},&texture,textureMode)<<'\n';return 0;}
        if(argc==3&&std::string(argv[1])=="--generated"){std::string directory,payload;std::cin>>directory>>payload;std::cout<<Run(argv[2],&directory,&payload,nullptr,{},true)<<'\n';return 0;}
        for(const auto* mode:{"normal","pre-false","post-false","failed-device","weights-zero"})(void)Run(mode);
        for(const auto* mode:{"queued","depth-only","queue-disabled","capacity","already-flushing","alpha-disabled","object-disabled","zero-blend","priority-wrap"})(void)RunAlpha(mode);
        for(const auto* mode:{"three","failed-device","preserve-lights","append-during-pre","already-active","empty"})(void)RunAlphaFlush(mode);
        std::cout<<"PASS "<<checks<<'/'<<checks<<": complete unlit Skin palette/material/draw caller\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
