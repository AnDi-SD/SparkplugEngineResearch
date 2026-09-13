// Own CPU adapter fixture on literal observed PC ABI records. No original
// executable, fixed-address code, emulator, D3D device, bridge or GPU runs.
#define WINX_REMIX_TEST
#include "winx_d3d9_probe.cpp"
#include <stdexcept>
#include <limits>

namespace owner_test {
static unsigned checks;
static void Check(bool ok,const char* message) {
  ++checks;if(!ok)throw std::runtime_error(message);
}
static uint32_t Ptr(const void* value) {return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(value));}
static void Put(void* data,unsigned offset,uint32_t value) {memcpy(static_cast<uint8_t*>(data)+offset,&value,4);}
static uint32_t Word(const void* data,unsigned offset) {uint32_t value;memcpy(&value,static_cast<const uint8_t*>(data)+offset,4);return value;}
static void Vector(void* data,unsigned offset,uint32_t* entries,unsigned count) {
  Put(data,offset+4,Ptr(entries));Put(data,offset+8,Ptr(entries)+count*4);Put(data,offset+12,Ptr(entries)+count*4);
}
struct Watchdog {
  HANDLE stop=nullptr,thread=nullptr;
  static DWORD WINAPI Wait(void* value) {
    if(WaitForSingleObject(static_cast<HANDLE>(value),30000)==WAIT_TIMEOUT)
      TerminateProcess(GetCurrentProcess(),0xe0520a30u);
    return 0;
  }
  Watchdog() {
    stop=CreateEventW(nullptr,TRUE,FALSE,nullptr);Check(stop!=nullptr,"create own CPU watchdog event");
    thread=CreateThread(nullptr,0,Wait,stop,0,nullptr);
    if(!thread){CloseHandle(stop);stop=nullptr;Check(false,"create own CPU watchdog thread");}
  }
  ~Watchdog() {if(stop)SetEvent(stop);if(thread){WaitForSingleObject(thread,1000);CloseHandle(thread);}if(stop)CloseHandle(stop);}
};
struct GlobalPages {
  void* allocation=nullptr;
  uintptr_t previousEngine=native_owner_source::enginePointerAddress,previousRenderer=native_owner_source::rendererPointerAddress;
  GlobalPages() {
    // Two own DWORD slots replace only the platform pointer-address inputs.
    // Native memory reading/ABI validation is unchanged; no foreign mappings
    // are changed and no original address-space behavior is claimed.
    allocation=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    Check(allocation!=nullptr,"allocate own nonexecutable global pointer slots");
    native_owner_source::enginePointerAddress=reinterpret_cast<uintptr_t>(allocation);
    native_owner_source::rendererPointerAddress=reinterpret_cast<uintptr_t>(allocation)+4;
  }
  ~GlobalPages(){native_owner_source::enginePointerAddress=previousEngine;native_owner_source::rendererPointerAddress=previousRenderer;if(allocation)VirtualFree(allocation,0,MEM_RELEASE);}
};
struct Fixture {
  alignas(4) uint8_t engine[0x80]{},scene[0x54]{},otherScene[0x54]{},system[0x1d8]{},root[0x84]{},child[0x84]{},zone[0xc8]{};
  alignas(4) uint8_t objects[2][0x10c]{},payload[0x8c]{},models[2][0x60]{},meshes[2][0x88]{};
  alignas(4) uint8_t camera[0x238]{},otherCamera[0x238]{},renderer[0xf368]{};
  uint32_t staticList[3]{},children[1]{},zoneRoots[2]{},renderables[3]{},otherRenderables[1]{},payloadRenderables[1]{};
  Fixture() {
    Put(reinterpret_cast<void*>(native_owner_source::enginePointerAddress),0,Ptr(engine));
    Put(reinterpret_cast<void*>(native_owner_source::rendererPointerAddress),0,Ptr(renderer));
    Put(engine,0x18,Ptr(scene));Put(engine,0x1c,Ptr(camera));Put(renderer,0,0x6f2918);
    Put(camera,0,0x6ef1e0);Put(camera,0x138,0x3f800000); // observed projection[11]=1, [15]=0
    Put(scene,0,0x6e7358);Put(scene,0x38,Ptr(system));Put(system,0,0x6ec528);Put(system,0x1d4,Ptr(root));
    for(auto node:{root,child}) {Put(node,0,0x6e4420);Put(node,0x80,Ptr(scene));Put(node,0x60,Ptr(zone));}
    children[0]=Ptr(child);Put(root,0x58,Ptr(children));Put(root,0x5c,1);Put(child,0x54,Ptr(root));Put(child,0x78,Ptr(payload));
    Put(zone,0,0x6ebacc);zoneRoots[0]=Ptr(root);zoneRoots[1]=Ptr(child);Vector(zone,0xb4,zoneRoots,2);
    for(unsigned i=0;i<2;++i) {
      auto object=objects[i];Put(object,0,0x6e65e8);Put(object,0x14,0x6e6604);
      Put(object,0x84,Ptr(object));Put(object,0x88,Ptr(scene));Put(object,0x48,Ptr(object)+0x8c);Put(object,0x4c,Ptr(object)+0xcc);
      float matrix[16]{};matrix[0]=matrix[5]=matrix[10]=matrix[15]=1;matrix[12]=float(i+3);
      memcpy(object+0x8c,matrix,64);memcpy(object+0xcc,matrix,64);
      Put(models[i],0,0x6eaa58);Put(models[i],0x58,Ptr(meshes[i]));Put(meshes[i],0,0x6ef334);Put(meshes[i],0x14,0x6ef32c);
    }
    staticList[0]=Ptr(objects[0]);staticList[1]=Ptr(objects[1]);staticList[2]=Ptr(objects[0]);Vector(root,0x30,staticList,3);
    renderables[0]=Ptr(models[0]);renderables[1]=Ptr(models[1]);renderables[2]=Ptr(models[0]);Vector(objects[0],0x18,renderables,3);
    otherRenderables[0]=Ptr(models[0]);Vector(objects[1],0x18,otherRenderables,1);
    Put(payload,0,0x6f4540);Put(payload,0x10,0x6f4528);Put(payload,0x80,Ptr(payload));Put(payload,0x88,Ptr(scene));
    Put(payload,0x44,Ptr(objects[0])+0x8c);Put(payload,0x48,Ptr(objects[0])+0xcc);
    payloadRenderables[0]=Ptr(models[1]);Vector(payload,0x14,payloadRenderables,1);
  }
  uint32_t Static(unsigned i=0) const {return Ptr(objects[i])+0x14;}
  uint32_t Partition() const {return Ptr(payload)+0x10;}
};

