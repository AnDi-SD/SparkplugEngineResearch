#pragma once
// Own current-source boundary for Skin. No file output, sampling, COM/API calls,
// producer execution or D3D suppression. Native addresses in Stamp are identity
// tokens, never owning references. A stamp is valid only in its original scope.
#include "winx_skin_packet.h"
namespace native_skin_packet_source {
#if defined(_M_IX86)
namespace source=native_mesh_source;
namespace packets=winx_remix::skin_packet;
namespace abi=sparkplug::evidence::pc;
struct Stamp {
  bool valid=false;
  native_skin_source::Observation skin{};
  abi::spDXMeshObservedLayout mesh{};
  abi::spDXVertexBufferLayout vb{};abi::spDXIndexBufferLayout ib{};
  std::array<uint32_t,7> declaration{};
  source::TransportWitness transport{}; // elements is always null in published stamps.
  uint64_t generation=0,vertexHash=0,indexHash=0;
};
struct Packet {packets::Packet geometry;Stamp stamp;};
// Internal borrowed inputs must not cross a COM/API call or escape guard.
struct Borrowed {
  Stamp stamp;
  source::ResourcePair pair{};
  const source::VertexElements* layout=nullptr;
};
static bool Fail(unsigned* reason,unsigned value){if(reason)*reason=value;return false;}
static bool ScopeCurrent(const native_skin_source::Observation& skin) {
  const auto scope=native_skin_source::active;
  return scope&&skin.withinTolerance&&scope->sequence==skin.modelCall&&scope->skin==skin.skin&&
    scope->camera==skin.camera&&scope->support==skin.support&&native_skin_source::Current(*scope,skin.owner);
}
static bool Acquire(const std::unique_lock<std::recursive_mutex>& borrow,
    const native_skin_source::Observation& skin,Borrowed& out,unsigned* reason=nullptr) {
  if(reason)*reason=0;
  if(!borrow.owns_lock()||borrow.mutex()!=&guard||!source::enabled||
     GetCurrentThreadId()!=source::ownerThread||!ScopeCurrent(skin))return Fail(reason,1);
  Borrowed value;auto& stamp=value.stamp;stamp.skin=skin;auto& mesh=stamp.mesh;auto& pair=value.pair;
  uint32_t device=0;
  if(!source::Read(skin.mesh,mesh)||scene_geometry::Word(skin.mesh)!=abi::spDXMeshVTable||
     mesh.base.base.secondaryVTable!=abi::spDXMeshInterfaceVTable||
     (mesh.indexType!=2&&mesh.indexType!=3)||!mesh.componentWeightCount||mesh.componentWeightCount>4||
     !source::ReadResourceHeaders(mesh,pair)||!source::Read(skin.renderer+0xc9e8,device)||
     !source::Read(mesh.vertexDeclaration,stamp.declaration)||stamp.declaration[0]!=0x6f2e58||
     stamp.declaration[5]!=mesh.fvfCode)return Fail(reason,1);
  const auto vb=reinterpret_cast<void*>(pair.vb.direct3DVertexBuffer),ib=reinterpret_cast<void*>(pair.ib.direct3DIndexBuffer);
  auto& transport=stamp.transport;
  if(!native_transport_source::Witness(borrow,reinterpret_cast<IDirect3DDevice9*>(device),vb,ib,
     reinterpret_cast<void*>(stamp.declaration[6]),transport)||transport.indexFormat!=D3DFMT_INDEX16||
     transport.vertexBytes!=pair.vb.byteSize||transport.indexBytes!=pair.ib.byteSize||
     !source::ResolveResourceRanges(mesh,mesh.vertexStride,vb,ib,pair)||
     surfaceWrites.count(vb)||surfaceWrites.count(ib))return Fail(reason,2);
  const auto v=surfaceBuffers.find(vb),i=surfaceBuffers.find(ib);
  if(v==surfaceBuffers.end()||i==surfaceBuffers.end()||!v->second.complete||!i->second.complete||
     !source::EqualsUpload(*pair.vertices,v->second.bytes)||!source::EqualsUpload(*pair.indices,i->second.bytes))return Fail(reason,3);
  value.layout=source::Layout(mesh.base.base.vertexComponentFlags);
  if(!value.layout||value.layout->size()!=transport.elementCount||
     memcmp(value.layout->data(),transport.elements,value.layout->size()*sizeof(packets::Element)))return Fail(reason,4);
  stamp.vb=pair.vb;stamp.ib=pair.ib;stamp.generation=pair.vertices->generation;
  // No declaration-vector pointer survives this function, even inside Borrowed.
  transport.elements=nullptr;stamp.valid=true;out=std::move(value);return true;
}
static bool CurrentBorrowed(const std::unique_lock<std::recursive_mutex>& borrow,const Borrowed& input,bool palette) {
  if(!borrow.owns_lock()||borrow.mutex()!=&guard||!input.stamp.valid)return false;
  const auto& stamp=input.stamp;const auto& skin=stamp.skin;
  if(!ScopeCurrent(skin))return false;
  abi::spDXMeshObservedLayout mesh{};source::ResourcePair pair{};
  abi::spSkinObservedLayout nativeSkin{};abi::spRenderSupportObservedLayout support{};
  std::array<uint32_t,7> declaration{};uint32_t device=0;
  if(!source::Read(skin.mesh,mesh)||memcmp(&mesh,&stamp.mesh,sizeof(mesh))||
     !source::ReadResourceHeaders(mesh,pair)||memcmp(&pair.vb,&stamp.vb,sizeof(pair.vb))||
     memcmp(&pair.ib,&stamp.ib,sizeof(pair.ib))||!source::Read(mesh.vertexDeclaration,declaration)||
     declaration!=stamp.declaration||!source::Read(skin.renderer+0xc9e8,device)||
     device!=reinterpret_cast<uintptr_t>(stamp.transport.device)||
     !source::Read(skin.skin,nativeSkin)||memcmp(&nativeSkin,&skin.nativeSkin,sizeof(nativeSkin))||
     !source::Read(skin.support,support)||memcmp(&support,&skin.supportState,sizeof(support))||
     scene_geometry::Word(skin.owner.object)!=skin.ownerPrimary||
     scene_geometry::Word(native_owner_source::rendererPointerAddress)!=skin.renderer||
     scene_geometry::Word(skin.renderer)!=abi::spPCRendererPrimaryVTable||
     !native_transport_source::Current(borrow,stamp.transport))return false;
  if(palette){
    if(!skin.fixedPaletteAvailable||!skin.boneCount||skin.boneCount>skin.fixedPalette.size())return false;
    uint32_t header[2]{};
    if(!source::Read(skin.renderer+0xc9b8,header)||header[0]!=skin.palette||header[1]!=skin.boneCount)return false;
    for(unsigned b=0;b<skin.boneCount;++b){native_skin_source::math::Matrix4 current{};
      if(!source::Read(skin.palette+b*64,current)||memcmp(current.data(),skin.fixedPalette[b].data(),sizeof(current)))return false;}
  }
  return ScopeCurrent(skin);
}
static bool Build(const std::unique_lock<std::recursive_mutex>& borrow,const Borrowed& input,
    packets::Packet& output,packets::Error* error=nullptr) {
  const auto& skin=input.stamp.skin;const auto& mesh=input.stamp.mesh;
  if(!borrow.owns_lock()||borrow.mutex()!=&guard||!input.stamp.valid||!skin.fixedPaletteAvailable||
     !skin.boneCount||skin.boneCount>skin.fixedPalette.size())return packets::Fail(error,packets::Error::Palette);
  return packets::Build(input.pair.vertices->data,input.pair.indices->data,input.layout->data(),input.layout->size(),
    mesh.vertexStride,mesh.vertexBegin,mesh.base.base.vertexCount,mesh.indexBegin,mesh.base.base.primitiveCount,
    mesh.indexType,mesh.componentWeightCount,skin.fixedPalette.data(),skin.boneCount,output,error);
}
static void HashSource(const Borrowed& input,Stamp& output) {
  const auto& v=input.pair.vertices->data;const auto& i=input.pair.indices->data;
  output.vertexHash=XXH3_64bits(v.data(),v.size());output.indexHash=XXH3_64bits(i.data(),i.size());
}
static bool Copy(const native_skin_source::Observation& skin,Packet& output,unsigned* reason=nullptr) {
  try {
    std::unique_lock<std::recursive_mutex> borrow(guard);Borrowed input;
    if(!Acquire(borrow,skin,input,reason))return false;
    Packet result;packets::Error error=packets::Error::None;
    if(!Build(borrow,input,result.geometry,&error))return Fail(reason,100+unsigned(error));
    if(!CurrentBorrowed(borrow,input,true))return Fail(reason,6);
    result.stamp=input.stamp;HashSource(input,result.stamp);
    output=std::move(result);return true;
  }catch(const std::bad_alloc&){return Fail(reason,7);}
}
// Reacquire source/upload under guard. No old borrowed pointer is dereferenced.
// Hashes also reject changed bytes if an observed source generation was reused.
static bool Current(const Stamp& stamp) {
  if(!stamp.valid||stamp.transport.elements)return false;
  try {
    std::unique_lock<std::recursive_mutex> borrow(guard);Borrowed now;
    if(!Acquire(borrow,stamp.skin,now)||!CurrentBorrowed(borrow,now,true)||
       memcmp(&now.stamp.mesh,&stamp.mesh,sizeof(stamp.mesh))||
       memcmp(&now.stamp.vb,&stamp.vb,sizeof(stamp.vb))||memcmp(&now.stamp.ib,&stamp.ib,sizeof(stamp.ib))||
       now.stamp.declaration!=stamp.declaration||now.stamp.generation!=stamp.generation||
       now.stamp.transport.device!=stamp.transport.device||
       now.stamp.transport.vertexGeneration!=stamp.transport.vertexGeneration||
       now.stamp.transport.indexGeneration!=stamp.transport.indexGeneration||
       now.stamp.transport.declarationGeneration!=stamp.transport.declarationGeneration)return false;
    HashSource(now,now.stamp);
    return now.stamp.vertexHash==stamp.vertexHash&&now.stamp.indexHash==stamp.indexHash;
  }catch(const std::bad_alloc&){return false;}
}
static bool MatchesDraw(const Stamp& stamp,IDirect3DDevice9* device,const source::DrawRange& draw) {
  const auto mesh=source::active;const auto& native=stamp.mesh;
  return stamp.valid&&ScopeCurrent(stamp.skin)&&mesh&&mesh->valid&&!mesh->parent&&mesh->mesh==stamp.skin.mesh&&
    mesh->renderer==stamp.skin.renderer&&mesh->sequence==stamp.skin.submission&&
    device==stamp.transport.device&&!memcmp(&mesh->value,&native,sizeof(native))&&
    draw.type==(native.indexType==2?D3DPT_TRIANGLELIST:D3DPT_TRIANGLESTRIP)&&draw.base>=0&&
    uint32_t(draw.base)==native.vertexBegin&&!draw.minimum&&draw.vertices==native.base.base.vertexCount&&
    draw.start==native.indexBegin&&draw.count==native.base.base.primitiveCount;
}
static bool CopyDraw(IDirect3DDevice9* device,const source::DrawRange& draw,Packet& output,unsigned* reason=nullptr) {
  const auto scope=native_skin_source::active;
  if(!scope||!scope->observed)return Fail(reason,1);
  // Use a value copy even though Copy currently makes no external calls.
  const auto observation=scope->observation;
  Packet result;if(!Copy(observation,result,reason))return false;
  if(!MatchesDraw(result.stamp,device,draw))return Fail(reason,8);
  output=std::move(result);return true;
}
#endif
} // namespace native_skin_packet_source
