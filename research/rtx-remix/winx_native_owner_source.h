// Own operation-scoped ownership audit. Native callbacks, queues and draws
// execute unchanged. Borrowed identities below are not persistent instances.
#pragma once
#include "../../Sparkplug/Analysis/PC/SparkplugAbi.h"
namespace native_owner_source {
namespace abi=sparkplug::evidence::pc;
struct Packet {
  uint32_t scene=0,system=0,root=0,camera=0,support=0,object=0,model=0,mesh=0,renderer=0,frame=0;
  uint32_t modelMaterial=0,selectedMaterial=0,registrations=0,modelOccurrences=0,firstModelOrdinal=0;
  uint32_t ownerKind=0,ownerPrimary=0;
  uint64_t sceneScope=0,mutation=0,modelCall=0,supportCall=0,submission=0,geometryGeneration=0;
  D3DMATRIX world{};
  bool valid=false;
};
static bool enabled;
static FILE* output;
static unsigned supportCalls,queueSupportCalls,modelCalls,modelWithoutMesh,modelFailures;
static unsigned captures,qualified,used,sceneRetirements,nodeRetirements,renderNodeRetirements;
enum Reason:unsigned { NoScope,Callsite,Scene,Registry,Support,Model,Mesh,World,Retired,Count };
static unsigned rejected[Count]{};
static bool CanLog(){return output&&_ftelli64(output)<16*1024*1024;}
static bool Reject(Reason reason){++rejected[reason];return false;}
#if defined(_M_IX86)
static DWORD ownerThread;
static uint64_t nextSupportCall,nextModelCall;
#if defined(WINX_REMIX_TEST)
// The CPU fixture supplies owned DWORD slots. Literal addresses may already
// contain an OS mapping in that process; never replace somebody else's view.
static uintptr_t enginePointerAddress=0x755274,rendererPointerAddress=0x75db68;
#else
static constexpr uintptr_t enginePointerAddress=0x755274,rendererPointerAddress=0x75db68;
#endif
template<class T> static bool Read(uintptr_t address,T& value){return scene_geometry::Read(address,&value,sizeof(value));}
struct SupportScope;
struct ModelScope;
static thread_local SupportScope* activeSupport;
static thread_local ModelScope* activeModel;
struct SupportScope {
  SupportScope* parent=activeSupport;
  uint32_t support,camera;uint64_t sequence=++nextSupportCall;
  SupportScope(uint32_t s,uint32_t c):support(s),camera(c){activeSupport=this;}
  ~SupportScope(){activeSupport=parent;}
};
struct ModelScope {
  ModelScope* parent=activeModel;
  uint32_t model,camera,support,frame=frameId;uint64_t sequence=++nextModelCall;
  uint64_t sceneScope=scene_geometry::active?scene_geometry::active->serial:0;
  unsigned ownMeshes=0;
  ModelScope(uint32_t m,uint32_t c,uint32_t s):model(m),camera(c),support(s){activeModel=this;}
  ~ModelScope(){activeModel=parent;}
};
using NativeDraw=uint32_t(__thiscall*)(void*,uint32_t,uint32_t);
using NativeDestroy=void(__thiscall*)(void*);
static NativeDraw originalStatic,originalPartition,originalModel,originalRenderNode;
static NativeDestroy originalSceneDestroy,originalNodeDestroy,originalRenderNodeDestroy;
static uint32_t SupportAndRestore(SupportScope* scope,NativeDraw original,void* object,uint32_t camera,uint32_t force) {
  uint32_t result=0;
  __try {result=original(object,camera,force);}
  __finally {activeSupport=scope->parent;}
  return result;
}
static uint32_t SupportDraw(NativeDraw original,void* object,uint32_t camera,uint32_t force) {
  if(!enabled||GetCurrentThreadId()!=ownerThread)return original(object,camera,force);
  SupportScope scope(uint32_t(reinterpret_cast<uintptr_t>(object)),camera);++supportCalls;
  const auto renderer=scene_geometry::Word(rendererPointerAddress);
  uint8_t queue=0;if(renderer&&Read(renderer+0xc050,queue)&&queue)++queueSupportCalls;
  return SupportAndRestore(&scope,original,object,camera,force);
}
static uint32_t __fastcall StaticDraw(void* object,void*,uint32_t camera,uint32_t force){return SupportDraw(originalStatic,object,camera,force);}
static uint32_t __fastcall PartitionDraw(void* object,void*,uint32_t camera,uint32_t force){return SupportDraw(originalPartition,object,camera,force);}
static uint32_t __fastcall RenderNodeDraw(void* object,void*,uint32_t camera,uint32_t force){return SupportDraw(originalRenderNode,object,camera,force);}
static uint32_t ModelAndRestore(ModelScope* scope,void* model,uint32_t camera,uint32_t support) {
  uint32_t result=0;
  __try {result=originalModel(model,camera,support);}
  __finally {activeModel=scope->parent;}
  return result;
}
static uint32_t __fastcall ModelDraw(void* model,void*,uint32_t camera,uint32_t support) {
  if(!enabled||GetCurrentThreadId()!=ownerThread)return originalModel(model,camera,support);
  ModelScope scope(uint32_t(reinterpret_cast<uintptr_t>(model)),camera,support);++modelCalls;
  const auto result=ModelAndRestore(&scope,model,camera,support);
  if(!scope.ownMeshes)++modelWithoutMesh;
  if(!(result&255))++modelFailures;
  return result;
}
static void Retire(uint32_t address,bool scene,bool renderNode=false) {
  // A known retirement invalidates cached graph evidence even if another
  // thread initiates it. No native pointer is read during/after destruction.
  const auto mutation=scene_geometry::AdvanceMutationSerial();
  if(GetCurrentThreadId()!=ownerThread)return;
  if(scene)++sceneRetirements;else if(renderNode)++renderNodeRetirements;else ++nodeRetirements;
  if(CanLog())fprintf(output,"{\"event\":\"retire\",\"frame\":%u,\"kind\":\"%s\",\"address\":%u,\"mutation\":%llu}\n",
    frameId,scene?"scene":renderNode?"render_node":"partition_node",address,mutation);
}
static void __fastcall SceneDestroy(void* scene,void*) {
  if(enabled)Retire(uint32_t(reinterpret_cast<uintptr_t>(scene)),true);
  originalSceneDestroy(scene);
}
static void __fastcall NodeDestroy(void* node,void*) {
  if(enabled)Retire(uint32_t(reinterpret_cast<uintptr_t>(node)),false);
  originalNodeDestroy(node);
}
static void __fastcall RenderNodeDestroy(void* node,void*) {
  if(enabled)Retire(uint32_t(reinterpret_cast<uintptr_t>(node)),false,true);
  originalRenderNodeDestroy(node);
}
static bool SupportIdentity(const Packet& packet,const abi::spRenderSupportObservedLayout& support) {
  if(support.completeObject!=packet.object)return false;
  if(support.vtable==abi::spRenderNodeSupportVTable) {
    // Inherited support is the boundary; derived primary vtables remain valid.
    // Enabled is logical visibility. Never replace it with the camera stamp.
    return packet.support==packet.object+0xb4&&scene_geometry::Word(packet.object+0x3c)==packet.scene&&
      (scene_geometry::Word(packet.object+0xb0)&0x200)&&support.worldMatrixPointer==packet.object+0x138&&
      support.inverseMatrixPointer==packet.object+0x178;
  }
  uint32_t adjusted=0;
  return (support.vtable==0x6f4528||support.vtable==0x6e6604)&&
    scene_geometry::Support(packet.object,packet.scene,support.vtable==0x6f4528,adjusted)&&adjusted==packet.support;
}
static bool Current(const Packet& packet) {
  const auto scope=scene_geometry::active;
  if(!scope||scope->parent||scope->serial!=packet.sceneScope||scope->scene!=packet.scene||
     scope->camera!=packet.camera||frameId!=packet.frame||scene_geometry::MutationSerial()!=packet.mutation)return false;
  const auto engine=scene_geometry::Word(enginePointerAddress);
  return engine&&scene_geometry::Word(engine+0x18)==packet.scene&&scene_geometry::Word(engine+0x1c)==packet.camera&&
    scene_geometry::Word(packet.scene+0x38)==packet.system&&scene_geometry::Word(packet.system+0x1d4)==packet.root&&
    scene_geometry::MutationSerial()==packet.mutation;
}
static bool Capture(uint32_t mesh,uint32_t renderer,uint64_t submission,uintptr_t returnAddress,Packet& out) {
  out={};if(!enabled)return false;
  if(GetCurrentThreadId()!=ownerThread||!activeModel)return Reject(NoScope);
  if(returnAddress!=0x479df3)return Reject(Callsite);
  auto& modelScope=*activeModel;++modelScope.ownMeshes;
  const auto scope=scene_geometry::active;
  if(!scope||scope->parent||scope->serial!=modelScope.sceneScope||modelScope.frame!=frameId||
     scope->camera!=modelScope.camera)return Reject(Scene);
  const scene_geometry::Registry* registry=nullptr;
  try {registry=&scope->RegistrySnapshot();}catch(const std::bad_alloc&){return Reject(Registry);}
  if(!registry->valid||!registry->occurrencesComplete||registry->mutationSerial!=scene_geometry::MutationSerial())return Reject(Registry);
  Packet packet{};packet.scene=scope->scene;packet.system=registry->system;packet.root=registry->root;
  packet.camera=modelScope.camera;packet.support=modelScope.support;packet.model=modelScope.model;
  packet.mesh=mesh;packet.renderer=renderer;packet.frame=frameId;packet.sceneScope=scope->serial;
  packet.mutation=registry->mutationSerial;packet.modelCall=modelScope.sequence;packet.submission=submission;
  if(!Current(packet))return Reject(Scene);
  abi::spCameraObservedLayout camera{};
  if(!Read(packet.camera,camera)||camera.projectionBranch||camera.twoDimensional||
     camera.projectionMatrix[11]!=1.0f||camera.projectionMatrix[15]!=0.0f)return Reject(Scene);
  abi::spRenderSupportObservedLayout support{};abi::spModelLayout model{};
  if(!Read(packet.support,support))return Reject(Support);
  packet.object=support.completeObject;
  if(!SupportIdentity(packet,support))return Reject(Support);
  packet.ownerKind=support.vtable==abi::spRenderNodeSupportVTable?2:support.vtable==0x6f4528?1:0;
  packet.ownerPrimary=scene_geometry::Word(packet.object);if(!packet.ownerPrimary)return Reject(Support);
  for(const auto& occurrence:registry->occurrences)
    if(occurrence.support==packet.support&&occurrence.object==packet.object)++packet.registrations;
  if(!packet.registrations)return Reject(Registry);
  if(!Read(packet.model,model)||scene_geometry::Word(packet.model)!=0x6eaa58)return Reject(Model);
  if(model.baseMeshData!=mesh||scene_geometry::Word(rendererPointerAddress)!=renderer||
     scene_geometry::Word(renderer)!=abi::spPCRendererPrimaryVTable)return Reject(Mesh);
  if(support.renderableBegin>support.renderableEnd||support.renderableEnd>support.renderableCapacity||
     (support.renderableEnd-support.renderableBegin)%4||support.renderableEnd-support.renderableBegin>4096*4)return Reject(Support);
  const auto count=(support.renderableEnd-support.renderableBegin)/4;
  for(uint32_t i=0;i<count;++i){uint32_t value=0;if(!Read(support.renderableBegin+i*4,value))return Reject(Support);
    if(value==packet.model){if(!packet.modelOccurrences)packet.firstModelOrdinal=i;++packet.modelOccurrences;}}
  if(!packet.modelOccurrences)return Reject(Model);
  if(!Read(support.worldMatrixPointer,packet.world))return Reject(World);
  for(const auto& row:packet.world.m)for(float value:row)if(!std::isfinite(value))return Reject(World);
  packet.modelMaterial=model.base.material;
  if(activeSupport&&activeSupport->support==packet.support&&activeSupport->camera==packet.camera)packet.supportCall=activeSupport->sequence;
  if(!Current(packet))return Reject(Retired);
  packet.valid=true;out=packet;++captures;return true;
}
static bool Qualify(Packet& packet,uint32_t mesh,uint32_t renderer,uint64_t submission,const D3DMATRIX& world) {
  if(!enabled||!packet.valid)return false;
  if(GetCurrentThreadId()!=ownerThread||!Current(packet)||!activeModel||activeModel->sequence!=packet.modelCall)return Reject(Retired);
  if(packet.mesh!=mesh||packet.renderer!=renderer||packet.submission!=submission||scene_geometry::Word(packet.model+0x58)!=mesh)return Reject(Mesh);
  abi::spRenderSupportObservedLayout support{};D3DMATRIX current{};
  if(!Read(packet.support,support)||!SupportIdentity(packet,support)||scene_geometry::Word(packet.object)!=packet.ownerPrimary||
     scene_geometry::Word(packet.model)!=0x6eaa58)return Reject(Support);
  if(support.renderableBegin>support.renderableEnd||support.renderableEnd>support.renderableCapacity||
     (support.renderableEnd-support.renderableBegin)%4||support.renderableEnd-support.renderableBegin>4096*4)return Reject(Support);
  unsigned occurrences=0,first=0;
  for(uint32_t i=0;i<(support.renderableEnd-support.renderableBegin)/4;++i){uint32_t value=0;
    if(!Read(support.renderableBegin+i*4,value))return Reject(Support);
    if(value==packet.model){if(!occurrences)first=i;++occurrences;}}
  if(!occurrences||occurrences!=packet.modelOccurrences||first!=packet.firstModelOrdinal)return Reject(Model);
  if(!Read(support.worldMatrixPointer,current)||
     memcmp(&current,&packet.world,sizeof(current))||memcmp(&world,&packet.world,sizeof(world)))return Reject(World);
  packet.selectedMaterial=scene_geometry::Word(renderer+abi::spRendererDrawContextOffset+
    offsetof(abi::spRendererDrawContextObservedLayout,selectedMaterial));
  if(!Current(packet))return Reject(Retired);
  ++qualified;return true;
}
struct Patch {
  uintptr_t address;unsigned size;unsigned char expected[7],replacement[7];DWORD protection=0;
};
static bool Install() {
  Patch patches[7]={{0x6e6604,4},{0x6f4528,4},{0x6eaa7c,4},{0x6dcadc,4},{0x45e5d0,7},{0x4264d0,7},{0x425050,7}};
  const uint32_t entries[4]={0x44fc00,0x4d72c0,0x479dc0,0x424b60};
  const uintptr_t hooks[7]={reinterpret_cast<uintptr_t>(&StaticDraw),reinterpret_cast<uintptr_t>(&PartitionDraw),
    reinterpret_cast<uintptr_t>(&ModelDraw),reinterpret_cast<uintptr_t>(&RenderNodeDraw),reinterpret_cast<uintptr_t>(&SceneDestroy),
    reinterpret_cast<uintptr_t>(&NodeDestroy),reinterpret_cast<uintptr_t>(&RenderNodeDestroy)};
  const unsigned char prefixes[3][7]={{0x6a,0xff,0x68,0x84,0x1f,0x6c,0},{0x6a,0xff,0x68,0x3a,0xfd,0x6b,0},{0x6a,0xff,0x68,0x3f,0xfc,0x6b,0}};
  unsigned char observed[7]{};
  for(unsigned i=0;i<7;++i){auto& p=patches[i];memcpy(p.expected,i<4?reinterpret_cast<const void*>(&entries[i]):prefixes[i-4],p.size);
    if(!scene_geometry::Read(p.address,observed,p.size)||memcmp(observed,p.expected,p.size))return false;}
  unsigned char* trampolines[3]{};
  for(unsigned i=0;i<3;++i){
    auto& t=trampolines[i];t=static_cast<unsigned char*>(VirtualAlloc(nullptr,12,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!t){for(auto allocated:trampolines)if(allocated)VirtualFree(allocated,0,MEM_RELEASE);return false;}
    memcpy(t,prefixes[i],7);t[7]=0xe9;const auto jump=uint32_t(patches[i+4].address+7-reinterpret_cast<uintptr_t>(t+12));memcpy(t+8,&jump,4);
    DWORD previous=0;if(!VirtualProtect(t,12,PAGE_EXECUTE_READ,&previous)){
      for(auto allocated:trampolines)if(allocated)VirtualFree(allocated,0,MEM_RELEASE);return false;}
    FlushInstructionCache(GetCurrentProcess(),t,12);
  }
  unsigned writable=0;
  for(;writable<7;++writable){auto& p=patches[writable];
    if(!VirtualProtect(reinterpret_cast<void*>(p.address),p.size,PAGE_EXECUTE_READWRITE,&p.protection))break;}
  if(writable!=7){DWORD ignored=0;while(writable){auto& p=patches[--writable];VirtualProtect(reinterpret_cast<void*>(p.address),p.size,p.protection,&ignored);}
    for(auto t:trampolines)VirtualFree(t,0,MEM_RELEASE);return false;}
  originalStatic=reinterpret_cast<NativeDraw>(entries[0]);originalPartition=reinterpret_cast<NativeDraw>(entries[1]);originalModel=reinterpret_cast<NativeDraw>(entries[2]);
  originalRenderNode=reinterpret_cast<NativeDraw>(entries[3]);
  originalSceneDestroy=reinterpret_cast<NativeDestroy>(trampolines[0]);originalNodeDestroy=reinterpret_cast<NativeDestroy>(trampolines[1]);
  originalRenderNodeDestroy=reinterpret_cast<NativeDestroy>(trampolines[2]);
  for(unsigned i=0;i<7;++i){auto& p=patches[i];if(i<4){const auto pointer=uint32_t(hooks[i]);memcpy(p.replacement,&pointer,4);}
    else{memset(p.replacement,0x90,7);p.replacement[0]=0xe9;const auto jump=uint32_t(hooks[i]-p.address-5);memcpy(p.replacement+1,&jump,4);}
    if(i<4)InterlockedExchangePointer(reinterpret_cast<void* volatile*>(p.address),reinterpret_cast<void*>(hooks[i]));
    else memcpy(reinterpret_cast<void*>(p.address),p.replacement,p.size);
  }
  // Some patches share a page. Restore protections in reverse acquisition order.
  for(unsigned i=7;i;--i){auto& p=patches[i-1];DWORD ignored=0;VirtualProtect(reinterpret_cast<void*>(p.address),p.size,p.protection,&ignored);
    FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(p.address),p.size);}
  return true;
}
#else
static bool Qualify(Packet&,uint32_t,uint32_t,uint64_t,const D3DMATRIX&){return false;}
#endif
static void RecordUse(const Packet& packet,bool nativeMaterial) {
  if(!packet.valid)return;++used;
  if(CanLog()&&(frameId%300==0||frameId+120==traceUntilFrame)) {
    fprintf(output,"{\"event\":\"submit\",\"frame\":%u,\"draw\":%u,\"scene\":%u,\"system\":%u,\"root\":%u,\"camera\":%u,\"support\":%u,\"object\":%u,\"model\":%u,\"mesh\":%u,\"modelMaterial\":%u,\"selectedMaterial\":%u,\"registrations\":%u,\"modelOccurrences\":%u,\"firstModelOrdinal\":%u,\"sceneScope\":%llu,\"mutation\":%llu,\"modelCall\":%llu,\"supportCall\":%llu,\"submission\":%llu,\"geometryGeneration\":%llu,\"nativeMaterial\":%s,\"ownerKind\":%u,\"ownerPrimary\":%u,\"worldBits\":[",
      frameId,drawId,packet.scene,packet.system,packet.root,packet.camera,packet.support,packet.object,packet.model,packet.mesh,packet.modelMaterial,packet.selectedMaterial,
      packet.registrations,packet.modelOccurrences,packet.firstModelOrdinal,packet.sceneScope,packet.mutation,packet.modelCall,packet.supportCall,packet.submission,packet.geometryGeneration,nativeMaterial?"true":"false",packet.ownerKind,packet.ownerPrimary);
    uint32_t words[16]{};memcpy(words,&packet.world,sizeof(words));
    for(unsigned i=0;i<16;++i)fprintf(output,"%s%u",i?",":"",words[i]);fputs("]}\n",output);
  }
}
static void Initialize() {
  wchar_t path[MAX_PATH]{};const auto length=GetEnvironmentVariableW(L"WINX_REMIX_NATIVE_OWNER_SOURCE",path,MAX_PATH);
  if(!length||length>=MAX_PATH)return;
#if defined(_M_IX86)
  output=_wfsopen(path,L"wb",_SH_DENYNO);
  if(!scene_audit::VerifiedImage()) {
    if(CanLog()){fputs("{\"event\":\"init\",\"schema\":1,\"enabled\":false,\"reason\":\"image_guard\",\"maxLogBytes\":16777216}\n",output);fflush(output);}
    return;
  }
  ownerThread=GetCurrentThreadId();enabled=Install();
  if(CanLog()){fprintf(output,"{\"event\":\"init\",\"schema\":1,\"enabled\":%s,\"maxLogBytes\":16777216,\"scope\":\"exact static/partition and inherited RenderNode model calls; operation-scoped borrowed identities; no persistent instance replay\"}\n",enabled?"true":"false");fflush(output);}
#endif
}
static void EndFrame() {
  bool hasRejections=false;for(auto value:rejected)hasRejections=hasRejections||value!=0;
  if(CanLog()&&(hasRejections||supportCalls||modelCalls||captures||sceneRetirements||nodeRetirements||renderNodeRetirements||frameId%300==0)) {
    fprintf(output,"{\"event\":\"frame\",\"frame\":%u,\"supportCalls\":%u,\"queueSupportCalls\":%u,\"modelCalls\":%u,\"modelWithoutMesh\":%u,\"modelFailures\":%u,\"captures\":%u,\"qualified\":%u,\"used\":%u,\"sceneRetirements\":%u,\"nodeRetirements\":%u,\"renderNodeRetirements\":%u,\"rejected\":[",
      frameId,supportCalls,queueSupportCalls,modelCalls,modelWithoutMesh,modelFailures,captures,qualified,used,sceneRetirements,nodeRetirements,renderNodeRetirements);
    for(unsigned i=0;i<Count;++i)fprintf(output,"%s%u",i?",":"",rejected[i]);fputs("]}\n",output);fflush(output);
  }
  supportCalls=queueSupportCalls=modelCalls=modelWithoutMesh=modelFailures=captures=qualified=used=sceneRetirements=nodeRetirements=renderNodeRetirements=0;
  memset(rejected,0,sizeof(rejected));
}
}
