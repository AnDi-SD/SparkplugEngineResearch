// Own bounded observer of the original Skin palette at its own mesh call.
// No producer, cache update, geometry submission or persistent instance replay.
#pragma once
#include "../../Sparkplug/Analysis/PC/spNodeTransformMath.h"
namespace native_skin_source { struct Observation; }
namespace native_skin_vertex_source { static void Capture(const native_skin_source::Observation&); }
namespace native_camera_source { static bool KnownCamera(uint32_t); }
namespace native_skin_source {
namespace abi=sparkplug::evidence::pc;
namespace math=abi::node_math;
static bool enabled;
static FILE* output;
static unsigned calls,callFailures,withoutMesh,attempts,matched,mismatches,bonesCompared,bitDifferences;
static double maxAbsoluteError,maxRelativeError;
enum Reason:unsigned {NoScope,Callsite,Scene,Registry,Support,Skin,Mesh,Palette,Bone,Nonfinite,Changed,Count};
static const char* reasonNames[Count]={"scope","callsite","scene","registry","support","skin","mesh","palette","bone","nonfinite","changed"};
static unsigned rejected[Count]{};
static constexpr unsigned maxBones=256;
static constexpr double absoluteTolerance=0.0001,relativeTolerance=0.00001;
static bool CanLog(){return output&&_ftelli64(output)<16*1024*1024;}
static bool Reject(Reason reason){++rejected[reason];return false;}
#if defined(_M_IX86)
static DWORD ownerThread;
static uint64_t nextCall;
using NativeDraw=uint32_t(__thiscall*)(void*,uint32_t,uint32_t);
static NativeDraw originalSkin;
#if defined(WINX_REMIX_TEST)
static uintptr_t renderSlot=abi::spSkinVTable+0x24,renderEntry=abi::spSkinRender;
#else
static constexpr uintptr_t renderSlot=abi::spSkinVTable+0x24,renderEntry=abi::spSkinRender;
#endif
template<class T> static bool Read(uintptr_t address,T& value){return scene_geometry::Read(address,&value,sizeof(value));}
struct Scope;
static thread_local Scope* active;
struct Scope {
  Scope* parent=active;
  uint32_t skin,camera,support,frame=frameId;
  uint64_t sequence=++nextCall,sceneScope=scene_geometry::active?scene_geometry::active->serial:0;
  uint64_t mutation=scene_geometry::MutationSerial();
  unsigned ownMeshes=0;
  Scope(uint32_t s,uint32_t c,uint32_t p):skin(s),camera(c),support(p){active=this;}
  ~Scope(){active=parent;}
};
static uint32_t __fastcall SkinDraw(void*,void*,uint32_t,uint32_t);
static bool Installed(){return scene_geometry::Word(renderSlot)==uint32_t(reinterpret_cast<uintptr_t>(&SkinDraw));}
static bool Current(const Scope& scope,const native_owner_source::Packet& owner) {
  return enabled&&GetCurrentThreadId()==ownerThread&&active==&scope&&!scope.parent&&
    scope.frame==frameId&&scope.sceneScope==owner.sceneScope&&scope.mutation==owner.mutation&&
    native_owner_source::Current(owner)&&Installed()&&scene_geometry::MutationSerial()==scope.mutation;
}
struct BoneInput {uint32_t address;abi::spNodeLayout node;math::Matrix4 inverseBind,palette;};
struct Observation {
  uint32_t scene=0,skin=0,support=0,camera=0,mesh=0,renderer=0,palette=0,boneCount=0,weightHint=0,meshWeights=0;
  uint64_t modelCall=0,submission=0;
  unsigned wordsDifferent=0;
  double absoluteError=0,relativeError=0;
  bool withinTolerance=true;
};
template<class T> static bool Finite(const T& values){for(float value:values)if(!std::isfinite(value))return false;return true;}
static bool Observe(uint32_t mesh,uint32_t renderer,uint64_t submission,uintptr_t returnAddress,Observation& out) {
  out={};++attempts;
  if(!active||active->parent||GetCurrentThreadId()!=ownerThread)return Reject(NoScope);
  if(returnAddress!=0x46a367)return Reject(Callsite);
  auto& scope=*active;++scope.ownMeshes;
  const auto scene=scene_geometry::active;
  if(!scene||scene->parent||scene->serial!=scope.sceneScope||scene->camera!=scope.camera||scope.frame!=frameId)return Reject(Scene);
  const scene_geometry::Registry* registry=nullptr;
  try {registry=&scene->RegistrySnapshot();}catch(const std::bad_alloc&){return Reject(Registry);}
  if(!registry->valid||!registry->occurrencesComplete||registry->mutationSerial!=scope.mutation)return Reject(Registry);
  native_owner_source::Packet owner{};owner.scene=scene->scene;owner.camera=scope.camera;owner.sceneScope=scope.sceneScope;
  owner.frame=scope.frame;owner.system=registry->system;owner.root=registry->root;owner.mutation=scope.mutation;
  owner.support=scope.support;
  if(!Current(scope,owner))return Reject(Scene);
  abi::spCameraObservedLayout camera{};
  if(!Read(scope.camera,camera)||!native_camera_source::KnownCamera(scene_geometry::Word(scope.camera))||camera.projectionBranch||
     camera.twoDimensional||camera.projectionMatrix[11]!=1.0f||camera.projectionMatrix[15]!=0.0f)return Reject(Scene);
  abi::spRenderSupportObservedLayout support{};
  if(!Read(scope.support,support))return Reject(Support);
  owner.object=support.completeObject;
  if(!native_owner_source::SupportIdentity(owner,support))return Reject(Support);
  const auto primary=scene_geometry::Word(owner.object);if(!primary)return Reject(Support);
  bool registered=false;for(const auto& occurrence:registry->occurrences)
    if(occurrence.support==scope.support&&occurrence.object==owner.object){registered=true;break;}
  if(!registered)return Reject(Registry);
  if(support.renderableBegin>support.renderableEnd||support.renderableEnd>support.renderableCapacity||
     (support.renderableEnd-support.renderableBegin)%4||support.renderableEnd-support.renderableBegin>4096*4)return Reject(Support);
  bool related=false;
  for(uint32_t i=0;i<(support.renderableEnd-support.renderableBegin)/4;++i){uint32_t value=0;
    if(!Read(support.renderableBegin+i*4,value))return Reject(Support);if(value==scope.skin)related=true;}
  if(!related)return Reject(Skin);
  abi::spSkinObservedLayout skin{};abi::spDXMeshObservedLayout meshHeader{};
  if(!Read(scope.skin,skin)||scene_geometry::Word(scope.skin)!=abi::spSkinVTable)return Reject(Skin);
  if(skin.base.baseMeshData!=mesh||!Read(mesh,meshHeader)||scene_geometry::Word(mesh)!=abi::spDXMeshVTable||
     meshHeader.base.base.secondaryVTable!=abi::spDXMeshInterfaceVTable||
     scene_geometry::Word(native_owner_source::rendererPointerAddress)!=renderer||
     scene_geometry::Word(renderer)!=abi::spPCRendererPrimaryVTable)return Reject(Mesh);
  uint32_t palette[2]{};
  if(!Read(renderer+0xc9b8,palette)||!skin.boneCount||skin.boneCount>maxBones||palette[1]!=skin.boneCount||
     !skin.bones||!skin.inverseBindMatrices||!palette[0]||
     uint64_t(skin.bones)+skin.boneCount*4>=0x7fff0000||
     uint64_t(skin.inverseBindMatrices)+skin.boneCount*64>=0x7fff0000||
     uint64_t(palette[0])+skin.boneCount*64>=0x7fff0000)return Reject(Palette);
  // Stack storage is bounded to 256 records. Each native read remains <=16KiB.
  BoneInput inputs[maxBones]{};
  Observation value{};value.scene=scene->scene;value.skin=scope.skin;value.support=scope.support;value.camera=scope.camera;
  value.mesh=mesh;value.renderer=renderer;value.palette=palette[0];value.boneCount=skin.boneCount;
  value.weightHint=skin.weightCount;value.meshWeights=meshHeader.componentWeightCount;value.modelCall=scope.sequence;value.submission=submission;
  for(unsigned i=0;i<skin.boneCount;++i){auto& b=inputs[i];
    if(!Read(skin.bones+i*4,b.address)||!Read(b.address,b.node)||scene_geometry::Word(b.address)!=abi::spNodeVTable||
       b.node.sceneLink!=owner.scene||(b.node.flags&15))return Reject(Bone);
    if(!Read(skin.inverseBindMatrices+i*64,b.inverseBind)||!Read(palette[0]+i*64,b.palette))return Reject(Palette);
    if(!Finite(b.node.cachedWorldPosition)||!Finite(b.node.cachedWorldScale)||!Finite(b.node.cachedWorldOrientation)||
       !Finite(b.inverseBind)||!Finite(b.palette))return Reject(Nonfinite);
    math::Vector3 position{},scale{};math::Matrix3 orientation{};
    memcpy(position.data(),b.node.cachedWorldPosition,sizeof(position));memcpy(scale.data(),b.node.cachedWorldScale,sizeof(scale));
    memcpy(orientation.data(),b.node.cachedWorldOrientation,sizeof(orientation));
    const auto world=math::Affine(position,orientation,scale);
    const auto computed=math::Multiply4ForAnalysis(b.inverseBind,world);
    if(!Finite(world)||!Finite(computed))return Reject(Nonfinite);
    for(unsigned j=0;j<16;++j){
      const double difference=std::fabs(double(computed[j])-double(b.palette[j]));
      const double magnitude=(std::max)(std::fabs(double(computed[j])),std::fabs(double(b.palette[j])));
      value.absoluteError=(std::max)(value.absoluteError,difference);
      value.relativeError=(std::max)(value.relativeError,difference/(std::max)(magnitude,1e-30));
      if(memcmp(&computed[j],&b.palette[j],4))++value.wordsDifferent;
      if(difference>absoluteTolerance+relativeTolerance*magnitude)value.withinTolerance=false;
    }
  }
  // Re-read every borrowed input after arithmetic. No producer/COM call occurs
  // between these reads; equality is operation evidence, not untracked ABA proof.
  abi::spSkinObservedLayout finalSkin{};abi::spDXMeshObservedLayout finalMesh{};abi::spRenderSupportObservedLayout finalSupport{};
  uint32_t finalPalette[2]{};
  if(!Read(scope.skin,finalSkin)||memcmp(&skin,&finalSkin,sizeof(skin))||
     !Read(mesh,finalMesh)||memcmp(&meshHeader,&finalMesh,sizeof(meshHeader))||
     !Read(scope.support,finalSupport)||memcmp(&support,&finalSupport,sizeof(support))||scene_geometry::Word(owner.object)!=primary||
     !Read(renderer+0xc9b8,finalPalette)||memcmp(palette,finalPalette,sizeof(palette)))return Reject(Changed);
  for(unsigned i=0;i<skin.boneCount;++i){BoneInput final{};
    if(!Read(skin.bones+i*4,final.address)||!Read(final.address,final.node)||
       !Read(skin.inverseBindMatrices+i*64,final.inverseBind)||!Read(palette[0]+i*64,final.palette)||
       memcmp(&inputs[i],&final,sizeof(final)))return Reject(Changed);
  }
  if(scene_geometry::Word(native_owner_source::rendererPointerAddress)!=renderer||
     scene_geometry::Word(renderer)!=abi::spPCRendererPrimaryVTable||!Current(scope,owner))return Reject(Changed);
  out=value;return true;
}
static void Capture(uint32_t mesh,uint32_t renderer,uint64_t submission,uintptr_t returnAddress) {
  if(!enabled||GetCurrentThreadId()!=ownerThread||!active)return;
  Observation value{};if(!Observe(mesh,renderer,submission,returnAddress,value))return;
  if(value.withinTolerance)++matched;else ++mismatches;
  if(value.withinTolerance)native_skin_vertex_source::Capture(value);
  bonesCompared+=value.boneCount;bitDifferences+=value.wordsDifferent;
  maxAbsoluteError=(std::max)(maxAbsoluteError,value.absoluteError);maxRelativeError=(std::max)(maxRelativeError,value.relativeError);
  if(CanLog()&&(!value.withinTolerance||frameId%300==0||triggered))
    fprintf(output,"{\"event\":\"compare\",\"frame\":%u,\"scene\":%u,\"skin\":%u,\"support\":%u,\"camera\":%u,\"mesh\":%u,\"renderer\":%u,\"palette\":%u,\"boneCount\":%u,\"weightHint\":%u,\"meshWeights\":%u,\"modelCall\":%llu,\"submission\":%llu,\"withinTolerance\":%s,\"bitDifferences\":%u,\"maxAbsoluteError\":%.17g,\"maxRelativeError\":%.17g}\n",
      frameId,value.scene,value.skin,value.support,value.camera,value.mesh,value.renderer,value.palette,value.boneCount,value.weightHint,value.meshWeights,value.modelCall,value.submission,value.withinTolerance?"true":"false",value.wordsDifferent,value.absoluteError,value.relativeError);
}
static uint32_t DrawAndRestore(Scope* scope,void* skin,uint32_t camera,uint32_t support) {
  uint32_t result=0;__try {result=originalSkin(skin,camera,support);}
  __finally {active=scope->parent;}return result;
}
static uint32_t __fastcall SkinDraw(void* skin,void*,uint32_t camera,uint32_t support) {
  if(!enabled||GetCurrentThreadId()!=ownerThread)return originalSkin(skin,camera,support);
  Scope scope(uint32_t(reinterpret_cast<uintptr_t>(skin)),camera,support);++calls;
  const auto result=DrawAndRestore(&scope,skin,camera,support);
  if(!(result&255))++callFailures;if(!scope.ownMeshes)++withoutMesh;return result;
}
static bool Install() {
  if(renderSlot%4||scene_geometry::Word(renderSlot)!=renderEntry)return false;
  DWORD previous=0,ignored=0;if(!VirtualProtect(reinterpret_cast<void*>(renderSlot),4,PAGE_READWRITE,&previous))return false;
  originalSkin=reinterpret_cast<NativeDraw>(renderEntry);ownerThread=GetCurrentThreadId();
  const auto before=InterlockedCompareExchangePointer(reinterpret_cast<void* volatile*>(renderSlot),reinterpret_cast<void*>(&SkinDraw),reinterpret_cast<void*>(renderEntry));
  VirtualProtect(reinterpret_cast<void*>(renderSlot),4,previous,&ignored);
  return reinterpret_cast<uintptr_t>(before)==renderEntry&&Installed();
}
#else
static void Capture(uint32_t,uint32_t,uint64_t,uintptr_t){}
#endif
static void Initialize() {
  wchar_t path[MAX_PATH]{};const auto length=GetEnvironmentVariableW(L"WINX_REMIX_NATIVE_SKIN_SOURCE",path,MAX_PATH);
  if(!length||length>=MAX_PATH)return;
#if defined(_M_IX86)
  output=_wfsopen(path,L"wb",_SH_DENYNO);const bool image=scene_audit::VerifiedImage();enabled=image&&Install();
  if(CanLog()){fprintf(output,"{\"event\":\"init\",\"schema\":1,\"enabled\":%s,\"imageVerified\":%s,\"pid\":%u,\"maxLogBytes\":16777216,\"maxBones\":256,\"absoluteTolerance\":%.17g,\"relativeTolerance\":%.17g,\"rejectionNames\":[",enabled?"true":"false",image?"true":"false",GetCurrentProcessId(),absoluteTolerance,relativeTolerance);
    for(unsigned i=0;i<Count;++i)fprintf(output,"%s\"%s\"",i?",":"",reasonNames[i]);
    fputs("],\"scope\":\"own Skin mesh call; current plain Node cached world PRS; bounded tolerant palette observer, not weighted geometry submission or durable lifetime\"}\n",output);fflush(output);}
#endif
}
static void EndFrame() {
  if(CanLog()&&(calls||attempts||frameId%300==0)){
    fprintf(output,"{\"event\":\"frame\",\"frame\":%u,\"calls\":%u,\"callFailures\":%u,\"withoutMesh\":%u,\"attempts\":%u,\"matched\":%u,\"mismatches\":%u,\"bones\":%u,\"bitDifferences\":%u,\"maxAbsoluteError\":%.17g,\"maxRelativeError\":%.17g,\"rejected\":[",frameId,calls,callFailures,withoutMesh,attempts,matched,mismatches,bonesCompared,bitDifferences,maxAbsoluteError,maxRelativeError);
    for(unsigned i=0;i<Count;++i)fprintf(output,"%s%u",i?",":"",rejected[i]);fputs("]}\n",output);fflush(output);
  }
  calls=callFailures=withoutMesh=attempts=matched=mismatches=bonesCompared=bitDifferences=0;
  maxAbsoluteError=maxRelativeError=0;memset(rejected,0,sizeof(rejected));
}
} // namespace native_skin_source
