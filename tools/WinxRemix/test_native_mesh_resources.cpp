// Own bounded CPU fixture. Literal ABI records and lifecycle tokens exercise
// adapter guards only; no game code, device, bridge, renderer or GPU executes.
#define WINX_REMIX_TEST
#include "winx_d3d9_probe.cpp"
#include <stdexcept>
#include <limits>

namespace mesh_resources_test {
namespace source=native_mesh_source;namespace abi=sparkplug::evidence::pc;
static unsigned checks;
static void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
static uint32_t Ptr(const void* value){return uint32_t(reinterpret_cast<uintptr_t>(value));}
struct Watchdog {
  HANDLE event=nullptr,thread=nullptr;
  static DWORD WINAPI Wait(void* event){if(WaitForSingleObject(event,30000)==WAIT_TIMEOUT)TerminateProcess(GetCurrentProcess(),0xe0520b30u);return 0;}
  Watchdog(){event=CreateEventW(nullptr,TRUE,FALSE,nullptr);Check(event!=nullptr,"watchdog event");thread=CreateThread(nullptr,0,Wait,event,0,nullptr);Check(thread!=nullptr,"watchdog thread");}
  ~Watchdog(){SetEvent(event);WaitForSingleObject(thread,1000);CloseHandle(thread);CloseHandle(event);}
};
struct Fixture {
  std::vector<uint8_t> renderer=std::vector<uint8_t>(0xca10);
  abi::spDXMeshObservedLayout mesh{};abi::spDXVertexBufferLayout vb{};abi::spDXIndexBufferLayout ib{};
  abi::spDXSharedMeshDataObservedLayout shared{};
  uint32_t declaration[7]{},tokens[4]{0xdead0001,0xdead0002,0xdead0003,0xdead0004};
  D3DMATRIX world{};
  D3DVERTEXELEMENT9 elements[5]={{0,0,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_POSITION,0},
    {0,12,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_NORMAL,0},{0,24,D3DDECLTYPE_D3DCOLOR,0,D3DDECLUSAGE_COLOR,0},
    {0,28,D3DDECLTYPE_FLOAT2,0,D3DDECLUSAGE_TEXCOORD,0},{0xff,0,D3DDECLTYPE_UNUSED,0,0,0}};
  source::TransportWitness witness{};
  IDirect3DDevice9* Device(){return reinterpret_cast<IDirect3DDevice9*>(&tokens[0]);}
  void* VB(){return &tokens[1];}void* IB(){return &tokens[2];}void* Decl(){return &tokens[3];}
  void DeviceIdentity(uint32_t device){memcpy(renderer.data()+0xc9e8,&device,4);}
  Fixture(){
    memcpy(renderer.data(),&abi::spPCRendererPrimaryVTable,4);DeviceIdentity(Ptr(Device()));
    mesh.base.base.base.base.base.vtableAddress=abi::spDXMeshVTable;mesh.base.base.secondaryVTable=abi::spDXMeshInterfaceVTable;
    mesh.base.base.vertexCount=4;mesh.base.base.primitiveCount=2;mesh.base.base.vertexComponentFlags=0x940;
    mesh.indexType=3;mesh.indexBegin=2;mesh.vertexBegin=1;mesh.vertexStride=36;mesh.fvfCode=0x152;
    mesh.vertexBuffer=Ptr(&vb);mesh.indexBuffer=Ptr(&ib);mesh.sharedMeshData=Ptr(&shared);mesh.vertexDeclaration=Ptr(declaration);
    vb.base.vtableAddress=abi::spDXVertexBufferVTable;vb.direct3DVertexBuffer=Ptr(VB());vb.byteSize=8*36;
    ib.base.vtableAddress=abi::spDXIndexBufferVTable;ib.direct3DIndexBuffer=Ptr(IB());ib.byteSize=24;
    shared.base.vtableAddress=abi::spDXSharedMeshDataVTable;shared.vertexBuffer=Ptr(&vb);shared.indexBuffer=Ptr(&ib);
    declaration[0]=0x6f2e58;declaration[5]=mesh.fvfCode;declaration[6]=Ptr(Decl());
    world._11=world._22=world._33=world._44=1;world._41=3;world._42=-7;world._43=12;
    witness={true,Device(),VB(),IB(),Decl(),81,82,83,vb.byteSize,ib.byteSize,D3DFMT_INDEX16,elements,5};
    source::buffers.emplace(VB(),source::Bytes{std::vector<uint8_t>(vb.byteSize,0x39),17,Ptr(&shared),Ptr(&vb)});
    source::buffers.emplace(IB(),source::Bytes{std::vector<uint8_t>(ib.byteSize,0x2a),17,Ptr(&shared),Ptr(&ib)});
    source::retainedBytes=vb.byteSize+ib.byteSize;
  }
};
static void Run(){
  Check(!source::enabled&&!source::active&&source::buffers.empty()&&source::layouts.empty(),"fresh standalone source state");
  Check(!testRemixApi&&!native_owner_source::activeModel&&!native_owner_source::activeSupport,"no API or native model/support scope");
  const auto oldThread=source::ownerThread;source::ownerThread=GetCurrentThreadId();source::enabled=true;
  const auto beforeMatched=source::matched,beforeUsed=source::used,beforeFailures=source::failures;
  Fixture f;std::unique_lock<std::recursive_mutex> borrow(guard);source::Geometry geometry{};
  auto resolve=[&](){return source::ResolveCurrentResources(borrow,f.Device(),Ptr(&f.mesh),Ptr(f.renderer.data()),f.world,f.witness,geometry);};
  auto reject=[&](const char* message){Check(!resolve(),message);Check(!geometry.vertices&&!geometry.indices&&!geometry.layout,"rejection clears borrowed result");};
  Check(resolve(),"current resources resolve without bound D3D or active native scope");
  Check(geometry.vertices==&source::buffers.at(f.VB())&&geometry.indices==&source::buffers.at(f.IB()),"output borrows exact native byte records");
  Check(geometry.range.type==D3DPT_TRIANGLESTRIP&&geometry.range.base==1&&geometry.range.minimum==0&&geometry.range.vertices==4&&
    geometry.range.start==2&&geometry.range.count==2&&geometry.stride==36,"output retains nonzero native base/index offsets and count");
  Check(geometry.mesh==Ptr(&f.mesh)&&geometry.renderer==Ptr(f.renderer.data())&&!memcmp(&geometry.world,&f.world,64),"current mesh/renderer and caller world retained");
  Check(geometry.nativeDeclaration==Ptr(f.declaration)&&geometry.vertexBuffer==f.VB()&&geometry.indexBuffer==f.IB()&&geometry.declaration==f.Decl()&&
    geometry.vertexGeneration==81&&geometry.indexGeneration==82&&geometry.declarationGeneration==83,"exact transport identity and three creation generations retained");
  Check(!geometry.submission&&!geometry.owner.valid&&geometry.componentFlags==0x940&&source::EqualLayout(geometry,f.elements,5),"CPU read grants no owner/submission credit and shares native layout");
  Check(!geometry.vertices->verified&&!geometry.indices->verified&&source::matched==beforeMatched&&source::used==beforeUsed,"read does not verify uploads or issue submission counters");
  source::Scope unrelated{};source::active=&unrelated;Check(resolve(),"unrelated invalid active scope cannot influence independent reader");source::active=nullptr;
  f.mesh.indexType=2;Check(resolve()&&geometry.range.type==D3DPT_TRIANGLELIST,"list resources accepted with six index elements");f.mesh.indexType=3;
  auto& vertices=source::buffers.at(f.VB());auto& indices=source::buffers.at(f.IB());
  vertices.partial=indices.partial=true;vertices.ranges={{36,180}};indices.ranges={{4,12}};
  Check(resolve(),"only the full used CPU intervals need coverage, not unused buffer tails");
  vertices.ranges={{36,72},{108,180}};reject("gap in used vertex interval rejects");vertices.ranges={{36,180}};
  indices.ranges={{4,6},{8,12}};reject("gap in used index interval rejects");indices.ranges={{4,12}};
  vertices.ranges={{0,144}};reject("coverage at wrong vertex base rejects");vertices.ranges={{36,180}};
  indices.ranges={{0,8}};reject("coverage at wrong index offset rejects");indices.ranges={{4,12}};
  f.mesh.indexType=2;reject("list requires six rather than strip four index words");indices.ranges={{4,16}};Check(resolve(),"expanded list coverage qualifies");f.mesh.indexType=3;
  vertices.partial=indices.partial=false;vertices.ranges.clear();indices.ranges.clear();
  ++indices.generation;reject("mixed CPU capture generations reject");--indices.generation;
  vertices.generation=indices.generation=0;reject("zero incomplete capture generation rejects");vertices.generation=indices.generation=17;
  ++vertices.owner;reject("wrong shared owner rejects");--vertices.owner;
  ++indices.nativeBuffer;reject("reused COM token bound to wrong native header rejects");--indices.nativeBuffer;
  indices.data.pop_back();reject("native byte size must match current buffer size");indices.data.push_back(0x2a);
  ++f.shared.indexBuffer;reject("current shared owner must still point to same buffers");--f.shared.indexBuffer;
  ++f.shared.base.vtableAddress;reject("foreign shared owner class rejects");--f.shared.base.vtableAddress;
  f.mesh.sharedMeshData=0;vertices.owner=indices.owner=0;Check(resolve(),"combiner resources with no shared owner qualify");
  f.mesh.sharedMeshData=Ptr(&f.shared);vertices.owner=indices.owner=Ptr(&f.shared);
  f.mesh.componentWeightCount=1;reject("weighted mesh rejects");f.mesh.componentWeightCount=0;
  f.mesh.base.base.vertexComponentFlags|=0x20;reject("packed vertex flags reject even with zero weight count");f.mesh.base.base.vertexComponentFlags=0x940;
  f.mesh.indexType=1;reject("unsupported topology rejects");f.mesh.indexType=3;
  f.mesh.vertexBegin=0x80000000u;reject("signed base overflow rejects");f.mesh.vertexBegin=1;
  f.mesh.vertexStride=35;reject("declaration input extends past stride");f.mesh.vertexStride=36;
  f.mesh.base.base.vertexCount=0xffffffffu;reject("vertex range multiplication cannot wrap");f.mesh.base.base.vertexCount=4;
  f.mesh.base.base.primitiveCount=0xffffffffu;reject("index range multiplication cannot wrap");f.mesh.base.base.primitiveCount=2;
  f.mesh.indexBegin=0xffffffffu;reject("index start cannot wrap");f.mesh.indexBegin=2;
  f.mesh.base.base.vertexCount=0;reject("empty vertex cohort rejects");f.mesh.base.base.vertexCount=4;
  ++f.mesh.base.base.base.base.base.vtableAddress;reject("exact mesh primary required");--f.mesh.base.base.base.base.base.vtableAddress;
  ++f.mesh.base.base.secondaryVTable;reject("exact mesh secondary required");--f.mesh.base.base.secondaryVTable;
  ++f.vb.base.vtableAddress;reject("exact VB header required");--f.vb.base.vtableAddress;
  ++f.ib.base.vtableAddress;reject("exact IB header required");--f.ib.base.vtableAddress;
  ++f.vb.direct3DVertexBuffer;reject("current native VB COM identity required");--f.vb.direct3DVertexBuffer;
  ++f.ib.direct3DIndexBuffer;reject("current native IB COM identity required");--f.ib.direct3DIndexBuffer;
  f.DeviceIdentity(Ptr(f.Decl()));reject("renderer must still own expected transport device");f.DeviceIdentity(Ptr(f.Device()));
  ++f.declaration[0];reject("exact native declaration class required");--f.declaration[0];
  ++f.declaration[5];reject("reinitialized declaration FVF differs from mesh");--f.declaration[5];
  ++f.declaration[6];reject("reinitialized declaration COM identity differs from witness");--f.declaration[6];
  ++f.elements[3].Offset;reject("actual declaration bytes must match shared emitter");--f.elements[3].Offset;
  --f.witness.elementCount;reject("truncated declaration rejects");++f.witness.elementCount;
  f.witness.valid=false;reject("missing live transport record rejects");f.witness.valid=true;
  f.witness.device=reinterpret_cast<IDirect3DDevice9*>(f.Decl());reject("foreign transport device rejects");f.witness.device=f.Device();
  for(auto generation:{&f.witness.vertexGeneration,&f.witness.indexGeneration,&f.witness.declarationGeneration}) {
    const auto saved=*generation;*generation=0;reject("unqualified Create generation rejects");*generation=saved;
  }
  ++f.witness.vertexBytes;reject("Create VB size must agree with native header");--f.witness.vertexBytes;
  ++f.witness.indexBytes;reject("Create IB size must agree with native header");--f.witness.indexBytes;
  f.witness.indexFormat=D3DFMT_INDEX32;reject("native capture is specifically index16");f.witness.indexFormat=D3DFMT_INDEX16;
  f.witness.elements=nullptr;reject("absent actual declaration elements reject");f.witness.elements=f.elements;
  f.world._41=std::numeric_limits<float>::infinity();reject("nonfinite world cannot enter independent geometry");f.world._41=3;
  source::enabled=false;reject("disabled source rejects");source::enabled=true;
  ++source::ownerThread;reject("foreign owner thread rejects");--source::ownerThread;
  borrow.unlock();reject("caller must hold borrow lock");borrow.lock();
  std::recursive_mutex other;std::unique_lock<std::recursive_mutex> wrong(other);
  Check(!source::ResolveCurrentResources(wrong,f.Device(),Ptr(&f.mesh),Ptr(f.renderer.data()),f.world,f.witness,geometry),"holding a different mutex cannot qualify");
  const auto savedVB=vertices;source::Forget(f.VB());reject("observed writable Lock/final Release invalidation removes byte provenance");
  source::buffers.emplace(f.VB(),savedVB);source::retainedBytes+=savedVB.data.size();Check(resolve(),"new capture restores only current pair provenance");
  // The earlier vertices reference is invalid after Forget and is not reused.
  auto& currentV=source::buffers.at(f.VB());currentV.generation=23;indices.generation=23;
  Check(resolve()&&geometry.vertices->generation==23,"same addresses resolve current recaptured generation");
  Check(!geometry.vertices->verified&&!geometry.indices->verified&&source::matched==beforeMatched&&source::used==beforeUsed&&source::failures==beforeFailures,
    "all independent reads preserve upload verification/submission accounting");
  source::buffers.clear();source::layouts.clear();source::retainedBytes=0;source::invalidations=0;source::enabled=false;source::ownerThread=oldThread;
  Check(!source::active&&!testRemixApi&&source::buffers.empty()&&source::layouts.empty(),"fixture restores owned caches and scopes");
}
}
int main(){try{mesh_resources_test::Watchdog watchdog;mesh_resources_test::Run();
  printf("{\"status\":\"PASS\",\"checks\":%u,\"gpu\":false,\"nativeGameCodeExecuted\":false,\"comMethodsCalled\":0}\n",mesh_resources_test::checks);return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL %s\n",error.what());return 1;}}
