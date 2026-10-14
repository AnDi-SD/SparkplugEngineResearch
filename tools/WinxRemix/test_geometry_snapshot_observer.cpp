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
  const auto rejectBefore=observer::rejectedScopes;ClearLog();witnessCurrent=false;
  {scene_geometry::Scope scope(Ptr(f.scene),Ptr(&f.camera));observer::Observe(scope,Ptr(f.manager),Ptr(f.scene),Ptr(&f.camera));}
  Check(observer::completedScopes==completed+1&&observer::rejectedScopes==rejectBefore+1&&ReadLog().find("update_witness")!=std::string::npos,"unqualified original update never becomes completed current geometry");witnessCurrent=true;
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
    geometry_observer_test::InitializeGuards();geometry_observer_test::HookBoundaries();geometry_observer_test::CurrentObservation();
    printf("{\"status\":\"PASS\",\"checks\":%u,\"nativeGameCodeExecuted\":false,\"gpu\":false}\n",geometry_observer_test::checks);return 0;
  }catch(const std::exception& error){fprintf(stderr,"FAIL %s\n",error.what());return 1;}
}
