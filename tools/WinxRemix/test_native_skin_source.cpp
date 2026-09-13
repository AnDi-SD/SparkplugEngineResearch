// Own CPU ABI/observer fixture. No original entry point, COM device or GPU.
#define WINX_REMIX_TEST
#include "winx_d3d9_probe.cpp"
#include "winx_native_skin_source.h"
#include <stdexcept>
#include <limits>
namespace skin_test {
namespace source=native_skin_source;
namespace abi=sparkplug::evidence::pc;
static unsigned checks;
static void Check(bool value,const char* label){++checks;if(!value)throw std::runtime_error(label);}
static uint32_t Ptr(const void* p){return uint32_t(reinterpret_cast<uintptr_t>(p));}
static void Put(void* p,unsigned offset,uint32_t value){memcpy(static_cast<uint8_t*>(p)+offset,&value,4);}
struct Watchdog {
  HANDLE stop=CreateEventW(nullptr,TRUE,FALSE,nullptr),thread=nullptr;
  static DWORD WINAPI Wait(void* stop){if(WaitForSingleObject(static_cast<HANDLE>(stop),30000)==WAIT_TIMEOUT)TerminateProcess(GetCurrentProcess(),0xe0525b30u);return 0;}
  Watchdog(){Check(stop!=nullptr,"owned watchdog event");thread=CreateThread(nullptr,0,Wait,stop,0,nullptr);Check(thread!=nullptr,"owned watchdog thread");}
  ~Watchdog(){SetEvent(stop);WaitForSingleObject(thread,1000);CloseHandle(thread);CloseHandle(stop);}
};
struct Fixture {
  alignas(4) uint8_t engine[0x80]{},scene[0x54]{},system[0x1d8]{},root[0x84]{},object[0x1d8]{},renderer[0xf368]{},camera[0x238]{};
  abi::spSkinObservedLayout skin{};abi::spDXMeshObservedLayout mesh{};
  abi::spNodeLayout nodes[256]{};
  uint32_t nodePointers[256]{},objectPointers[1]{},renderables[3]{},engineSlot=0,rendererSlot=0,hookSlot=0;
  source::math::Matrix4 inverse[256]{},palette[256]{};
  uintptr_t oldEngine=native_owner_source::enginePointerAddress,oldRenderer=native_owner_source::rendererPointerAddress;
  uintptr_t oldSlot=source::renderSlot,oldEntry=source::renderEntry;
  Fixture(){
    native_owner_source::enginePointerAddress=reinterpret_cast<uintptr_t>(&engineSlot);engineSlot=Ptr(engine);
    native_owner_source::rendererPointerAddress=reinterpret_cast<uintptr_t>(&rendererSlot);rendererSlot=Ptr(renderer);
    source::renderSlot=reinterpret_cast<uintptr_t>(&hookSlot);
    Put(engine,0x18,Ptr(scene));Put(engine,0x1c,Ptr(camera));Put(scene,0,0x6e7358);Put(scene,0x38,Ptr(system));
    Put(system,0,0x6ec528);Put(system,0x1d4,Ptr(root));Put(root,0,0x6e4420);Put(root,0x80,Ptr(scene));
    objectPointers[0]=Ptr(object);Put(root,0x24,Ptr(objectPointers));Put(root,0x28,Ptr(objectPointers)+4);Put(root,0x2c,Ptr(objectPointers)+4);
    Put(object,0,0x10203040);Put(object,0x3c,Ptr(scene));Put(object,0xb0,0x200);Put(object,0xb4,0x6dcadc);Put(object,0x124,Ptr(object));
    Put(object,0xe8,Ptr(object)+0x138);Put(object,0xec,Ptr(object)+0x178);
    renderables[0]=renderables[2]=Ptr(&skin);renderables[1]=0;
    Put(object,0xbc,Ptr(renderables));Put(object,0xc0,Ptr(renderables)+12);Put(object,0xc4,Ptr(renderables)+12);
    Put(camera,0,0x6ef1e0);Put(camera,0x138,0x3f800000);Put(renderer,0,0x6f2918);
    Put(&skin,0,0x6e8c5c);skin.base.baseMeshData=Ptr(&mesh);skin.weightCount=4;skin.boneCount=1;
    skin.bones=Ptr(nodePointers);skin.inverseBindMatrices=Ptr(inverse);
    Put(&mesh,0,0x6ef334);Put(&mesh,0x14,0x6ef32c);mesh.componentWeightCount=4;
    Put(renderer,0xc9b8,Ptr(palette));Put(renderer,0xc9bc,1);
    for(unsigned i=0;i<256;++i){auto& n=nodes[i];Put(&n,0,0x6dc4f4);n.sceneLink=Ptr(scene);nodePointers[i]=Ptr(&n);
      n.cachedWorldPosition[0]=1;n.cachedWorldPosition[1]=2;n.cachedWorldPosition[2]=3;
      n.cachedWorldScale[0]=2;n.cachedWorldScale[1]=3;n.cachedWorldScale[2]=4;
      n.cachedWorldOrientation[0]=n.cachedWorldOrientation[4]=n.cachedWorldOrientation[8]=1;
      inverse[i]={1,0,0,0,0,1,0,0,0,0,1,0,5,6,7,1};
      // Literal noncommuting inverseBind * world expected values from CP64.
      palette[i]={2,0,0,0,0,3,0,0,0,0,4,0,11,20,31,1};
    }
  }
  uint32_t Support(){return Ptr(object)+0xb4;}
  ~Fixture(){native_owner_source::enginePointerAddress=oldEngine;native_owner_source::rendererPointerAddress=oldRenderer;source::renderSlot=oldSlot;source::renderEntry=oldEntry;}
};
static Fixture* fixture;
static unsigned originalCalls;
static uint32_t originalResult=0xabcdef01;
static void (*action)();
static uint32_t __fastcall Original(void* skin,void*,uint32_t camera,uint32_t support){
  ++originalCalls;Check(skin==&fixture->skin&&camera==Ptr(fixture->camera)&&support==fixture->Support(),"full this/camera/support forwarded");
  if(action)action();return originalResult;
}
static void Capture(){source::Capture(Ptr(&fixture->mesh),Ptr(fixture->renderer),77,0x46a367);}
static void Raise(){RaiseException(0xe0525b01u,0,0,nullptr);}
static bool RaiseThroughWrapper(){
  __try {source::SkinDraw(&fixture->skin,nullptr,Ptr(fixture->camera),fixture->Support());}
  __except(GetExceptionCode()==0xe0525b01u?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return true;}
  return false;
}
static void Call(){Check(source::SkinDraw(&fixture->skin,nullptr,Ptr(fixture->camera),fixture->Support())==originalResult,"entire original EAX preserved");}
template<class Change,class Restore> static void Reject(source::Reason reason,Change change,Restore restore){
  auto& f=*fixture;scene_geometry::Scope scene(Ptr(f.scene),Ptr(f.camera));source::Scope skin(Ptr(&f.skin),Ptr(f.camera),f.Support());
  const auto before=source::rejected[reason],success=source::matched,bad=source::mismatches;
  change();Capture();restore();Check(source::rejected[reason]==before+1&&source::matched==success&&source::mismatches==bad,"specific malformed input rejects without match credit");
}
static void Run(){
  Fixture f;fixture=&f;frameId=17;source::ownerThread=GetCurrentThreadId();source::enabled=true;
  f.hookSlot=Ptr(reinterpret_cast<void*>(&Original));source::renderEntry=f.hookSlot;
  f.hookSlot=0;Check(!source::Install(),"unknown hook target fails closed");f.hookSlot=uint32_t(source::renderEntry);
  Check(source::Install()&&source::Installed(),"owned slot installs only exact expected target");
  Check(!source::Install(),"already replaced slot is not overwritten");
  const auto calls=originalCalls;action=nullptr;originalResult=0x12340000;Call();
  Check(originalCalls==calls+1&&source::callFailures==1&&source::withoutMesh==1&&!source::active,"false AL forwards once and restores scope");
  originalResult=0x12340001;action=Capture;
  {
    const auto skinBefore=f.skin;const auto nodeBefore=f.nodes[0];const auto paletteBefore=f.palette[0];
    scene_geometry::Scope scene(Ptr(f.scene),Ptr(f.camera));const auto before=source::matched;Call();
    Check(source::matched==before+1&&source::bonesCompared==1&&source::bitDifferences==0,"literal complete palette matched at own mesh call");
    Check(!memcmp(&skinBefore,&f.skin,sizeof(skinBefore))&&!memcmp(&nodeBefore,&f.nodes[0],sizeof(nodeBefore))&&
      paletteBefore==f.palette[0],"observer leaves raw Skin, bone cache and original palette unchanged");
    Call();Check(source::matched==before+2,"repeated Skin calls have independent scopes and captures");
    source::Observation packet;{source::Scope s(Ptr(&f.skin),Ptr(f.camera),f.Support());
      Check(source::Observe(Ptr(&f.mesh),Ptr(f.renderer),991,0x46a367,packet),"current raw input observation");
      Check(packet.boneCount==1&&packet.weightHint==4&&packet.meshWeights==4&&packet.skin==Ptr(&f.skin)&&packet.submission==991&&packet.withinTolerance,"exact diagnostic metadata without geometry credit");}
    const auto beforeMismatch=source::mismatches;f.palette[0][12]+=0.25f;Call();
    Check(source::mismatches==beforeMismatch+1&&source::maxAbsoluteError>=0.25,"significant palette mismatch is counted");f.palette[0][12]=11;
    const auto beforeBits=source::bitDifferences;f.palette[0][0]=2.00001f;Call();
    Check(source::bitDifferences==beforeBits+1&&source::mismatches==beforeMismatch+1,"small tolerated difference is still not bit-exact");f.palette[0][0]=2;
    source::Scope parent(Ptr(&f.skin),Ptr(f.camera),f.Support());auto rejected=source::rejected[source::NoScope];
    Call();Check(source::active==&parent&&source::rejected[source::NoScope]==rejected+1,"nested Skin call forwards but cannot borrow outer provenance");
    action=Raise;Check(RaiseThroughWrapper()&&source::active==&parent,"SEH preserves prior Skin TLS scope");action=Capture;
  }
  const auto out=source::attempts;Capture();Check(source::attempts==out,"outside Skin scope is ignored");
  {scene_geometry::Scope scene(Ptr(f.scene),Ptr(f.camera));source::Scope skin(Ptr(&f.skin),Ptr(f.camera),f.Support());
    const auto before=source::rejected[source::Callsite],matches=source::matched;
    source::Capture(Ptr(&f.mesh),Ptr(f.renderer),77,0x479df3);
    Check(source::rejected[source::Callsite]==before+1&&source::matched==matches,"Model/callback callsite cannot claim Skin palette");}
  Reject(source::Skin,[&]{Put(&f.skin,0,0x6eaa58);},[&]{Put(&f.skin,0,0x6e8c5c);});
  Reject(source::Mesh,[&]{f.skin.base.baseMeshData=0;},[&]{f.skin.base.baseMeshData=Ptr(&f.mesh);});
  Reject(source::Mesh,[&]{Put(f.renderer,0,0x10203040);},[&]{Put(f.renderer,0,0x6f2918);});
  Reject(source::Palette,[&]{Put(f.renderer,0xc9bc,2);},[&]{Put(f.renderer,0xc9bc,1);});
  Reject(source::Palette,[&]{f.skin.boneCount=257;Put(f.renderer,0xc9bc,257);},[&]{f.skin.boneCount=1;Put(f.renderer,0xc9bc,1);});
  Reject(source::Palette,[&]{f.skin.bones=0;},[&]{f.skin.bones=Ptr(f.nodePointers);});
  Reject(source::Palette,[&]{f.skin.inverseBindMatrices=0xfffffff0;},[&]{f.skin.inverseBindMatrices=Ptr(f.inverse);});
  Reject(source::Bone,[&]{Put(&f.nodes[0],0,0x10203040);},[&]{Put(&f.nodes[0],0,0x6dc4f4);});
  Reject(source::Bone,[&]{f.nodes[0].sceneLink=0;},[&]{f.nodes[0].sceneLink=Ptr(f.scene);});
  Reject(source::Bone,[&]{f.nodes[0].flags=1;},[&]{f.nodes[0].flags=0;});
  Reject(source::Nonfinite,[&]{f.nodes[0].cachedWorldScale[0]=std::numeric_limits<float>::infinity();},[&]{f.nodes[0].cachedWorldScale[0]=2;});
  Reject(source::Nonfinite,[&]{f.inverse[0][0]=std::numeric_limits<float>::quiet_NaN();},[&]{f.inverse[0][0]=1;});
  Reject(source::Nonfinite,[&]{f.palette[0][0]=std::numeric_limits<float>::infinity();},[&]{f.palette[0][0]=2;});
  Reject(source::Support,[&]{Put(f.object,0xb0,0);},[&]{Put(f.object,0xb0,0x200);});
  Reject(source::Scene,[&]{Put(f.engine,0x18,0);},[&]{Put(f.engine,0x18,Ptr(f.scene));});
  Reject(source::Scene,[&]{f.camera[0x231]=1;},[&]{f.camera[0x231]=0;});
  Reject(source::Scene,[&]{++frameId;},[&]{--frameId;});
  Reject(source::Registry,[&]{scene_geometry::AdvanceMutationSerial();},[]{});
  Reject(source::Scene,[&]{f.hookSlot=0;},[&]{f.hookSlot=Ptr(reinterpret_cast<void*>(&source::SkinDraw));});
  {
    scene_geometry::Scope scene(Ptr(f.scene),Ptr(f.camera));f.skin.boneCount=256;Put(f.renderer,0xc9bc,256);auto bones=source::bonesCompared;
    action=Capture;Call();Check(source::bonesCompared==bones+256,"exact bone limit is bounded and supported");f.skin.boneCount=1;Put(f.renderer,0xc9bc,1);
    auto before=source::attempts;source::ownerThread=GetCurrentThreadId()+1;Call();source::ownerThread=GetCurrentThreadId();Check(source::attempts==before,"foreign owner thread forwards without capture");
  }
  Check(!source::active&&!scene_geometry::active&&!testRemixApi,"fixture leaves no scope or Remix API instance");
  source::enabled=false;source::originalSkin=nullptr;action=nullptr;fixture=nullptr;
}
}
int main(){try{skin_test::Watchdog watchdog;skin_test::Run();printf("{\"status\":\"PASS\",\"checks\":%u,\"gpu\":false,\"nativeCode\":false}\n",skin_test::checks);return 0;}
  catch(const std::exception& error){fprintf(stderr,"FAIL %s\n",error.what());return 1;}}