namespace source=native_owner_source;
static Fixture* current;
static unsigned originalDrawCalls,originalMeshCalls,originalDestroyCalls;
static uint32_t expectedSelf,expectedCamera,expectedArgument,drawResult=0x12345601u;
static void (*drawAction)(void*,uint32_t,uint32_t);
static std::vector<source::Packet> captured;
static D3DMATRIX World(uint32_t support) {
  D3DMATRIX value{};Check(source::Read(Word(reinterpret_cast<void*>(support),0x34),value),"read owned fixture matrix");return value;
}
static uint32_t __fastcall MockDraw(void* self,void*,uint32_t camera,uint32_t argument) {
  ++originalDrawCalls;
  Check(Ptr(self)==expectedSelf&&camera==expectedCamera&&argument==expectedArgument,"wrapper forwards this and both full DWORD arguments unchanged");
  if(drawAction)drawAction(self,camera,argument);
  return drawResult;
}
static void __fastcall MockDestroy(void* self,void*) {
  ++originalDestroyCalls;Check(Ptr(self)==expectedSelf,"direct destructor wrapper forwards exact complete pointer");
}
static uint32_t __fastcall MockMesh(void* renderer,void*,uint32_t mesh) {
  ++originalMeshCalls;
  Check(Ptr(renderer)==Ptr(current->renderer)+0x18&&mesh==Ptr(current->meshes[0]),"existing mesh wrapper forwards exact interface and mesh");
  Check(native_mesh_source::active&&native_mesh_source::active->valid&&!native_mesh_source::active->owner.valid,
    "test callback callsite cannot gain owner credit through existing mesh wrapper");
  return 0xabcd0100u;
}
static uint32_t __fastcall RaiseMesh(void*,void*,uint32_t) {RaiseException(0xe0520a01u,0,0,nullptr);return 0;}
static bool RaisedMesh(void* renderer,uint32_t mesh) {
  __try {native_mesh_source::Submit(renderer,nullptr,mesh);}
  __except(GetExceptionCode()==0xe0520a01u?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return true;}
  return false;
}
static void CaptureOwn(void* model,uint32_t camera,uint32_t support) {
  Check(source::activeModel&&source::activeModel->model==Ptr(model)&&source::activeModel->camera==camera&&
    source::activeModel->support==support,"Model wrapper publishes exact borrowed arguments");
  source::Packet packet;
  Check(source::Capture(Word(model,0x58),Ptr(current->renderer),91,0x479df3,packet),"test-owned own-callsite input captures current Model relation");
  Check(source::Qualify(packet,packet.mesh,packet.renderer,91,World(support)),"captured current relation qualifies against exact world");
  captured.push_back(packet);
}
static void RaiseOwn(void*,uint32_t,uint32_t) {RaiseException(0xe0520a01u,0,0,nullptr);}
static bool RaisedModel(void* model,uint32_t camera,uint32_t support) {
  __try {source::ModelDraw(model,nullptr,camera,support);}
  __except(GetExceptionCode()==0xe0520a01u?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return true;}
  return false;
}
static bool RaisedSupport(void* support,uint32_t camera,uint32_t force) {
  __try {source::StaticDraw(support,nullptr,camera,force);}
  __except(GetExceptionCode()==0xe0520a01u?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return true;}
  return false;
}
static void ExpectDraw(uint32_t self,uint32_t camera,uint32_t argument) {
  expectedSelf=self;expectedCamera=camera;expectedArgument=argument;drawAction=nullptr;
}
template<class Action> static void WithPacket(Fixture& f,Action action) {
  scene_geometry::Scope scene(Ptr(f.scene),Ptr(f.camera));
  source::ModelScope model(Ptr(f.models[0]),Ptr(f.camera),f.Static());
  source::Packet packet;
  Check(source::Capture(Ptr(f.meshes[0]),Ptr(f.renderer),101,0x479df3,packet),"fresh literal ownership captures before mutation case");
  action(packet);
}
static bool Qualify(source::Packet& packet) {return source::Qualify(packet,packet.mesh,packet.renderer,packet.submission,packet.world);}
static void Wrappers(Fixture& f) {
  source::originalStatic=source::originalPartition=source::originalModel=reinterpret_cast<source::NativeDraw>(&MockDraw);
  source::originalSceneDestroy=source::originalNodeDestroy=reinterpret_cast<source::NativeDestroy>(&MockDestroy);
  native_mesh_source::originalSubmit=reinterpret_cast<native_mesh_source::NativeSubmit>(&MockMesh);
  native_mesh_source::ownerThread=GetCurrentThreadId();
  scene_geometry::Scope scene(Ptr(f.scene),Ptr(f.camera));
  const auto supportBefore=source::supportCalls;
  ExpectDraw(f.Static(),Ptr(f.camera),0xfedcba98u);drawResult=0x12345600;
  auto count=originalDrawCalls;
  Check(source::StaticDraw(reinterpret_cast<void*>(f.Static()),nullptr,Ptr(f.camera),0xfedcba98u)==drawResult&&originalDrawCalls==count+1,
    "Static wrapper calls original once and preserves full false-AL EAX");
  Check(!source::activeSupport&&source::supportCalls==supportBefore+1,"Static TLS restored and exactly one support call counted");
  ExpectDraw(f.Partition(),Ptr(f.camera),0x01020304u);drawResult=0xabcdef01;
  count=originalDrawCalls;
  Check(source::PartitionDraw(reinterpret_cast<void*>(f.Partition()),nullptr,Ptr(f.camera),0x01020304u)==drawResult&&originalDrawCalls==count+1,
    "Partition wrapper preserves adjusted receiver and complete true-AL EAX");
  const auto queued=source::queueSupportCalls;Put(f.renderer,0xc050,1);
  source::PartitionDraw(reinterpret_cast<void*>(f.Partition()),nullptr,Ptr(f.camera),0x01020304u);
  Check(source::queueSupportCalls==queued+1&&source::modelCalls==0&&source::captures==0,
    "queued support notification alone is not a Model or mesh submission");Put(f.renderer,0xc050,0);
  Put(f.renderer,0xc050,0xffffff00u);
  source::PartitionDraw(reinterpret_cast<void*>(f.Partition()),nullptr,Ptr(f.camera),0x01020304u);
  Check(source::queueSupportCalls==queued+1,"queue flag reads only C050 byte and ignores nonzero adjacent bytes");Put(f.renderer,0xc050,0);
  ExpectDraw(Ptr(f.models[0]),Ptr(f.camera),f.Static());
  auto failures=source::modelFailures,without=source::modelWithoutMesh;drawResult=0x12340100;
  count=originalDrawCalls;
  Check(source::ModelDraw(f.models[0],nullptr,Ptr(f.camera),f.Static())==drawResult&&originalDrawCalls==count+1,
    "Model wrapper preserves entire EAX without bool normalization");
  Check(source::modelFailures==failures+1&&source::modelWithoutMesh==without+1,"AL zero counts false despite nonzero high EAX");
  drawResult=0x12340001;failures=source::modelFailures;
  source::ModelDraw(f.models[0],nullptr,Ptr(f.camera),f.Static());
  Check(source::modelFailures==failures&&!source::activeModel,"AL true counts success and Model TLS restores");
  drawAction=&CaptureOwn;captured.clear();
  source::ModelDraw(f.models[0],nullptr,Ptr(f.camera),f.Static());
  Check(captured.size()==1&&captured[0].supportCall==0&&captured[0].registrations==2&&
    captured[0].modelOccurrences==2&&captured[0].firstModelOrdinal==0,
    "queued Model outside support scope uses argument membership and preserves repeated refs");
  const auto first=captured[0].modelCall;
  source::ModelDraw(f.models[0],nullptr,Ptr(f.camera),f.Static());
  Check(captured.size()==2&&captured[1].modelCall!=first,"repeated actual Model calls receive distinct operation identities");
  {
    source::SupportScope support(f.Static(),Ptr(f.camera));
    source::ModelDraw(f.models[0],nullptr,Ptr(f.camera),f.Static());
    Check(captured.back().supportCall==support.sequence,"matching support operation joins actual Model call");
  }
  ExpectDraw(Ptr(f.models[0]),Ptr(f.camera),f.Static(1));drawAction=&CaptureOwn;
  source::ModelDraw(f.models[0],nullptr,Ptr(f.camera),f.Static(1));
  Check(captured.back().object==Ptr(f.objects[1])&&captured.back().registrations==1&&captured.back().modelOccurrences==1,
    "shared Model in another support remains a distinct owner relation");
  ExpectDraw(Ptr(f.models[1]),Ptr(f.camera),f.Partition());drawAction=&CaptureOwn;
  source::ModelDraw(f.models[1],nullptr,Ptr(f.camera),f.Partition());
  Check(captured.back().object==Ptr(f.payload)&&captured.back().support==f.Partition(),"partition complete/support adjustment qualifies independently");
  ExpectDraw(Ptr(f.models[0]),Ptr(f.camera),f.Static());drawAction=[](void*,uint32_t,uint32_t){
    const auto calls=originalMeshCalls,own=source::activeModel->ownMeshes;
    Check(native_mesh_source::Submit(current->renderer+0x18,nullptr,Ptr(current->meshes[0]))==0xabcd0100u&&originalMeshCalls==calls+1,
      "existing mesh wrapper invokes test original once and preserves full EAX");
    Check(source::activeModel->ownMeshes==own,"real test callback return address is not Model own callsite479DF3");
  };
  source::ModelDraw(f.models[0],nullptr,Ptr(f.camera),f.Static());
  Check(!native_mesh_source::active,"existing mesh scope restores after callback submit");
  {
    native_mesh_source::Scope parent{};auto* previous=native_mesh_source::active;native_mesh_source::active=&parent;
    native_mesh_source::originalSubmit=reinterpret_cast<native_mesh_source::NativeSubmit>(&RaiseMesh);
    Check(RaisedMesh(f.renderer+0x18,Ptr(f.meshes[0])),"existing mesh wrapper propagates original SEH");
    Check(native_mesh_source::active==&parent,"mesh finally restores parent TLS after SEH");
    native_mesh_source::active=previous;native_mesh_source::originalSubmit=reinterpret_cast<native_mesh_source::NativeSubmit>(&MockMesh);
  }
  drawAction=&RaiseOwn;
  {
    source::ModelScope parent(Ptr(f.models[1]),Ptr(f.camera),f.Partition());
    Check(RaisedModel(f.models[0],Ptr(f.camera),f.Static()),"original Model SEH reaches caller unchanged");
    Check(source::activeModel==&parent,"Model finally restores previous TLS after SEH");
  }
  ExpectDraw(f.Static(),Ptr(f.camera),91);drawAction=&RaiseOwn;
  {
    source::SupportScope parent(f.Partition(),Ptr(f.camera));
    Check(RaisedSupport(reinterpret_cast<void*>(f.Static()),Ptr(f.camera),91),"original support SEH reaches caller unchanged");
    Check(source::activeSupport==&parent,"support finally restores previous TLS after SEH");
  }
  ExpectDraw(Ptr(f.models[0]),Ptr(f.camera),f.Static());drawResult=0xfeed0100;
  auto models=source::modelCalls;source::enabled=false;count=originalDrawCalls;
  Check(source::ModelDraw(f.models[0],nullptr,Ptr(f.camera),f.Static())==drawResult&&originalDrawCalls==count+1&&source::modelCalls==models,
    "disabled audit preserves original call and skips observer accounting");source::enabled=true;
  source::ownerThread=0;count=originalDrawCalls;
  Check(source::ModelDraw(f.models[0],nullptr,Ptr(f.camera),f.Static())==drawResult&&originalDrawCalls==count+1&&source::modelCalls==models,
    "foreign owner thread preserves original call and skips observer accounting");source::ownerThread=GetCurrentThreadId();
  Check(!source::activeModel&&!source::activeSupport,"normal and exceptional wrapper tests leave no borrowed TLS");
}
static void CaptureGuards(Fixture& f) {
  source::Packet packet;packet.valid=true;
  Check(!source::Capture(Ptr(f.meshes[0]),Ptr(f.renderer),100,0x479df3,packet)&&!packet.valid,"Capture clears output and rejects absent Model scope");
  {
    scene_geometry::Scope scene(Ptr(f.scene),Ptr(f.camera));source::ModelScope model(Ptr(f.models[0]),Ptr(f.camera),f.Static());
    Check(!source::Capture(Ptr(f.meshes[0]),Ptr(f.renderer),100,0x479dd4,packet)&&model.ownMeshes==0,
      "pre/post callback same-mesh inputs cannot claim Model own callsite");
    f.camera[0x231]=1;Check(!source::Capture(Ptr(f.meshes[0]),Ptr(f.renderer),100,0x479df3,packet),"alternate projection main camera is excluded");f.camera[0x231]=0;
    f.camera[0xc8]=1;Check(!source::Capture(Ptr(f.meshes[0]),Ptr(f.renderer),100,0x479df3,packet),"Is2D main camera is excluded");f.camera[0xc8]=0;
    Put(f.camera,0x138,0);Check(!source::Capture(Ptr(f.meshes[0]),Ptr(f.renderer),100,0x479df3,packet),"nonperspective projection matrix is excluded");Put(f.camera,0x138,0x3f800000);
    Put(f.models[0],0x58,Ptr(f.meshes[1]));
    Check(!source::Capture(Ptr(f.meshes[0]),Ptr(f.renderer),100,0x479df3,packet),"entry-time old mesh rejected after test-owned pre mutation");
    Check(source::Capture(Ptr(f.meshes[1]),Ptr(f.renderer),100,0x479df3,packet)&&packet.mesh==Ptr(f.meshes[1]),
      "own-mesh capture uses live model58 after pre rather than scope-entry snapshot");Put(f.models[0],0x58,Ptr(f.meshes[0]));
    auto matrix=World(f.Static());float nan=std::numeric_limits<float>::quiet_NaN();memcpy(f.objects[0]+0x8c,&nan,4);
    Check(!source::Capture(Ptr(f.meshes[0]),Ptr(f.renderer),100,0x479df3,packet),"nonfinite native world refused");memcpy(f.objects[0]+0x8c,&matrix,64);
    Put(f.models[0],0,0x6eaa5c);Check(!source::Capture(Ptr(f.meshes[0]),Ptr(f.renderer),100,0x479df3,packet),"derived or unknown Model identity remains unqualified");Put(f.models[0],0,0x6eaa58);
    Vector(f.objects[0],0x18,f.renderables,0);Check(!source::Capture(Ptr(f.meshes[0]),Ptr(f.renderer),100,0x479df3,packet),"Model outside live renderable vector rejected");Vector(f.objects[0],0x18,f.renderables,3);
    {
      scene_geometry::Scope nested(Ptr(f.scene),Ptr(f.camera));source::ModelScope nestedModel(Ptr(f.models[0]),Ptr(f.camera),f.Static());
      Check(!source::Capture(Ptr(f.meshes[0]),Ptr(f.renderer),100,0x479df3,packet),"nested Scene is not main owner source");
    }
    Check(source::activeModel==&model&&scene_geometry::active==&scene,"nested scopes restore original ownership context");
  }
  {
    scene_geometry::Scope scene(Ptr(f.scene),Ptr(f.camera));source::ModelScope model(Ptr(f.models[0]),Ptr(f.otherCamera),f.Static());
    Check(!source::Capture(Ptr(f.meshes[0]),Ptr(f.renderer),100,0x479df3,packet),"foreign camera Model call cannot claim main scene");
  }
}
static void QualificationGuards(Fixture& f) {
  WithPacket(f,[&](source::Packet& p){Check(Qualify(p),"unchanged owner qualifies");
    Check(!source::Qualify(p,p.mesh,p.renderer,p.submission+1,p.world),"foreign submission sequence rejected");
    D3DMATRIX other=p.world;other.m[3][0]+=1;Check(!source::Qualify(p,p.mesh,p.renderer,p.submission,other),"actual D3D world mismatch rejected");
    Put(f.models[0],0x58,Ptr(f.meshes[1]));Check(!Qualify(p),"live mesh replacement invalidates packet");Put(f.models[0],0x58,Ptr(f.meshes[0]));
    Vector(f.objects[0],0x18,f.renderables,0);Check(!Qualify(p),"live Model removal after Capture invalidates packet");Vector(f.objects[0],0x18,f.renderables,3);
    f.renderables[2]=Ptr(f.models[1]);Check(!Qualify(p),"changed repeated-reference count after Capture invalidates packet");f.renderables[2]=Ptr(f.models[0]);
    Put(f.models[0],0,0x6eaa5c);Check(!Qualify(p),"live Model identity change after Capture invalidates packet");Put(f.models[0],0,0x6eaa58);
    Put(f.objects[0],0x14,0x6dcadc);Check(!Qualify(p),"live support identity change after Capture invalidates packet");Put(f.objects[0],0x14,0x6e6604);
    Put(f.objects[0],0x88,Ptr(f.otherScene));Check(!Qualify(p),"live support scene change after Capture invalidates packet");Put(f.objects[0],0x88,Ptr(f.scene));
    Put(f.system,0x1d4,Ptr(f.child));Check(!Qualify(p),"partition root replacement invalidates packet");Put(f.system,0x1d4,Ptr(f.root));
    Put(f.engine,0x18,Ptr(f.otherScene));Check(!Qualify(p),"engine scene replacement invalidates packet");Put(f.engine,0x18,Ptr(f.scene));
    Put(f.engine,0x1c,Ptr(f.otherCamera));Check(!Qualify(p),"engine camera replacement invalidates packet");Put(f.engine,0x1c,Ptr(f.camera));
    ++frameId;Check(!Qualify(p),"packet cannot cross frame boundary");--frameId;
    scene_geometry::AdvanceMutationSerial();Check(!Qualify(p),"retirement/reset mutation invalidates current packet");
  });
  source::Packet stale;
  WithPacket(f,[&](source::Packet& p){stale=p;});
  WithPacket(f,[&](source::Packet& fresh){Check(fresh.sceneScope!=stale.sceneScope&&!Qualify(stale),"same-address new scene operation cannot reuse old packet");});
  WithPacket(f,[&](source::Packet& p){source::ModelScope replacement(Ptr(f.models[0]),Ptr(f.camera),f.Static());
    Check(!Qualify(p),"same-address new Model operation cannot reuse old packet");});
}
static uint64_t destroySerial;
static void __fastcall CheckRetiredDestroy(void* self,void*) {
  MockDestroy(self,nullptr);Check(scene_geometry::MutationSerial()>destroySerial,"retirement precedes original destructor callback");
}
static DWORD WINAPI RetireOnWorker(void* node) {source::Retire(Ptr(node),false);return 0;}
static void Lifetime(Fixture& f) {
  source::originalSceneDestroy=source::originalNodeDestroy=reinterpret_cast<source::NativeDestroy>(&CheckRetiredDestroy);
  WithPacket(f,[&](source::Packet& packet){expectedSelf=Ptr(f.child);destroySerial=scene_geometry::MutationSerial();auto calls=originalDestroyCalls;
    source::NodeDestroy(f.child,nullptr);Check(originalDestroyCalls==calls+1&&!Qualify(packet),"child direct dtor invalidates entire current graph epoch and runs once");});
  WithPacket(f,[&](source::Packet& packet){expectedSelf=Ptr(f.scene);destroySerial=scene_geometry::MutationSerial();auto calls=originalDestroyCalls;
    source::SceneDestroy(f.scene,nullptr);Check(originalDestroyCalls==calls+1&&!Qualify(packet),"scene direct dtor invalidates owner before original and runs once");});
  WithPacket(f,[&](source::Packet& packet){const auto serial=scene_geometry::MutationSerial();const auto count=source::nodeRetirements;
    HANDLE thread=CreateThread(nullptr,0,RetireOnWorker,f.child,0,nullptr);Check(thread!=nullptr,"create own retirement worker");
    const auto waited=WaitForSingleObject(thread,3000);CloseHandle(thread);Check(waited==WAIT_OBJECT_0,"bounded own retirement worker joined");
    Check(scene_geometry::MutationSerial()==serial+1&&source::nodeRetirements==count&&!Qualify(packet),
      "foreign-thread retirement atomically invalidates without touching render-thread counters");});
}
static source::Packet* retiringRenderNodePacket;
static void __fastcall CheckRetiredRenderNodeDestroy(void* self,void*) {
  CheckRetiredDestroy(self,nullptr);
  Check(retiringRenderNodePacket&&!Qualify(*retiringRenderNodePacket),
    "RenderNode owner is invalid before the original destructor can use it");
}
static void RenderNode(Fixture& f) {
  // Literal inherited support with a deliberately opaque test-owned primary
  // identity. It is compared as data, never dereferenced or executed.
  struct OwnedNode {
    Fixture& fixture;
    alignas(4) uint8_t object[0x1b8]{},previousRoot[16]{},previousChild[16]{};
    uint32_t roots[3]{},children[1]{},models[3]{};
    explicit OwnedNode(Fixture& f):fixture(f) {
      memcpy(previousRoot,f.root+0x20,16);memcpy(previousChild,f.child+0x20,16);
      Put(object,0,0x13572468);Put(object,0x3c,Ptr(f.scene));Put(object,0xb0,0x201);
      Put(object,0xb4,0x6dcadc);Put(object,0x124,Ptr(object));
      Put(object,0xe8,Ptr(object)+0x138);Put(object,0xec,Ptr(object)+0x178);
      memcpy(object+0x138,f.objects[0]+0x8c,64);memcpy(object+0x178,f.objects[0]+0xcc,64);
      models[0]=Ptr(f.models[0]);models[1]=Ptr(f.models[1]);models[2]=Ptr(f.models[0]);Vector(object,0xb8,models,3);
      roots[0]=roots[2]=Ptr(object);children[0]=Ptr(object);
      Vector(f.root,0x20,roots,3);Vector(f.child,0x20,children,1);
    }
    ~OwnedNode(){memcpy(fixture.root+0x20,previousRoot,16);memcpy(fixture.child+0x20,previousChild,16);}
    uint32_t Support() const{return Ptr(object)+0xb4;}
  } node(f);
  source::originalRenderNode=reinterpret_cast<source::NativeDraw>(&MockDraw);
  source::originalRenderNodeDestroy=reinterpret_cast<source::NativeDestroy>(&CheckRetiredRenderNodeDestroy);
  scene_geometry::Scope scene(Ptr(f.scene),Ptr(f.camera));
  const auto& registry=scene.RegistrySnapshot();unsigned occurrences=0;
  for(const auto& occurrence:registry.occurrences)if(occurrence.object==Ptr(node.object)) {
    Check(occurrence.support==node.Support()&&occurrence.source==scene_geometry::OccurrenceSource::RenderNodeVector&&
      ((occurrence.node==Ptr(f.root)&&(occurrence.ordinal==0||occurrence.ordinal==2))||
       (occurrence.node==Ptr(f.child)&&occurrence.ordinal==0)),
      "inherited owner occurrence preserves vector20 node and ordinal including null gap");
    ++occurrences;
  }
  Check(registry.valid&&registry.occurrencesComplete&&registry.dynamic==1&&occurrences==3,
    "inherited support is unique while repeated root and child registrations are retained");
  source::ModelScope model(Ptr(f.models[0]),Ptr(f.camera),node.Support());
  auto capture=[&](source::Packet& packet){return source::Capture(Ptr(f.meshes[0]),Ptr(f.renderer),171,0x479df3,packet);};
  source::Packet packet;
  Check(capture(packet)&&packet.valid&&packet.object==Ptr(node.object)&&packet.support==node.Support()&&
    packet.ownerKind==2&&packet.ownerPrimary==0x13572468&&packet.registrations==3&&
    packet.modelOccurrences==2&&packet.firstModelOrdinal==0&&packet.supportCall==0,
    "derived primary with exact inherited support captures queued Model and repeated refs");
  Check(Qualify(packet)&&!memcmp(&packet.world,node.object+0x138,64),
    "inherited owner qualifies exact native world without a primary-class allowlist");
  source::Packet rejected;
  Put(node.object,0xb0,1);
  Check(!capture(rejected)&&!rejected.valid,"Enabled bit clear rejects inherited Capture despite other flags");
  Check(!Qualify(packet),"Enabled bit clear invalidates inherited packet");Put(node.object,0xb0,0x201);
  Put(node.object,0x3c,Ptr(f.otherScene));
  Check(!capture(rejected),"foreign complete RenderNode scene rejects Capture");
  Check(!Qualify(packet),"foreign complete RenderNode scene invalidates packet");Put(node.object,0x3c,Ptr(f.scene));
  Put(node.object,0xe8,Ptr(f.objects[0])+0x8c);
  Check(!capture(rejected),"equal matrix bytes at foreign world pointer cannot gain inherited owner");
  Check(!Qualify(packet),"wrong inherited world pointer invalidates packet even with equal bytes");Put(node.object,0xe8,Ptr(node.object)+0x138);
  Put(node.object,0xec,Ptr(f.objects[0])+0xcc);
  Check(!capture(rejected),"equal inverse bytes at foreign pointer cannot gain inherited owner");
  Check(!Qualify(packet),"wrong inherited inverse pointer invalidates packet");Put(node.object,0xec,Ptr(node.object)+0x178);
  Put(node.object,0,0x24681357);
  Check(!Qualify(packet),"changed opaque primary identity invalidates previously captured owner");
  Check(capture(rejected)&&rejected.ownerPrimary==0x24681357&&Qualify(rejected),
    "another nonzero derived primary qualifies through the same inherited boundary");
  Put(node.object,0,0);Check(!capture(rejected),"missing opaque primary identity rejects inherited Capture");Put(node.object,0,0x13572468);
  Check(Qualify(packet),"restored live inherited fields qualify within the unchanged operation");
  {
    source::SupportScope parent(f.Static(),Ptr(f.camera));
    ExpectDraw(node.Support(),Ptr(f.camera),0xfedcba98);drawResult=0x24680100;
    drawAction=[](void* self,uint32_t camera,uint32_t){
      Check(source::activeSupport&&source::activeSupport->support==Ptr(self)&&source::activeSupport->camera==camera,
        "RenderNode wrapper publishes adjusted inherited support only during original call");
    };
    const auto calls=originalDrawCalls;const auto supports=source::supportCalls;
    Check(source::RenderNodeDraw(reinterpret_cast<void*>(node.Support()),nullptr,Ptr(f.camera),0xfedcba98)==drawResult&&originalDrawCalls==calls+1,
      "RenderNode wrapper calls original once with full arguments and preserves false-AL EAX");
    Check(source::activeSupport==&parent&&source::supportCalls==supports+1,
      "RenderNode wrapper restores previous support scope and counts once");drawAction=nullptr;
  }
  Check(capture(packet)&&Qualify(packet),"inherited packet refreshed before destruction case");
  expectedSelf=Ptr(node.object);destroySerial=scene_geometry::MutationSerial();retiringRenderNodePacket=&packet;
  const auto calls=originalDestroyCalls;const auto retired=source::renderNodeRetirements;const auto partitions=source::nodeRetirements;
  source::RenderNodeDestroy(node.object,nullptr);retiringRenderNodePacket=nullptr;
  Check(originalDestroyCalls==calls+1&&source::renderNodeRetirements==retired+1&&source::nodeRetirements==partitions,
    "RenderNode destructor forwards complete receiver once and records the correct retirement kind");
  Check(!Qualify(packet)&&!capture(rejected),"RenderNode retirement invalidates packet and current cached registry epoch");
}
static void Run() {
  Fixture fixture;current=&fixture;source::enabled=true;source::ownerThread=GetCurrentThreadId();frameId=17;
  Check(!source::activeModel&&!source::activeSupport&&!scene_geometry::active&&!testRemixApi,"fresh CPU observer has no live device/API/scopes");
  Wrappers(fixture);CaptureGuards(fixture);QualificationGuards(fixture);Lifetime(fixture);RenderNode(fixture);
  Check(!source::activeModel&&!source::activeSupport&&!scene_geometry::active&&!native_mesh_source::active,
    "all normal and exceptional tests restore TLS and scene scopes");
  Check(source::used==0&&!testRemixApi,"owner capture and qualification never submit an API instance themselves");
  source::enabled=false;source::originalStatic=source::originalPartition=source::originalModel=source::originalRenderNode=nullptr;
  source::originalSceneDestroy=source::originalNodeDestroy=source::originalRenderNodeDestroy=nullptr;native_mesh_source::originalSubmit=nullptr;
  Put(reinterpret_cast<void*>(source::enginePointerAddress),0,0);Put(reinterpret_cast<void*>(source::rendererPointerAddress),0,0);current=nullptr;
}
} // namespace owner_test

int main() {
  try {
    owner_test::GlobalPages pages;owner_test::Watchdog watchdog;owner_test::Run();
    printf("{\"status\":\"PASS\",\"checks\":%u,\"gpu\":false,\"nativeGameCodeExecuted\":false}\n",owner_test::checks);
    return 0;
  } catch(const std::exception& error) {fprintf(stderr,"FAIL %s\n",error.what());return 1;}
}
