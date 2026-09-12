// Own PC native data boundary. Original methods still run, including callbacks
// and uploads. Capture shared raw arrays and ordinary spMesh CPU buffer inputs
// before D3D upload. Later D3D writes/releases invalidate that provenance.
#pragma once
#include <algorithm>
#include "../../Sparkplug/Analysis/PC/SparkplugAbi.h"
namespace native_mesh_source {
namespace abi = sparkplug::evidence::pc;
struct DrawRange { D3DPRIMITIVETYPE type; INT base; UINT minimum,vertices,start,count; };
struct Bytes {
  std::vector<uint8_t> data; uint64_t generation=0; uint32_t owner=0,nativeBuffer=0; mutable bool verified=false;
  bool partial=false;
  std::vector<std::pair<size_t,size_t>> ranges;
  const char* source="native_shared_mesh_initializer";
};
static bool Covered(const Bytes& bytes,size_t begin,size_t size) {
  if(begin>bytes.data.size()||size>bytes.data.size()-begin)return false;
  if(!bytes.partial)return true;
  for(const auto& range:bytes.ranges)if(begin>=range.first&&begin+size<=range.second)return true;
  return false;
}
static bool EqualsUpload(const Bytes& bytes,const std::vector<uint8_t>& uploaded) {
  if(bytes.data.size()!=uploaded.size())return false;
  if(!bytes.partial)return bytes.data==uploaded;
  for(const auto& range:bytes.ranges)
    if(memcmp(bytes.data.data()+range.first,uploaded.data()+range.first,range.second-range.first))return false;
  return true;
}
static void PutRange(Bytes& bytes,size_t begin,const std::vector<uint8_t>& source) {
  memcpy(bytes.data.data()+begin,source.data(),source.size());bytes.verified=false;
  bytes.ranges.emplace_back(begin,begin+source.size());
  std::sort(bytes.ranges.begin(),bytes.ranges.end());
  size_t count=0;
  for(const auto& range:bytes.ranges) {
    if(count&&range.first<=bytes.ranges[count-1].second)bytes.ranges[count-1].second=(std::max)(bytes.ranges[count-1].second,range.second);
    else bytes.ranges[count++]=range;
  }
  bytes.ranges.resize(count);
}
struct Geometry {
  const Bytes* vertices=nullptr; const Bytes* indices=nullptr;
  uint32_t mesh=0,renderer=0,stride=0; uint64_t submission=0;
  DrawRange range{}; D3DMATRIX world{};
};
static FILE* output;
static bool enabled,submitEnabled;
static size_t retainedBytes;
static uint64_t nextGeneration,nextSubmission;
static unsigned captures,invalidations,failures,limited,submissions,matched,used,uploadMismatches;
static std::map<void*,Bytes> buffers;
static void Forget(void* object) {
  std::lock_guard<std::recursive_mutex> lock(guard);
  const auto it=buffers.find(object);if(it==buffers.end())return;
  retainedBytes-=it->second.data.size();buffers.erase(it);++invalidations;
}
#if defined(_M_IX86)
static DWORD ownerThread;
template<class T> static bool Read(uintptr_t address,T& value) {
  return scene_geometry::Read(address,&value,sizeof(value));
}
static bool ReadBytes(uint32_t address,std::vector<uint8_t>& bytes,uint32_t count) {
  if(!count||count>8*1024*1024||address<0x10000||uint64_t(address)+count>=0x7fff0000)return false;
  bytes.resize(count);
  for(size_t offset=0;offset<count;offset+=16384) {
    const auto amount=(std::min)(size_t(16384),size_t(count)-offset);
    if(!scene_geometry::Read(address+offset,bytes.data()+offset,amount))return false;
  }
  return true;
}
struct Scope {
  Scope* parent;
  uint32_t mesh,renderer; uint64_t sequence;
  abi::spDXMeshObservedLayout value{};
  bool valid=false;
};
static thread_local Scope* active;
using NativeSubmit=uint32_t(__thiscall*)(void*,uint32_t);
using NativeSharedInitialize=uint32_t(__thiscall*)(void*,uint32_t,uint32_t,uint32_t,uint32_t);
using NativeMeshInitialize=uint32_t(__thiscall*)(void*,uint32_t,uint32_t,uint32_t);
static NativeSubmit originalSubmit;
static NativeSharedInitialize originalSharedInitialize;
static NativeMeshInitialize originalMeshInitialize;
static uint32_t __fastcall Submit(void* rendererInterface,void*,uint32_t mesh) {
  if(GetCurrentThreadId()!=ownerThread)return originalSubmit(rendererInterface,mesh);
  Scope scope{active,mesh,uint32_t(reinterpret_cast<uintptr_t>(rendererInterface)-0x18),++nextSubmission};
  scope.valid=scene_geometry::Word(scope.renderer)==abi::spPCRendererPrimaryVTable && Read(mesh,scope.value) &&
    scene_geometry::Word(mesh)==abi::spDXMeshVTable && scope.value.base.base.secondaryVTable==abi::spDXMeshInterfaceVTable;
  struct Restore {Scope* previous;~Restore(){active=previous;}} restore{active};
  active=&scope;++submissions;
  return originalSubmit(rendererInterface,mesh);
}
static uint32_t __fastcall SharedInitialize(void* object,void*,uint32_t indexSize,uint32_t vertexSize,
                                          uint32_t indexData,uint32_t vertexData) {
  // The original function takes (indexByteSize, vertexByteSize, indexData,
  // vertexData), as proven by PC4C29C0 and its complete native payload fixture.
  if(!enabled||GetCurrentThreadId()!=ownerThread)return originalSharedInitialize(object,indexSize,vertexSize,indexData,vertexData);
  std::lock_guard<std::recursive_mutex> lock(guard);
  std::vector<uint8_t> indices,vertices;
  bool captured=false;
  try {
    if(buffers.size()+2>4096||uint64_t(retainedBytes)+indexSize+vertexSize>64*1024*1024)++limited;
    else captured=ReadBytes(indexData,indices,indexSize)&&ReadBytes(vertexData,vertices,vertexSize);
  }catch(...){++failures;}
  const auto result=originalSharedInitialize(object,indexSize,vertexSize,indexData,vertexData);
  if(!(result&255)||!captured)return result;
  try {
    const auto owner=uint32_t(reinterpret_cast<uintptr_t>(object));
    abi::spDXSharedMeshDataObservedLayout shared{};
    abi::spDXVertexBufferLayout vb{};abi::spDXIndexBufferLayout ib{};
    if(!Read(owner,shared)||shared.base.vtableAddress!=abi::spDXSharedMeshDataVTable||
       !Read(shared.vertexBuffer,vb)||vb.base.vtableAddress!=abi::spDXVertexBufferVTable||vb.byteSize!=vertexSize||
       !Read(shared.indexBuffer,ib)||ib.base.vtableAddress!=abi::spDXIndexBufferVTable||ib.byteSize!=indexSize||
       !vb.direct3DVertexBuffer||!ib.direct3DIndexBuffer){++failures;return result;}
    void* v=reinterpret_cast<void*>(vb.direct3DVertexBuffer);void* i=reinterpret_cast<void*>(ib.direct3DIndexBuffer);
    Forget(v);Forget(i);
    const auto generation=++nextGeneration;
    // Keep accounting correct if one map allocation fails. Partial capture is
    // never eligible: Resolve requires the same generation for both buffers.
    buffers.emplace(v,Bytes{std::move(vertices),generation,owner,shared.vertexBuffer});retainedBytes+=vertexSize;
    buffers.emplace(i,Bytes{std::move(indices),generation,owner,shared.indexBuffer});retainedBytes+=indexSize;
    ++captures;
    if(output&&_ftelli64(output)<16*1024*1024)
      fprintf(output,"{\"event\":\"capture\",\"generation\":%llu,\"owner\":%u,\"nativeVB\":%u,\"nativeIB\":%u,\"vertexBytes\":%u,\"indexBytes\":%u}\n",
        generation,owner,shared.vertexBuffer,shared.indexBuffer,vertexSize,indexSize);
  }catch(...){++failures;}
  return result;
}
static uint32_t __fastcall MeshInitialize(void* meshInterface,void*,uint32_t indexObject,uint32_t vertexObject,uint32_t keepCPU) {
  if(!enabled||GetCurrentThreadId()!=ownerThread)return originalMeshInitialize(meshInterface,indexObject,vertexObject,keepCPU);
  std::lock_guard<std::recursive_mutex> lock(guard);
  abi::spIndexBufferLayout indices{};abi::spVertexBufferLayout vertices{};
  std::vector<uint8_t> indexBytes,vertexBytes;
  bool captured=false;
  try {
    // PC429A40 -> 4AA000, shared recovered BuildVertexBytesForAnalysis:
    // without weights/packed indices the authored vertex stream is copied
    // unchanged. The packed/weighted conversion stays on the existing path.
    if(Read(indexObject,indices)&&indices.base.vtableAddress==abi::spIndexBufferVTable&&indices.initialized&&
       !(indices.formatFlags&1)&&Read(vertexObject,vertices)&&vertices.base.vtableAddress==abi::spVertexBufferVTable&&
       vertices.initialized&&!(vertices.componentFlags&0x3e)&&vertices.vertexStride>=12&&
       uint64_t(vertices.vertexStride)*vertices.vertexCount<=vertices.vertexSize&&indices.indexCount<=4*1024*1024)
      captured=ReadBytes(indices.indexData,indexBytes,indices.indexCount*2)&&
        ReadBytes(vertices.vertexData,vertexBytes,vertices.vertexStride*vertices.vertexCount);
  }catch(...){++failures;}
  const auto result=originalMeshInitialize(meshInterface,indexObject,vertexObject,keepCPU);
  if(!(result&255)||!captured)return result;
  try {
    const auto meshAddress=uint32_t(reinterpret_cast<uintptr_t>(meshInterface)-0x14);
    abi::spDXMeshObservedLayout mesh{};abi::spDXVertexBufferLayout vb{};abi::spDXIndexBufferLayout ib{};
    if(!Read(meshAddress,mesh)||scene_geometry::Word(meshAddress)!=abi::spDXMeshVTable||mesh.sharedMeshData||
       mesh.vertexStride!=vertices.vertexStride||mesh.base.base.vertexCount!=vertices.vertexCount||
       mesh.base.base.vertexComponentFlags!=vertices.componentFlags||mesh.indexType!=indices.type||
       !Read(mesh.vertexBuffer,vb)||vb.base.vtableAddress!=abi::spDXVertexBufferVTable||
       !Read(mesh.indexBuffer,ib)||ib.base.vtableAddress!=abi::spDXIndexBufferVTable)return result;
    const uint64_t vBegin=uint64_t(mesh.vertexBegin)*mesh.vertexStride,iBegin=uint64_t(mesh.indexBegin)*2;
    if(!vb.direct3DVertexBuffer||!ib.direct3DIndexBuffer||vb.byteSize>8*1024*1024||ib.byteSize>8*1024*1024||
       vBegin+vertexBytes.size()>vb.byteSize||iBegin+indexBytes.size()>ib.byteSize){++failures;return result;}
    void* v=reinterpret_cast<void*>(vb.direct3DVertexBuffer);void* i=reinterpret_cast<void*>(ib.direct3DIndexBuffer);
    auto foundV=buffers.find(v),foundI=buffers.find(i);
    if(foundV==buffers.end()||foundI==buffers.end()||foundV->second.generation!=foundI->second.generation||
       foundV->second.nativeBuffer!=mesh.vertexBuffer||foundI->second.nativeBuffer!=mesh.indexBuffer||
       foundV->second.data.size()!=vb.byteSize||foundI->second.data.size()!=ib.byteSize||
       foundV->second.owner||foundI->second.owner||!foundV->second.partial||!foundI->second.partial) {
      Forget(v);Forget(i);
      if(buffers.size()+2>4096||uint64_t(retainedBytes)+vb.byteSize+ib.byteSize>64*1024*1024){++limited;return result;}
      foundV=buffers.emplace(v,Bytes{std::vector<uint8_t>(vb.byteSize),0,0,mesh.vertexBuffer,false,true,{},"native_mesh_cpu_input"}).first;retainedBytes+=vb.byteSize;
      foundI=buffers.emplace(i,Bytes{std::vector<uint8_t>(ib.byteSize),0,0,mesh.indexBuffer,false,true,{},"native_mesh_cpu_input"}).first;retainedBytes+=ib.byteSize;
    }
    // Bound metadata as well as byte storage, even for disjoint tiny ranges.
    if(foundV->second.ranges.size()>=4096||foundI->second.ranges.size()>=4096){++limited;return result;}
    PutRange(foundV->second,size_t(vBegin),vertexBytes);PutRange(foundI->second,size_t(iBegin),indexBytes);
    const auto generation=++nextGeneration;foundV->second.generation=foundI->second.generation=generation;++captures;
    if(output&&_ftelli64(output)<16*1024*1024)
      fprintf(output,"{\"event\":\"capture_range\",\"generation\":%llu,\"mesh\":%u,\"nativeVB\":%u,\"nativeIB\":%u,\"vertexBegin\":%llu,\"vertexBytes\":%zu,\"indexBegin\":%llu,\"indexBytes\":%zu}\n",
        generation,meshAddress,mesh.vertexBuffer,mesh.indexBuffer,vBegin,vertexBytes.size(),iBegin,indexBytes.size());
  }catch(...){++failures;}
  return result;
}
static bool Resolve(IDirect3DDevice9* device,const DrawRange& draw,void* boundVB,void* boundIB,
                    UINT streamOffset,UINT stride,Geometry& geometry) {
  if(!enabled||!active||!active->valid||GetCurrentThreadId()!=ownerThread)return false;
  const auto& scope=*active;const auto& mesh=scope.value;
  if(mesh.componentWeightCount)return false;
  // PC4BC290's table maps native type2/3 to triangle-list/strip. All other
  // primitive types stay with their original producer and D3D path.
  const auto primitive=mesh.indexType==2?D3DPT_TRIANGLELIST:mesh.indexType==3?D3DPT_TRIANGLESTRIP:D3DPT_POINTLIST;
  if((mesh.indexType!=2&&mesh.indexType!=3)||draw.type!=primitive||draw.base<0||
     uint32_t(draw.base)!=mesh.vertexBegin||draw.minimum||draw.vertices!=mesh.base.base.vertexCount||
     draw.start!=mesh.indexBegin||draw.count!=mesh.base.base.primitiveCount||streamOffset||stride!=mesh.vertexStride)return false;
  abi::spRendererDrawContextObservedLayout state{};
  abi::spDXVertexBufferLayout vb{};abi::spDXIndexBufferLayout ib{};
  if(!Read(scope.renderer+abi::spRendererDrawContextOffset,state)||state.device!=reinterpret_cast<uintptr_t>(device)||
     state.vertexBuffer!=mesh.vertexBuffer||state.indexBuffer!=mesh.indexBuffer||state.vertexDeclaration!=mesh.vertexDeclaration||
     !Read(mesh.vertexBuffer,vb)||vb.base.vtableAddress!=abi::spDXVertexBufferVTable||vb.direct3DVertexBuffer!=reinterpret_cast<uintptr_t>(boundVB)||
     !Read(mesh.indexBuffer,ib)||ib.base.vtableAddress!=abi::spDXIndexBufferVTable||ib.direct3DIndexBuffer!=reinterpret_cast<uintptr_t>(boundIB))return false;
  abi::spDXMaterialObservedLayout material{};
  if(!Read(state.selectedMaterial,material)||material.base.base.vtableAddress!=abi::spDXMaterialPrimaryVTable||
     material.base.materialVTable!=abi::spDXMaterialInterfaceVTable||material.base.passCount!=1)return false;
  const auto v=buffers.find(boundVB),i=buffers.find(boundIB);
  if(v==buffers.end()||i==buffers.end()||v->second.generation!=i->second.generation||
     v->second.owner!=mesh.sharedMeshData||i->second.owner!=mesh.sharedMeshData||
     v->second.nativeBuffer!=mesh.vertexBuffer||i->second.nativeBuffer!=mesh.indexBuffer||
     v->second.data.size()!=vb.byteSize||i->second.data.size()!=ib.byteSize)return false;
  const uint64_t indexCount=mesh.indexType==2?uint64_t(mesh.base.base.primitiveCount)*3:uint64_t(mesh.base.base.primitiveCount)+2;
  const uint64_t vertexBegin=uint64_t(mesh.vertexBegin)*stride,vertexBytes=uint64_t(mesh.base.base.vertexCount)*stride;
  if(vertexBegin>vb.byteSize||vertexBytes>vb.byteSize-vertexBegin||uint64_t(mesh.indexBegin)*2+indexCount*2>ib.byteSize||
     !Covered(v->second,size_t(vertexBegin),size_t(vertexBytes))||!Covered(i->second,size_t(mesh.indexBegin)*2,size_t(indexCount)*2))return false;
  D3DMATRIX world{},boundWorld{};
  // Same native renderer world input used by recovered PC4AD540 matrix refresh.
  if(!Read(scope.renderer+0xca40,world)||FAILED(device->GetTransform(D3DTS_WORLD,&boundWorld))||
     memcmp(&world,&boundWorld,sizeof(world)))return false;
  geometry={&v->second,&i->second,scope.mesh,scope.renderer,stride,scope.sequence,
    {primitive,INT(mesh.vertexBegin),0,mesh.base.base.vertexCount,mesh.indexBegin,mesh.base.base.primitiveCount},world};
  ++matched;return true;
}
static bool InstallSharedHook() {
  // Two complete position-independent instructions; no protected entry or
  // branch relocation. Caller already verified the fixed-base debug image.
  constexpr unsigned char expected[7]={0x6a,0xff,0x68,0x86,0x4e,0x6c,0};
  unsigned char observed[7]{};
  if(!scene_geometry::Read(abi::spDXSharedMeshDataInitialize,observed,7)||memcmp(observed,expected,7))return false;
  auto trampoline=static_cast<unsigned char*>(VirtualAlloc(nullptr,12,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
  if(!trampoline)return false;
  memcpy(trampoline,expected,7);trampoline[7]=0xe9;
  auto jump=uint32_t(abi::spDXSharedMeshDataInitialize+7-reinterpret_cast<uintptr_t>(trampoline+12));memcpy(trampoline+8,&jump,4);
  DWORD previous=0,ignored=0;
  if(!VirtualProtect(trampoline,12,PAGE_EXECUTE_READ,&previous)){VirtualFree(trampoline,0,MEM_RELEASE);return false;}
  auto entry=reinterpret_cast<void*>(abi::spDXSharedMeshDataInitialize);
  if(!VirtualProtect(entry,7,PAGE_EXECUTE_READWRITE,&previous)){VirtualFree(trampoline,0,MEM_RELEASE);return false;}
  originalSharedInitialize=reinterpret_cast<NativeSharedInitialize>(trampoline);
  unsigned char replacement[7]={0xe9,0,0,0,0,0x90,0x90};
  jump=uint32_t(reinterpret_cast<uintptr_t>(&SharedInitialize)-abi::spDXSharedMeshDataInitialize-5);memcpy(replacement+1,&jump,4);
  memcpy(entry,replacement,7);VirtualProtect(entry,7,previous,&ignored);
  FlushInstructionCache(GetCurrentProcess(),trampoline,12);FlushInstructionCache(GetCurrentProcess(),entry,7);return true;
}
static bool InstallMeshHook() {
  // sub esp,18; push ebx; mov ebx,[esp+24]. Three complete instructions.
  constexpr unsigned char expected[8]={0x83,0xec,0x18,0x53,0x8b,0x5c,0x24,0x24};
  unsigned char observed[8]{};const auto address=abi::spDXMeshInitializeInterface;
  if(!scene_geometry::Read(address,observed,8)||memcmp(observed,expected,8))return false;
  auto trampoline=static_cast<unsigned char*>(VirtualAlloc(nullptr,13,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
  if(!trampoline)return false;
  memcpy(trampoline,expected,8);trampoline[8]=0xe9;
  auto jump=uint32_t(address+8-reinterpret_cast<uintptr_t>(trampoline+13));memcpy(trampoline+9,&jump,4);
  DWORD previous=0,ignored=0;
  if(!VirtualProtect(trampoline,13,PAGE_EXECUTE_READ,&previous)){VirtualFree(trampoline,0,MEM_RELEASE);return false;}
  auto entry=reinterpret_cast<void*>(address);
  if(!VirtualProtect(entry,8,PAGE_EXECUTE_READWRITE,&previous)){VirtualFree(trampoline,0,MEM_RELEASE);return false;}
  originalMeshInitialize=reinterpret_cast<NativeMeshInitialize>(trampoline);
  unsigned char replacement[8]={0xe9,0,0,0,0,0x90,0x90,0x90};
  jump=uint32_t(reinterpret_cast<uintptr_t>(&MeshInitialize)-address-5);memcpy(replacement+1,&jump,4);
  memcpy(entry,replacement,8);VirtualProtect(entry,8,previous,&ignored);
  FlushInstructionCache(GetCurrentProcess(),trampoline,13);FlushInstructionCache(GetCurrentProcess(),entry,8);return true;
}
#else
static bool Resolve(IDirect3DDevice9*,const DrawRange&,void*,void*,UINT,UINT,Geometry&){return false;}
#endif
static void Initialize() {
  wchar_t path[MAX_PATH]{},option[8]{},mode[32]{};
  const auto length=GetEnvironmentVariableW(L"WINX_REMIX_NATIVE_MESH_SOURCE",path,MAX_PATH);
  if(!length||length>=MAX_PATH)return;
#if defined(_M_IX86)
  if(!scene_audit::VerifiedImage())return;
  output=_wfsopen(path,L"wb",_SH_DENYNO);ownerThread=GetCurrentThreadId();
  const auto slot=abi::spPCRendererInterfaceVTable+abi::spPCRendererSubmitMeshSlot*4;
  DWORD previous=0,ignored=0;
  if(scene_geometry::Word(slot)==abi::spPCRendererSubmitMesh&&InstallSharedHook()&&InstallMeshHook()&&
     VirtualProtect(reinterpret_cast<void*>(slot),4,PAGE_READWRITE,&previous)) {
    originalSubmit=reinterpret_cast<NativeSubmit>(abi::spPCRendererSubmitMesh);
    InterlockedExchangePointer(reinterpret_cast<void* volatile*>(slot),reinterpret_cast<void*>(&Submit));
    VirtualProtect(reinterpret_cast<void*>(slot),4,previous,&ignored);enabled=true;
  }
  GetEnvironmentVariableW(L"WINX_REMIX_BACKEND",mode,32);
  submitEnabled=enabled&&wcscmp(mode,L"system")!=0&&
    GetEnvironmentVariableW(L"WINX_REMIX_NATIVE_MESH_SUBMIT",option,8)&&wcscmp(option,L"1")==0;
  if(output){fprintf(output,"{\"event\":\"init\",\"schema\":1,\"enabled\":%s,\"submit\":%s,\"maxBytes\":67108864,\"maxBuffers\":4096,\"maxLogBytes\":16777216,\"sources\":[\"native_shared_mesh_initializer\",\"native_mesh_cpu_input\"]}\n",enabled?"true":"false",submitEnabled?"true":"false");fflush(output);}
#else
  (void)option;(void)mode;
#endif
}
static void EndFrame() {
  std::lock_guard<std::recursive_mutex> lock(guard);
  if(!output||_ftelli64(output)>=16*1024*1024)return;
  if(captures||invalidations||failures||limited||used||frameId%300==0) {
    fprintf(output,"{\"event\":\"frame\",\"frame\":%u,\"captures\":%u,\"invalidations\":%u,\"failures\":%u,\"limited\":%u,\"meshCalls\":%u,\"matched\":%u,\"used\":%u,\"uploadMismatches\":%u,\"buffers\":%zu,\"bytes\":%zu}\n",
      frameId,captures,invalidations,failures,limited,submissions,matched,used,uploadMismatches,buffers.size(),retainedBytes);fflush(output);
  }
  captures=invalidations=failures=limited=submissions=matched=used=uploadMismatches=0;
}
} // namespace native_mesh_source
