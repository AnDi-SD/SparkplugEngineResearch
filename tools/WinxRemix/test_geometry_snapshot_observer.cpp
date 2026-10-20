// Own CPU boundary fixture. Literal ABI records and opaque resource tokens
// exercise the passive adapter; no native game instructions, COM calls or GPU.
#define WINX_REMIX_TEST
#include "winx_d3d9_probe.cpp"
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace geometry_observer_test {
namespace observer=geometry_snapshot_observer;
namespace abi=sparkplug::evidence::pc;
static unsigned checks;
static void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
static uint32_t Ptr(const void* value){return uint32_t(reinterpret_cast<uintptr_t>(value));}
static void Put(void* object,unsigned offset,uint32_t value){memcpy(static_cast<unsigned char*>(object)+offset,&value,4);}
static uint32_t Word(const void* object,unsigned offset){uint32_t value=0;memcpy(&value,static_cast<const unsigned char*>(object)+offset,4);return value;}
static void Vector(void* object,unsigned offset,uint32_t* entries,unsigned count){
  Put(object,offset+4,Ptr(entries));Put(object,offset+8,Ptr(entries)+count*4);Put(object,offset+12,Ptr(entries)+count*4);
}
struct Fixture {
  alignas(4) unsigned char scene[0x54]{},system[0x1d8]{},root[0x84]{},objects[2][0x10c]{},manager[0x54]{},engine[0x20]{};
  uint32_t singleton=0,rendererSingleton=0,staticList[2]{},selection[2]{},modelSlots[2]{},tokens[4]{1,2,3,4},declaration[7]{};
  abi::spCameraObservedLayout camera{};
  abi::spNodeLayout systemRoot{};
  abi::spModelLayout models[2]{};
  abi::spDXMeshObservedLayout mesh{};
  abi::spDXVertexBufferLayout vb{};abi::spDXIndexBufferLayout ib{};
  abi::spDXSharedMeshDataObservedLayout shared{};
  std::vector<unsigned char> renderer=std::vector<unsigned char>(0xf368);
  D3DVERTEXELEMENT9 elements[5]={{0,0,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_POSITION,0},
    {0,12,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_NORMAL,0},{0,24,D3DDECLTYPE_D3DCOLOR,0,D3DDECLUSAGE_COLOR,0},
    {0,28,D3DDECLTYPE_FLOAT2,0,D3DDECLUSAGE_TEXCOORD,0},{0xff,0,D3DDECLTYPE_UNUSED,0,0,0}};
  native_update_source::Witness witness{};
  IDirect3DDevice9* Device(){return reinterpret_cast<IDirect3DDevice9*>(&tokens[0]);}
  void* VB(){return &tokens[1];}void* IB(){return &tokens[2];}void* Decl(){return &tokens[3];}
  Fixture(){
    singleton=Ptr(engine);rendererSingleton=Ptr(renderer.data());Put(engine,0x18,Ptr(scene));Put(engine,0x1c,Ptr(&camera));
    Put(scene,0,0x6e7358);Put(scene,0x14,Ptr(&systemRoot));Put(scene,0x38,Ptr(system));Put(system,0x1d4,Ptr(root));
    Put(root,0,0x6e4420);Put(root,0x80,Ptr(scene));Vector(root,0x30,staticList,2);
    systemRoot.base.base.vtableAddress=abi::spNodeVTable;systemRoot.sceneLink=Ptr(scene);
    Put(manager,0,0x6e8cdc);Put(manager,0x14,91);Vector(manager,0x28,selection,2);
    D3DMATRIX world{};world._11=world._22=world._33=world._44=1;
    for(unsigned i=0;i<2;++i){
      auto object=objects[i];staticList[i]=Ptr(object);modelSlots[i]=Ptr(&models[i]);
      Put(object,0,0x6e65e8);Put(object,0x14,0x6e6604);Put(object,0x84,Ptr(object));Put(object,0x88,Ptr(scene));
      Put(object,0x48,Ptr(object)+0x8c);Put(object,0x4c,Ptr(object)+0xcc);Put(object,0x78,41+i);
      memcpy(object+0x8c,&world,64);memcpy(object+0xcc,&world,64);Vector(object+0x14,4,&modelSlots[i],1);
      const float sphere[4]={0,0,i?-5.0f:3.0f,1};memcpy(object+0x38,sphere,sizeof(sphere));
      models[i].base.base.base.vtableAddress=0x6eaa58;models[i].baseMeshData=Ptr(&mesh);
    }
    selection[0]=Ptr(objects[0])+0x14;selection[1]=0;
    camera.base.base.base.vtableAddress=0x6ef1e0;
    camera.viewMatrix[0]=camera.viewMatrix[5]=camera.viewMatrix[10]=camera.viewMatrix[15]=1;
    camera.projectionMatrix[0]=camera.projectionMatrix[5]=camera.projectionMatrix[10]=camera.projectionMatrix[11]=1;
    camera.derivedBasis[3]=0;camera.derivedBasis[4]=0;camera.derivedBasis[5]=1;
    Put(renderer.data(),0,abi::spPCRendererPrimaryVTable);Put(renderer.data(),0xc9e8,Ptr(Device()));
    mesh.base.base.base.base.base.vtableAddress=abi::spDXMeshVTable;mesh.base.base.secondaryVTable=abi::spDXMeshInterfaceVTable;
    mesh.base.base.vertexCount=3;mesh.base.base.primitiveCount=1;mesh.base.base.vertexComponentFlags=0x940;
    mesh.indexType=2;mesh.vertexStride=36;mesh.fvfCode=0x152;mesh.vertexBuffer=Ptr(&vb);mesh.indexBuffer=Ptr(&ib);
    mesh.sharedMeshData=Ptr(&shared);mesh.vertexDeclaration=Ptr(declaration);
    vb.base.vtableAddress=abi::spDXVertexBufferVTable;vb.direct3DVertexBuffer=Ptr(VB());vb.byteSize=3*36;
    ib.base.vtableAddress=abi::spDXIndexBufferVTable;ib.direct3DIndexBuffer=Ptr(IB());ib.byteSize=6;
    shared.base.vtableAddress=abi::spDXSharedMeshDataVTable;shared.vertexBuffer=Ptr(&vb);shared.indexBuffer=Ptr(&ib);
    declaration[0]=0x6f2e58;declaration[5]=mesh.fvfCode;declaration[6]=Ptr(Decl());
    witness.manager=Ptr(manager);witness.scene=Ptr(scene);witness.systemRoot=Ptr(&systemRoot);
    witness.system=Ptr(system);witness.root=Ptr(root);witness.frame=frameId;witness.sequence=73;
    witness.mutation=scene_geometry::MutationSerial();witness.deviceEpoch=native_camera_source::deviceEpoch;
    witness.thread=GetCurrentThreadId();witness.valid=true;
  }
  void Bind(){
    scene_geometry::enginePointerAddress=reinterpret_cast<uintptr_t>(&singleton);
    native_owner_source::enginePointerAddress=reinterpret_cast<uintptr_t>(&singleton);
    native_owner_source::rendererPointerAddress=reinterpret_cast<uintptr_t>(&rendererSingleton);
  }
  void Resources(){
    native_mesh_source::enabled=true;native_mesh_source::ownerThread=GetCurrentThreadId();native_transport_source::enabled=true;
    native_mesh_source::buffers.emplace(VB(),native_mesh_source::Bytes{std::vector<uint8_t>(vb.byteSize),17,Ptr(&shared),Ptr(&vb)});
    native_mesh_source::buffers.emplace(IB(),native_mesh_source::Bytes{std::vector<uint8_t>(ib.byteSize),17,Ptr(&shared),Ptr(&ib)});
    const uint16_t indices[3]={0,1,2};memcpy(native_mesh_source::buffers.at(IB()).data.data(),indices,sizeof(indices));
    native_mesh_source::retainedBytes=vb.byteSize+ib.byteSize;
    surfaceBuffers.emplace(VB(),SurfaceBuffer{native_mesh_source::buffers.at(VB()).data,true});
    surfaceBuffers.emplace(IB(),SurfaceBuffer{native_mesh_source::buffers.at(IB()).data,true});surfaceBufferBytes=vb.byteSize+ib.byteSize;
    Check(native_transport_source::Remember(Device(),VB(),native_transport_source::Kind::Vertex,vb.byteSize,D3DFMT_UNKNOWN),"owned vertex creation witness");
    Check(native_transport_source::Remember(Device(),IB(),native_transport_source::Kind::Index,ib.byteSize,D3DFMT_INDEX16),"owned index creation witness");
    Check(native_transport_source::Remember(Device(),Decl(),native_transport_source::Kind::Declaration,0,D3DFMT_UNKNOWN,elements,5),"owned declaration creation witness");
  }
  std::string NativeBytes()const{
    std::string bytes;auto add=[&](const void* data,size_t size){bytes.append(static_cast<const char*>(data),size);};
    add(scene,sizeof(scene));add(system,sizeof(system));add(root,sizeof(root));add(objects,sizeof(objects));add(manager,sizeof(manager));
    add(&camera,sizeof(camera));add(models,sizeof(models));add(&mesh,sizeof(mesh));add(&vb,sizeof(vb));add(&ib,sizeof(ib));
    add(&shared,sizeof(shared));add(declaration,sizeof(declaration));add(staticList,sizeof(staticList));add(selection,sizeof(selection));add(modelSlots,sizeof(modelSlots));
    return bytes;
  }
};
static Fixture* fixture;
static unsigned passiveCalls,independentCalls,order;
static unsigned originalSelections;
static bool callbackCurrent,independentCurrent,renderScoped;
static void PassiveSpy(scene_geometry::Scope& scope,uint32_t manager,uint32_t scene,uint32_t camera)noexcept{
  ++passiveCalls;callbackCurrent=fixture&&order==0&&scope.manager==0&&scene==Ptr(fixture->scene)&&camera==Ptr(&fixture->camera)&&
    manager==Ptr(fixture->manager)&&Word(fixture->manager,0x2c)==Ptr(fixture->selection)&&fixture->selection[0]==Ptr(fixture->objects[0])+0x14&&
    fixture->selection[1]==0&&Word(fixture->objects[1],0x78)==42;order=1;
}
static void IndependentSpy(scene_geometry::Scope& scope,const scene_geometry::Registry& registry,uint32_t manager){
  ++independentCalls;independentCurrent=order==1&&scope.manager==0&&registry.valid&&registry.supports.size()==2&&
    manager==Ptr(fixture->manager)&&Word(fixture->manager,0x2c)==Ptr(fixture->selection);order=2;
}
static uintptr_t __fastcall MockRender(void*,void*,void*){renderScoped=scene_geometry::active!=nullptr;return 0x98abcdef;}
static uintptr_t __fastcall MockSelect(void*,void*,void*,void*){++originalSelections;return 0xa57f0200;}
static uintptr_t __fastcall RaiseRender(void*,void*,void*){RaiseException(0xe0421001,0,0,nullptr);return 0;}
static bool CatchRender(scene_geometry::Scope* scope){
  __try{scene_geometry::RenderAndRestore(scope,nullptr,nullptr);}
  __except(GetExceptionCode()==0xe0421001?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return true;}
  return false;
}
static void HookBoundaries(){
  Fixture f;fixture=&f;f.Bind();sceneGeometryEnabled=true;sceneGeometryObservationEnabled=true;
  sceneGeometryObserveOnly=keepSceneGeometryForComparison=false;
  scene_geometry::observeOriginalSelection=&PassiveSpy;scene_geometry::observeSelection=&IndependentSpy;
  scene_audit::ownerThread=GetCurrentThreadId();scene_audit::originalSelect=reinterpret_cast<scene_audit::NativeSelect>(&MockSelect);
  const auto before=f.NativeBytes();
  {
    scene_geometry::Scope scope(Ptr(f.scene),Ptr(&f.camera));
    Check(scene_audit::Select(f.manager,nullptr,f.scene,&f.camera)==0xa57f0200&&originalSelections==1,"selection hook preserves full original return and calls original exactly once");
    Check(passiveCalls==1&&callbackCurrent&&independentCalls==1&&independentCurrent&&order==2,"passive callback sees exact original selection before independent observer");
    Check(scope.manager==Ptr(f.manager)&&scope.expanded.size()==3&&scope.expanded[0]==f.selection[0]&&scope.expanded[1]==0,"existing extension executes after both observers");
    Check(Word(f.objects[1],0x78)==91,"extension remains the sole writer of added visibility mark");
  }
  Check(f.NativeBytes()==before&&!scene_geometry::active,"scope restores original selection and marks after extension");
  for(unsigned mode=0;mode<3;++mode){
    passiveCalls=independentCalls=order=0;callbackCurrent=false;
    sceneGeometryEnabled=mode!=0;sceneGeometryObserveOnly=mode==1;keepSceneGeometryForComparison=mode==2;
    scene_geometry::Scope scope(Ptr(f.scene),Ptr(&f.camera));scene_geometry::AfterSelect(Ptr(f.manager),Ptr(f.scene),Ptr(&f.camera));
    Check(passiveCalls==1&&callbackCurrent&&independentCalls==0,"passive path remains separate from disabled, ObserveOnly and comparison extension paths");
    Check(!scope.manager&&scope.expanded.empty()&&f.NativeBytes()==before,"passive path leaves native vector and visibility marks unchanged");
  }
  sceneGeometryEnabled=false;sceneGeometryObserveOnly=keepSceneGeometryForComparison=false;scene_geometry::observeOriginalSelection=nullptr;
  {
    scene_geometry::Scope scope(Ptr(f.scene),Ptr(&f.camera));Put(f.system,0x1d4,0);
    scene_geometry::AfterSelect(Ptr(f.manager),Ptr(f.scene),Ptr(&f.camera));Put(f.system,0x1d4,Ptr(f.root));
    Check(scope.RegistrySnapshot().valid,"null passive callback does not eagerly populate scope registry cache");
  }
  scene_geometry::renderThread=GetCurrentThreadId();scene_geometry::originalRender=reinterpret_cast<scene_geometry::NativeRender>(&MockRender);
  renderScoped=false;Check(scene_geometry::Render(f.scene,nullptr,&f.camera)==0x98abcdef&&renderScoped,"passive-only render hook creates a scope and preserves full native return");
  sceneGeometryObservationEnabled=false;renderScoped=true;
  Check(scene_geometry::Render(f.scene,nullptr,&f.camera)==0x98abcdef&&!renderScoped,"disabled passive and extension flags do not create render scope");
  {
    scene_geometry::Scope scope(Ptr(f.scene),Ptr(&f.camera));scope.ExtendSelection(Ptr(f.manager),scene_geometry::ReadRegistry(Ptr(f.scene)));
    scene_geometry::originalRender=reinterpret_cast<scene_geometry::NativeRender>(&RaiseRender);
    Check(CatchRender(&scope),"original structured exception propagates from unchanged renderer");
    Check(f.NativeBytes()==before&&!scene_geometry::active,"exception finally restores vector, marks and active scope");
  }
  scene_geometry::observeSelection=nullptr;fixture=nullptr;
}
static void SurfaceUploadActivation(){
  Check(surfaceBuffers.empty()&&surfaceWrites.empty()&&!surfaceBufferBytes,"surface upload gate begins without retained capture state");
  const auto oldAuto=autoSurfaceRoles,oldObserver=observer::enabled;const auto oldSkin=skin_packet_capture::directory[0];
  autoSurfaceRoles=false;skin_packet_capture::directory[0]=0;observer::enabled=false;
  Check(!skin_packet_capture::Enabled()&&!CaptureSurfaceInputs(),"both old features and passive observer disabled leave upload capture off");
  uint32_t tokens[3]{};const uint8_t bytes[8]={0x91,0x02,0xe3,0x44,0x55,0xa6,0x77,0x18};
  if(CaptureSurfaceInputs())surfaceWrites[&tokens[0]]={bytes,0,sizeof(bytes),sizeof(bytes),0};
  Check(surfaceWrites.empty()&&surfaceBuffers.empty(),"inactive capture gate records no surface upload");
  observer::enabled=true;
  Check(!autoSurfaceRoles&&!skin_packet_capture::Enabled()&&CaptureSurfaceInputs(),"passive observer alone activates original surface upload capture");
  if(CaptureSurfaceInputs())surfaceWrites[&tokens[0]]={bytes,0,sizeof(bytes),sizeof(bytes),0};
  if(CaptureSurfaceInputs())CaptureSurfaceWrite(&tokens[0]);
  Check(surfaceWrites.empty()&&surfaceBuffers.count(&tokens[0])==1&&surfaceBuffers.at(&tokens[0]).complete&&
    surfaceBuffers.at(&tokens[0]).bytes==std::vector<uint8_t>(bytes,bytes+sizeof(bytes))&&surfaceBufferBytes==sizeof(bytes),
    "observer-only full write completes exact literal bytes and consumes pending write");
  surfaceWrites[&tokens[1]]={bytes+2,2,3,sizeof(bytes),0};CaptureSurfaceWrite(&tokens[1]);
  std::vector<uint8_t> partial(sizeof(bytes));memcpy(partial.data()+2,bytes+2,3);
  Check(!surfaceBuffers.at(&tokens[1]).complete&&surfaceBuffers.at(&tokens[1]).bytes==partial&&surfaceBufferBytes==2*sizeof(bytes),
    "bounded partial capture retains exact written interval without claiming full upload");
  const uint8_t replacement[2]={0xfa,0x35};surfaceWrites[&tokens[0]]={replacement,3,2,sizeof(bytes),D3DLOCK_DISCARD};CaptureSurfaceWrite(&tokens[0]);
  std::vector<uint8_t> discarded(bytes,bytes+sizeof(bytes));memcpy(discarded.data()+3,replacement,sizeof(replacement));
  Check(!surfaceBuffers.at(&tokens[0]).complete&&surfaceBuffers.at(&tokens[0]).bytes==discarded&&surfaceWrites.empty(),
    "partial DISCARD invalidates previous full-upload credit while copying its exact bytes");
  surfaceWrites[&tokens[0]]={bytes,7,2,sizeof(bytes),0};CaptureSurfaceWrite(&tokens[0]);
  Check(!surfaceBuffers.count(&tokens[0])&&!surfaceWrites.count(&tokens[0])&&surfaceBufferBytes==sizeof(bytes),
    "out-of-range write retires previous snapshot without reading beyond literal bytes");
  surfaceWrites[&tokens[2]]={bytes,0,0,8*1024*1024+1,0};CaptureSurfaceWrite(&tokens[2]);
  Check(!surfaceBuffers.count(&tokens[2])&&!surfaceWrites.count(&tokens[2])&&surfaceBufferBytes==sizeof(bytes),
    "oversized capture is rejected before allocating an unbounded snapshot");
  surfaceWrites[&tokens[1]]={bytes,0,sizeof(bytes),sizeof(bytes),0};ForgetSurfaceBuffer(&tokens[1]);
  Check(surfaceBuffers.empty()&&surfaceWrites.empty()&&!surfaceBufferBytes,"fresh resource-address invalidation clears snapshot, pending write and byte accounting");
  observer::enabled=false;autoSurfaceRoles=true;
  Check(CaptureSurfaceInputs()&&!skin_packet_capture::Enabled(),"existing auto-surface feature still activates capture with observer disabled");
  autoSurfaceRoles=false;Check(!CaptureSurfaceInputs(),"disabling all three consumers returns capture gate to inactive");
  autoSurfaceRoles=oldAuto;observer::enabled=oldObserver;skin_packet_capture::directory[0]=oldSkin;
}
// Plain C++ producer mock, not an IUnknown/COM implementation. The owned
// table is only an identity for the existing Original lookup.
struct CaptureBuffer {
  void** table=nullptr;
  uint8_t bytes[8]{0x71,0x22,0xd3,0x54,0x15,0xe6,0x37,0x88};
  unsigned locks=0,unlocks=0,descriptions=0;
  HRESULT lockResult=HRESULT(0x12345678),unlockResult=HRESULT(0x88765abc);
  HRESULT GetDesc(D3DVERTEXBUFFER_DESC* value){
    ++descriptions;*value={};value->Size=sizeof(bytes);value->Type=D3DRTYPE_VERTEXBUFFER;value->Pool=D3DPOOL_MANAGED;return S_OK;
  }
};
static HRESULT STDMETHODCALLTYPE OriginalCaptureLock(CaptureBuffer* buffer,UINT,UINT,void** data,DWORD){++buffer->locks;*data=buffer->bytes;return buffer->lockResult;}
static HRESULT STDMETHODCALLTYPE OriginalCaptureUnlock(CaptureBuffer* buffer){++buffer->unlocks;return buffer->unlockResult;}
static CaptureAllocation failCaptureAllocation=CaptureAllocation::SurfaceWrite;
static unsigned captureFaultHits;
static void ThrowCaptureAllocation(CaptureAllocation point){if(point==failCaptureAllocation){++captureFaultHits;throw std::bad_alloc();}}
static void SurfaceAllocationFailures(){
  static_assert(noexcept(RememberSurfaceWrite(nullptr,SurfaceWrite{})),"pending surface insertion cannot escape native call");
  static_assert(noexcept(CaptureSurfaceWriteNoThrow(nullptr)),"surface snapshot allocation cannot escape native call");
  static_assert(noexcept(RememberBufferLock(nullptr,BufferLock{})),"buffer trace allocation cannot escape native call");
  Check(surfaceBuffers.empty()&&surfaceWrites.empty()&&!surfaceBufferBytes,"allocation fixture starts without retained upload captures");
  const auto oldAuto=autoSurfaceRoles,oldObserver=observer::enabled;const auto oldSkin=skin_packet_capture::directory[0];
  const auto oldLog=logFile;const auto oldRecords=bufferRecords,oldDynamic=dynamicBufferRecords;
  autoSurfaceRoles=false;skin_packet_capture::directory[0]=0;observer::enabled=true;logFile=nullptr;
  void* table[13]{};CaptureBuffer buffer;buffer.table=table;uint32_t unrelated=0;
  Check(!originals.count(table),"owned mock table has no previous original call records");
  originals.emplace(table,std::map<unsigned,void*>{{11,reinterpret_cast<void*>(&OriginalCaptureLock)},{12,reinterpret_cast<void*>(&OriginalCaptureUnlock)}});
  const std::vector<uint8_t> bytes(buffer.bytes,buffer.bytes+sizeof(buffer.bytes));
  surfaceBuffers.emplace(&unrelated,SurfaceBuffer{bytes,true});surfaceBufferBytes=bytes.size();
  auto seed=[&](){
    ForgetSurfaceBuffer(&buffer);surfaceBuffers.emplace(&buffer,SurfaceBuffer{bytes,true});surfaceBufferBytes+=bytes.size();
    surfaceWrites[&buffer]={buffer.bytes,0,sizeof(buffer.bytes),sizeof(buffer.bytes),0};
  };
  auto discarded=[&](){return !surfaceBuffers.count(&buffer)&&!surfaceWrites.count(&buffer)&&surfaceBufferBytes==bytes.size()&&
    surfaceBuffers.at(&unrelated).complete&&surfaceBuffers.at(&unrelated).bytes==bytes;};
  captureAllocationFault=&ThrowCaptureAllocation;failCaptureAllocation=CaptureAllocation::SurfaceWrite;captureFaultHits=0;seed();
  void* data=nullptr;
  const HRESULT locked=BufferLockCall<CaptureBuffer,D3DVERTEXBUFFER_DESC>(&buffer,0,sizeof(buffer.bytes),&data,0);
  Check(locked==buffer.lockResult&&buffer.locks==1&&buffer.descriptions==1&&data==buffer.bytes&&captureFaultHits==1,
    "pending-write allocation failure preserves full original Lock result and single call");
  Check(discarded(),"pending-write allocation failure retires target's stale full capture and pending write only");
  Check(BufferUnlockCall(&buffer)==buffer.unlockResult&&buffer.unlocks==1,"Lock-side capture failure still permits original Unlock and full failure result");
  failCaptureAllocation=CaptureAllocation::SurfaceSnapshot;captureFaultHits=0;seed();
  Check(BufferUnlockCall(&buffer)==buffer.unlockResult&&buffer.unlocks==2&&captureFaultHits==1,
    "snapshot allocation failure cannot bypass original Unlock or replace its HRESULT");
  Check(discarded(),"snapshot allocation failure invalidates both target records and preserves unrelated upload bytes");
  failCaptureAllocation=CaptureAllocation::BufferLock;captureFaultHits=0;seed();
  bufferLocks[&buffer]={buffer.bytes,1,0};
  logFile=_wfsopen(L"geometry-observer-allocation-trace.jsonl",L"wb",_SH_DENYNO);Check(logFile!=nullptr,"open owned diagnostic trace for insertion failure");
  bufferRecords=0;dynamicBufferRecords=0;data=nullptr;
  Check(BufferLockCall<CaptureBuffer,D3DVERTEXBUFFER_DESC>(&buffer,0,sizeof(buffer.bytes),&data,0)==buffer.lockResult&&
    buffer.locks==2&&buffer.descriptions==2&&data==buffer.bytes&&captureFaultHits==1,
    "diagnostic lock-map allocation failure preserves original Lock result and call count");
  Check(!bufferLocks.count(&buffer)&&!bufferRecords&&!dynamicBufferRecords&&surfaceWrites.count(&buffer)==1,
    "failed diagnostic insertion erases stale lock record without discarding independently captured pending write");
  buffer.unlockResult=HRESULT(0x23456701);
  Check(BufferUnlockCall(&buffer)==buffer.unlockResult&&buffer.unlocks==3&&surfaceBuffers.at(&buffer).complete&&
    surfaceBuffers.at(&buffer).bytes==bytes&&!surfaceWrites.count(&buffer),
    "unrelated diagnostic failure leaves successful snapshot and original Unlock result intact");
  captureAllocationFault=nullptr;fclose(logFile);logFile=oldLog;bufferRecords=oldRecords;dynamicBufferRecords=oldDynamic;
  ForgetSurfaceBuffer(&buffer);ForgetSurfaceBuffer(&unrelated);bufferLocks.erase(&buffer);originals.erase(table);
  Check(surfaceBuffers.empty()&&surfaceWrites.empty()&&!surfaceBufferBytes,"allocation fixture releases all owned capture records and accounting");
  autoSurfaceRoles=oldAuto;observer::enabled=oldObserver;skin_packet_capture::directory[0]=oldSkin;
}
static bool witnessCurrent=true;
static unsigned getCalls,qualifyCalls;
enum class LateChange {None,Frame,Camera,Selection,Epoch,Reentry};
static LateChange lateChange=LateChange::None;
static unsigned changeAtQualify=0;
static bool changedLate;
static bool GetWitness(uint32_t scene,uint32_t frame,uint64_t mutation,native_update_source::Witness& result){
  ++getCalls;result={};if(!fixture||!witnessCurrent||scene!=fixture->witness.scene||frame!=fixture->witness.frame||mutation!=fixture->witness.mutation)return false;
  result=fixture->witness;return true;
}
static bool QualifyWitness(const native_update_source::Witness& value,uint32_t scene,uint32_t frame,uint64_t mutation){
  ++qualifyCalls;
  if(fixture&&!changedLate&&lateChange!=LateChange::None&&qualifyCalls==changeAtQualify){
    changedLate=true;
    switch(lateChange){
      case LateChange::Frame:++frameId;break;
      case LateChange::Camera:fixture->camera.viewMatrix[12]=7;break;
      case LateChange::Selection:fixture->selection[1]=Ptr(fixture->objects[1])+0x14;break;
      case LateChange::Epoch:++native_camera_source::deviceEpoch;break;
      case LateChange::Reentry:if(scene_geometry::active)observer::Observe(*scene_geometry::active,Ptr(fixture->manager),Ptr(fixture->scene),Ptr(&fixture->camera));break;
      default:break;
    }
  }
  return fixture&&witnessCurrent&&value.valid&&value.scene==scene&&value.frame==frame&&value.mutation==mutation&&
    value.sequence==fixture->witness.sequence&&value.deviceEpoch==native_camera_source::deviceEpoch;
}
static std::string ReadLog(){
  fflush(observer::output);Check(fseek(observer::output,0,SEEK_END)==0,"seek observer output");const long size=ftell(observer::output);
  Check(size>=0&&size<1024*1024,"bounded observer fixture output");Check(fseek(observer::output,0,SEEK_SET)==0,"rewind observer output");
  std::string value(size,'\0');Check(value.empty()||fread(&value[0],1,value.size(),observer::output)==value.size(),"read complete observer fixture output");
  Check(fseek(observer::output,0,SEEK_END)==0,"restore observer output append position");return value;
}
static void ClearLog(){fflush(observer::output);Check(_chsize_s(_fileno(observer::output),0)==0&&fseek(observer::output,0,SEEK_SET)==0,"clear owned observer log");}
static unsigned Count(const std::string& text,const char* needle){unsigned count=0;size_t at=0;while((at=text.find(needle,at))!=std::string::npos){++count;at+=strlen(needle);}return count;}
static void InitializeGuards(){
  Check(!observer::enabled&&!sceneGeometryEnabled&&!sceneGeometryObservationEnabled,"observer begins opt-in and does not enable extension");
  scene_geometry::observeSelection=&IndependentSpy;SetEnvironmentVariableW(L"WINX_REMIX_GEOMETRY_SNAPSHOT_OBSERVER",nullptr);
  observer::Initialize();Check(!observer::enabled&&!scene_geometry::observeOriginalSelection&&scene_geometry::observeSelection==&IndependentSpy,"absent observer option preserves both existing observer contracts");
  SetEnvironmentVariableW(L"WINX_REMIX_GEOMETRY_SNAPSHOT_OBSERVER",L"geometry-observer-init-rejected.jsonl");
  sceneGeometryObservationEnabled=true;native_update_source::enabled=native_camera_source::enabled=native_mesh_source::enabled=native_transport_source::enabled=false;
  observer::Initialize();Check(!observer::enabled&&!sceneGeometryEnabled&&!scene_geometry::observeOriginalSelection&&scene_geometry::observeSelection==&IndependentSpy,"missing native prerequisites cannot activate observer or visibility extension");
  if(observer::output){fclose(observer::output);observer::output=nullptr;}
  SetEnvironmentVariableW(L"WINX_REMIX_GEOMETRY_SNAPSHOT_OBSERVER",nullptr);sceneGeometryObservationEnabled=false;scene_geometry::observeSelection=nullptr;
}
static DWORD WINAPI ForeignObserve(void* pointer){auto scope=static_cast<scene_geometry::Scope*>(pointer);observer::Observe(*scope,Ptr(fixture->manager),Ptr(fixture->scene),Ptr(&fixture->camera));return 0;}
static void CurrentObservation(){
  frameId=900;Fixture f;fixture=&f;f.Bind();f.Resources();witnessCurrent=true;
  native_owner_source::enabled=native_update_source::enabled=native_camera_source::enabled=true;sceneGeometryObservationEnabled=true;
  native_owner_source::ownerThread=native_update_source::ownerThread=native_camera_source::ownerThread=scene_geometry::renderThread=GetCurrentThreadId();
  observer::getWitness=&GetWitness;observer::qualifyWitness=&QualifyWitness;observer::ownerThread=GetCurrentThreadId();observer::enabled=true;
  observer::output=_wfsopen(L"geometry-observer-fixture.jsonl",L"w+b",_SH_DENYNO);Check(observer::output!=nullptr,"open owned passive observer log");
  native_camera_source::pending={};native_camera_source::pending.valid=true;native_camera_source::pending.scene=Ptr(f.scene);native_camera_source::pending.camera=Ptr(&f.camera);
  native_camera_source::pending.frame=frameId;native_camera_source::pending.sequence=19;native_camera_source::pending.deviceEpoch=native_camera_source::deviceEpoch;
  memcpy(native_camera_source::pending.info.view,f.camera.viewMatrix,64);memcpy(native_camera_source::pending.info.projection,f.camera.projectionMatrix,64);
  const auto before=f.NativeBytes();const auto independentScans=independent_scene_source::scans,submissions=native_mesh_source::submissions,cameraSubmits=native_camera_source::submitted;
  const auto completed=observer::completedScopes,rows=observer::rows,world=observer::worldQualified,mesh=observer::meshQualified;
  scene_geometry::observeSelection=&IndependentSpy;scene_geometry::observeOriginalSelection=&observer::Observe;
  {
    scene_geometry::Scope scope(Ptr(f.scene),Ptr(&f.camera));scene_geometry::AfterSelect(Ptr(f.manager),Ptr(f.scene),Ptr(&f.camera));
    Check(observer::completedScopes==completed+1&&observer::rows==rows+2&&observer::worldQualified==world+2&&observer::meshQualified==mesh+2,"current scope captures selected and unselected rigid resources from full membership");
    Check(getCalls==1&&qualifyCalls>0,"completed observation requires original root witness before and after reads");
    Check(!scope.manager&&scope.expanded.empty()&&f.NativeBytes()==before,"successful diagnostic changes no native storage or selection");
  }
  const auto log=ReadLog();
  Check(Count(log,"\"event\":\"scope\"")==1&&Count(log,"\"originallySelected\":true")==1&&Count(log,"\"originallySelected\":false")==1,"one observation contains original selected and unselected memberships separately");
  Check(Count(log,"\"worldQualified\":true")==2&&Count(log,"\"meshQualified\":true")==2&&log.find("\"completed\":true")!=std::string::npos&&log.find("\"fencesStable\":true")!=std::string::npos,"only completed stable packet publishes qualified world/resource rows");
  Check(independent_scene_source::scans==independentScans&&native_mesh_source::submissions==submissions&&native_camera_source::submitted==cameraSubmits&&!testRemixApi,"observer uses no independent scan, camera submit or mesh/API submission");
  Check(!native_mesh_source::buffers.at(f.VB()).verified&&!native_mesh_source::buffers.at(f.IB()).verified,"diagnostic upload comparisons do not change native-source verification cache");
  Check(scene_geometry::observeSelection==&IndependentSpy,"passive callback does not overwrite independent observer");
  ClearLog();
  {
    scene_geometry::Scope scope(Ptr(f.scene),Ptr(&f.camera));const auto& cached=scope.RegistrySnapshot();const auto* cachedAddress=&cached;const auto* cachedSupports=cached.supports.data();
    Check(cached.valid&&cached.supports.size()==2,"prime immutable scope registry before independent original-selection observation");
    const auto completedBefore=observer::completedScopes,rowsBefore=observer::rows;
    const auto oldMutation=scene_geometry::MutationSerial();const auto oldSecond=f.staticList[1];f.staticList[1]=0;
    observer::Observe(scope,Ptr(f.manager),Ptr(f.scene),Ptr(&f.camera));
    Check(observer::completedScopes==completedBefore+1&&observer::rows==rowsBefore+1,"passive observer reads changed physical membership instead of earlier scope cache");
    const auto freshLog=ReadLog();
    Check(Count(freshLog,"\"event\":\"scope\"")==1&&Count(freshLog,"\"originallySelected\":")==1&&
      freshLog.find("\"supports\":1")!=std::string::npos&&freshLog.find("\"registrySource\":\"fresh_selection_read\"")!=std::string::npos&&
      freshLog.find("\"completed\":true")!=std::string::npos,"fresh-selection packet identifies its own one-support observation");
    Check(&scope.RegistrySnapshot()==cachedAddress&&cached.supports.data()==cachedSupports&&cached.valid&&cached.supports.size()==2&&cached.supports[1]==Ptr(f.objects[1])+0x14&&
      scene_geometry::MutationSerial()==oldMutation,"fresh diagnostic preserves existing immutable cache and makes no retirement or lifetime claim");
    f.staticList[1]=oldSecond;
  }
  Check(f.NativeBytes()==before,"fresh membership fixture restores owned vector without changing native visibility");
  const auto completedAfterFresh=observer::completedScopes;
  const auto rejectBefore=observer::rejectedScopes;ClearLog();witnessCurrent=false;
  {scene_geometry::Scope scope(Ptr(f.scene),Ptr(&f.camera));observer::Observe(scope,Ptr(f.manager),Ptr(f.scene),Ptr(&f.camera));}
  Check(observer::completedScopes==completedAfterFresh&&observer::rejectedScopes==rejectBefore+1&&ReadLog().find("update_witness")!=std::string::npos,"unqualified original update never becomes completed current geometry");witnessCurrent=true;
  auto reject=[&](scene_geometry::Scope& scope,uint32_t manager,uint32_t scene,uint32_t camera,const char* message){
    const auto count=observer::completedScopes,failures=observer::rejectedScopes;observer::Observe(scope,manager,scene,camera);
    Check(observer::completedScopes==count&&observer::rejectedScopes==failures+1&&f.NativeBytes()==before,message);
  };
  {
    scene_geometry::Scope outer(Ptr(f.scene),Ptr(&f.camera));
    {scene_geometry::Scope nested(Ptr(f.scene),Ptr(&f.camera));reject(nested,Ptr(f.manager),Ptr(f.scene),Ptr(&f.camera),"nested render scope rejects instead of borrowing parent observation");}
    reject(outer,Ptr(f.manager),Ptr(f.scene)+4,Ptr(&f.camera),"callback scene mismatch rejects");
    reject(outer,Ptr(f.manager),Ptr(f.scene),Ptr(&f.camera)+4,"callback camera mismatch rejects");
    const auto completedBefore=observer::completedScopes;const auto sizeBefore=ReadLog().size();
    HANDLE thread=CreateThread(nullptr,0,&ForeignObserve,&outer,0,nullptr);Check(thread!=nullptr,"create owned foreign observer thread");
    Check(WaitForSingleObject(thread,3000)==WAIT_OBJECT_0,"foreign observer returns within bounded fixture wait");CloseHandle(thread);
    Check(observer::completedScopes==completedBefore&&ReadLog().size()==sizeBefore,"foreign observer cannot read or publish owner-thread geometry");
  }
  for(unsigned boundary=1;boundary<=2;++boundary)for(auto change:{LateChange::Frame,LateChange::Camera,LateChange::Selection,LateChange::Epoch,LateChange::Reentry}){
    ClearLog();const auto completedBefore=observer::completedScopes,rejectedBefore=observer::rejectedScopes;
    const auto rowsBefore=observer::rows,worldBefore=observer::worldQualified,meshBefore=observer::meshQualified;
    const auto oldFrame=frameId;const auto oldEpoch=native_camera_source::deviceEpoch;
    changedLate=false;lateChange=change;changeAtQualify=qualifyCalls+boundary;
    {scene_geometry::Scope scope(Ptr(f.scene),Ptr(&f.camera));observer::Observe(scope,Ptr(f.manager),Ptr(f.scene),Ptr(&f.camera));}
    Check(changedLate,"late mutation reaches requested first or final qualification boundary");
    Check(observer::completedScopes==completedBefore&&observer::rejectedScopes>rejectedBefore&&observer::rows==rowsBefore&&
      observer::worldQualified==worldBefore&&observer::meshQualified==meshBefore,"late frame, camera, selection, reset or reentry grants no completed row credit");
    const auto rejectedLog=ReadLog();Check(rejectedLog.find("\"completed\":true")==std::string::npos&&rejectedLog.find("\"completed\":false")!=std::string::npos,"late mutation cannot publish a completed stable packet");
    frameId=oldFrame;native_camera_source::deviceEpoch=oldEpoch;f.camera.viewMatrix[12]=0;f.selection[1]=0;lateChange=LateChange::None;
    Check(f.NativeBytes()==before,"late mutation fixture restores owned records between independent samples");
  }
  observer::enabled=false;ClearLog();const auto sampled=observer::sampledScopes;
  {scene_geometry::Scope scope(0,0);observer::Observe(scope,1,2,3);}
  Check(observer::sampledScopes==sampled&&ReadLog().empty(),"disabled observer returns before accessing invalid native arguments");
  Check(f.NativeBytes()==before&&scene_geometry::observeSelection==&IndependentSpy&&!scene_geometry::active,"all passive/rejected observations preserve native bytes and independent callback");
  fclose(observer::output);observer::output=nullptr;observer::getWitness=&native_update_source::GetWitness;observer::qualifyWitness=&native_update_source::QualifyWitness;
  native_mesh_source::buffers.clear();native_mesh_source::retainedBytes=0;native_transport_source::records.clear();native_transport_source::declarationCount=0;
  surfaceBuffers.clear();surfaceBufferBytes=0;
  native_owner_source::enabled=native_mesh_source::enabled=native_transport_source::enabled=native_update_source::enabled=native_camera_source::enabled=false;
  sceneGeometryObservationEnabled=false;scene_geometry::observeOriginalSelection=nullptr;scene_geometry::observeSelection=nullptr;fixture=nullptr;
}
}
int main(){
  try{
    static_assert(noexcept(geometry_snapshot_observer::Observe(std::declval<scene_geometry::Scope&>(),0,0,0)),"passive callback cannot unwind through original selection");
    geometry_observer_test::InitializeGuards();geometry_observer_test::HookBoundaries();geometry_observer_test::SurfaceUploadActivation();geometry_observer_test::SurfaceAllocationFailures();geometry_observer_test::CurrentObservation();
    printf("{\"status\":\"PASS\",\"checks\":%u,\"nativeGameCodeExecuted\":false,\"gpu\":false}\n",geometry_observer_test::checks);return 0;
  }catch(const std::exception& error){fprintf(stderr,"FAIL %s\n",error.what());return 1;}
}
