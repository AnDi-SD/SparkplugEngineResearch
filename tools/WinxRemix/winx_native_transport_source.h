// Own bounded D3D creation/lifetime records. They prove transport association
// from successful Create calls, not from unresolved native buffer fields.
// No extra COM references are retained and no borrowed raw pointer is called.
#pragma once
#include <array>
namespace native_transport_source {
static bool enabled;
static FILE* output;
enum class Kind:unsigned { Vertex,Index,Declaration };
struct Record {
  IDirect3DDevice9* device=nullptr;
  Kind kind=Kind::Vertex;
  uint64_t generation=0;
  UINT bytes=0;
  D3DFORMAT format=D3DFMT_UNKNOWN;
  std::vector<D3DVERTEXELEMENT9> elements;
};
static std::map<void*,Record> records;
static std::map<IDirect3DDevice9*,unsigned> resetting;
static uint64_t nextGeneration;
static unsigned created[3]{},retired[3]{},resets,rejected,reused,queries,accepted;
constexpr size_t recordLimit=8192,declarationLimit=512;
static unsigned declarationCount;
// Separate bounded registry: ordinary managed textures only. All access is
// under guard. Identity is the temporary owned QueryInterface(IUnknown) result
// observed at Create; no COM reference is retained by either registry.
struct TextureWitness {
  bool valid=false;IDirect3DDevice9* device=nullptr;IDirect3DTexture9* texture=nullptr;
  uint64_t generation=0,contentGeneration=0;
  UINT width=0,height=0,levels=0;D3DFORMAT format=D3DFMT_UNKNOWN;
};
struct TextureAccess {unsigned char lock=0;HDC dc=nullptr;}; //0 closed,1 readonly,2 writable
struct TextureRecord {
  TextureWitness value{};void* identity=nullptr;
  std::vector<D3DSURFACE_DESC> layout;
  std::array<TextureAccess,32> access{};
};
struct TextureSurface {IDirect3DTexture9* texture=nullptr;uint64_t generation=0;void* identity=nullptr;UINT level=0;};
static std::map<IDirect3DTexture9*,TextureRecord> textures;
static std::map<IDirect3DSurface9*,TextureSurface> textureSurfaces;
static std::set<IDirect3DDevice9*> textureBlockedDevices;
static bool textureCoverage=true;
static uint64_t nextTextureContent;
constexpr size_t textureLimit=4096,textureSurfaceLimit=8192;
static unsigned textureCreates,textureRetires,textureWrites,textureRejects,textureQueries,textureAccepted;
static void ForgetTexture(IDirect3DTexture9* texture) {
  const auto found=textures.find(texture);if(found==textures.end())return;
  for(auto it=textureSurfaces.begin();it!=textureSurfaces.end();) {
    if(it->second.texture==texture)it=textureSurfaces.erase(it);else ++it;
  }
  textures.erase(found);++textureRetires;
}
static void RetireDeviceTextures(IDirect3DDevice9* device) {
  for(auto it=textures.begin();it!=textures.end();) {
    if(it->second.value.device==device){const auto texture=it->first;++it;ForgetTexture(texture);}else ++it;
  }
}
static void BlockTextureDevice(IDirect3DDevice9* device) {
  // An escaped unhooked interface may outlive Reset and later write another
  // texture. Keep this conservative device block through Reset/address reuse.
  RetireDeviceTextures(device);++textureRejects;
  try {
    if(textureBlockedDevices.size()>=64&&!textureBlockedDevices.count(device))throw std::bad_alloc();
    textureBlockedDevices.insert(device);
  }catch(const std::bad_alloc&){textureCoverage=false;textures.clear();textureSurfaces.clear();}
}
static IDirect3DTexture9* TextureByIdentity(void* identity) {
  if(!identity)return nullptr;
  for(const auto& entry:textures)if(entry.second.identity==identity)return entry.first;
  return nullptr;
}
static IDirect3DTexture9* SurfaceTexture(IDirect3DSurface9* surface) {
  const auto entry=textureSurfaces.find(surface);if(entry==textureSurfaces.end())return nullptr;
  const auto texture=textures.find(entry->second.texture);
  return texture!=textures.end()&&texture->second.value.generation==entry->second.generation?texture->first:nullptr;
}
static bool RememberTexture(IDirect3DDevice9* device,IDirect3DTexture9* texture,void* identity,
                            const D3DSURFACE_DESC* layout,UINT count) {
  if(!enabled)return false;ForgetTexture(texture);
  if(!textureCoverage||!device||!texture||!identity||textureBlockedDevices.count(device)||resetting.count(device)||!layout||!count||count>32||textures.size()>=textureLimit){++textureRejects;return false;}
  const auto& first=layout[0];UINT width=first.Width,height=first.Height;
  if(!width||!height||(first.Format!=D3DFMT_A8R8G8B8&&first.Format!=D3DFMT_X8R8G8B8)){++textureRejects;return false;}
  for(UINT level=0;level<count;++level) {
    const auto& desc=layout[level];
    if(desc.Pool!=D3DPOOL_MANAGED||desc.Usage||desc.Type!=D3DRTYPE_SURFACE||desc.Format!=first.Format||
       desc.MultiSampleType!=D3DMULTISAMPLE_NONE||desc.MultiSampleQuality||desc.Width!=width||desc.Height!=height||
       (level&&layout[level-1].Width==1&&layout[level-1].Height==1)){++textureRejects;return false;}
    width=(std::max)(1u,width/2);height=(std::max)(1u,height/2);
  }
  TextureRecord record;record.identity=identity;
  record.value={true,device,texture,++nextGeneration,++nextTextureContent,first.Width,first.Height,count,first.Format};
  try {record.layout.assign(layout,layout+count);textures.emplace(texture,std::move(record));}
  catch(const std::bad_alloc&){++textureRejects;return false;}
  ++textureCreates;return true;
}
static void ForgetTextureSurface(IDirect3DSurface9* surface) {
  const auto entry=textureSurfaces.find(surface);if(entry==textureSurfaces.end())return;
  const auto texture=textures.find(entry->second.texture);const UINT level=entry->second.level;
  if(texture!=textures.end()&&(texture->second.access[level].lock||texture->second.access[level].dc)) {
    ForgetTexture(texture->first);return; // Lost access owner cannot silently unblock.
  }
  textureSurfaces.erase(entry);
}
static bool RememberSurface(IDirect3DTexture9* texture,IDirect3DSurface9* surface,void* identity,UINT level) {
  const auto parent=textures.find(texture);
  if(!enabled||parent==textures.end()||!surface||!identity||level>=parent->second.value.levels)return false;
  const auto previous=textureSurfaces.find(surface);
  if(previous!=textureSurfaces.end()&&previous->second.texture==texture&&previous->second.generation==parent->second.value.generation&&
     previous->second.level==level&&previous->second.identity==identity)return true;
  if(previous!=textureSurfaces.end()){ForgetTextureSurface(surface);if(!textures.count(texture))return false;}
  if(textureSurfaces.size()>=textureSurfaceLimit){ForgetTexture(texture);++textureRejects;return false;}
  try {textureSurfaces.emplace(surface,TextureSurface{texture,parent->second.value.generation,identity,level});}
  catch(const std::bad_alloc&){ForgetTexture(texture);++textureRejects;return false;}
  return true;
}
static void TextureLockResult(IDirect3DTexture9* texture,UINT level,DWORD flags,HRESULT result) {
  const auto found=textures.find(texture);if(found==textures.end()||FAILED(result))return;
  if(level>=found->second.value.levels){ForgetTexture(texture);++textureRejects;return;}
  auto& record=found->second;const auto mode=(flags&D3DLOCK_READONLY)?1u:2u;
  // Surface and texture API aliases describe the same one-lock-per-mip access.
  // Internal forwarding may be observed twice; a close through either alias
  // closes that same storage. NO_DIRTY_UPDATE remains a writable lock.
  record.access[level].lock=static_cast<unsigned char>((std::max)(unsigned(record.access[level].lock),mode));
  if(mode==2){record.value.contentGeneration=++nextTextureContent;++textureWrites;}
}
static void TextureUnlockResult(IDirect3DTexture9* texture,UINT level,HRESULT result) {
  const auto found=textures.find(texture);if(found==textures.end()||FAILED(result))return;
  if(level>=found->second.value.levels){ForgetTexture(texture);++textureRejects;return;}
  found->second.access[level].lock=0;
}
static void SurfaceLockResult(IDirect3DSurface9* surface,DWORD flags,HRESULT result) {
  const auto entry=textureSurfaces.find(surface);if(entry!=textureSurfaces.end())TextureLockResult(entry->second.texture,entry->second.level,flags,result);
}
static void SurfaceUnlockResult(IDirect3DSurface9* surface,HRESULT result) {
  const auto entry=textureSurfaces.find(surface);if(entry!=textureSurfaces.end())TextureUnlockResult(entry->second.texture,entry->second.level,result);
}
static void SurfaceDCResult(IDirect3DSurface9* surface,HDC dc,HRESULT result) {
  const auto entry=textureSurfaces.find(surface);if(entry==textureSurfaces.end()||FAILED(result))return;
  const auto parent=textures.find(entry->second.texture);if(parent==textures.end())return;
  if(!dc){ForgetTexture(parent->first);++textureRejects;return;}
  parent->second.access[entry->second.level].dc=dc;parent->second.value.contentGeneration=++nextTextureContent;++textureWrites;
}
static void SurfaceReleaseDCResult(IDirect3DSurface9* surface,HDC dc,HRESULT result) {
  const auto entry=textureSurfaces.find(surface);if(entry==textureSurfaces.end()||FAILED(result))return;
  const auto parent=textures.find(entry->second.texture);if(parent==textures.end())return;
  auto& current=parent->second.access[entry->second.level].dc;
  if(current!=dc){ForgetTexture(parent->first);++textureRejects;return;}current=nullptr;
}
static bool BorrowTexture(const std::unique_lock<std::recursive_mutex>& borrow,IDirect3DDevice9* device,
                          IDirect3DTexture9* texture,TextureWitness& out) {
  out={};if(!enabled||!textureCoverage||!borrow.owns_lock()||borrow.mutex()!=&guard||textureBlockedDevices.count(device)||resetting.count(device))return false;++textureQueries;
  const auto found=textures.find(texture);if(found==textures.end()||found->second.value.device!=device)return false;
  for(UINT level=0;level<found->second.value.levels;++level)
    if(found->second.access[level].lock==2||found->second.access[level].dc)return false;
  out=found->second.value;++textureAccepted;return true;
}
static bool CurrentTexture(const std::unique_lock<std::recursive_mutex>& borrow,const TextureWitness& input) {
  TextureWitness now{};return input.valid&&BorrowTexture(borrow,input.device,input.texture,now)&&
    now.generation==input.generation&&now.contentGeneration==input.contentGeneration;
}
static bool CanLog(){return output&&_ftelli64(output)<16*1024*1024;}
// Caller holds guard from before the corresponding native API operation.
static void Forget(void* pointer) {
  const auto found=records.find(pointer);if(found==records.end())return;
  ++retired[static_cast<unsigned>(found->second.kind)];
  if(found->second.kind==Kind::Declaration)--declarationCount;
  records.erase(found);
}
static bool Remember(IDirect3DDevice9* device,void* pointer,Kind kind,UINT bytes,D3DFORMAT format,
                     const D3DVERTEXELEMENT9* elements=nullptr,UINT count=0) {
  if(!enabled)return false;
  if(records.count(pointer)){++reused;Forget(pointer);}
  if(!device||!pointer||resetting.count(device)||records.size()>=recordLimit||
     (kind==Kind::Declaration&&(!elements||!count||count>MAXD3DDECLLENGTH+1||declarationCount>=declarationLimit))||
     (kind!=Kind::Declaration&&!bytes)){++rejected;return false;}
  Record record;record.device=device;record.kind=kind;record.generation=++nextGeneration;
  record.bytes=bytes;record.format=format;
  try {
    if(kind==Kind::Declaration)record.elements.assign(elements,elements+count);
    records.emplace(pointer,std::move(record));
  } catch(const std::bad_alloc&){++rejected;return false;}
  if(kind==Kind::Declaration)++declarationCount;
  ++created[static_cast<unsigned>(kind)];return true;
}
static void RetireDevice(IDirect3DDevice9* device) {
  if(!enabled)return;
  for(auto it=records.begin();it!=records.end();) {
    if(it->second.device==device){const auto pointer=it->first;++it;Forget(pointer);}else ++it;
  }
  RetireDeviceTextures(device);
}
static void BeginReset(IDirect3DDevice9* device) {
  if(!enabled)return;++resets;RetireDevice(device);
  try {++resetting[device];}catch(const std::bad_alloc&){enabled=false;records.clear();declarationCount=0;textures.clear();textureSurfaces.clear();}
}
static void EndReset(IDirect3DDevice9* device){const auto it=resetting.find(device);if(it!=resetting.end()&&!--it->second)resetting.erase(it);}
#if defined(_M_IX86)
static bool Witness(const std::unique_lock<std::recursive_mutex>& borrow,IDirect3DDevice9* device,
                    void* vertex,void* index,void* declaration,native_mesh_source::TransportWitness& out) {
  out={};if(!enabled||!borrow.owns_lock()||borrow.mutex()!=&guard||resetting.count(device))return false;++queries;
  const auto v=records.find(vertex),i=records.find(index),d=records.find(declaration);
  if(v==records.end()||i==records.end()||d==records.end()||v->second.kind!=Kind::Vertex||
     i->second.kind!=Kind::Index||d->second.kind!=Kind::Declaration||v->second.device!=device||
     i->second.device!=device||d->second.device!=device)return false;
  out.valid=true;out.device=device;out.vertexBuffer=vertex;out.indexBuffer=index;out.declaration=declaration;
  out.vertexGeneration=v->second.generation;out.indexGeneration=i->second.generation;out.declarationGeneration=d->second.generation;
  out.vertexBytes=v->second.bytes;out.indexBytes=i->second.bytes;out.indexFormat=i->second.format;
  out.elements=d->second.elements.data();out.elementCount=static_cast<UINT>(d->second.elements.size());++accepted;return true;
}
static bool Current(const std::unique_lock<std::recursive_mutex>& borrow,const native_mesh_source::TransportWitness& input) {
  native_mesh_source::TransportWitness now{};
  return input.valid&&Witness(borrow,input.device,input.vertexBuffer,input.indexBuffer,input.declaration,now)&&
    input.vertexGeneration==now.vertexGeneration&&input.indexGeneration==now.indexGeneration&&
    input.declarationGeneration==now.declarationGeneration;
}
#endif
static void Initialize() {
  wchar_t path[MAX_PATH]{};const auto length=GetEnvironmentVariableW(L"WINX_REMIX_NATIVE_TRANSPORT_SOURCE",path,MAX_PATH);
  if(!length||length>=MAX_PATH||!native_mesh_source::enabled)return;
  output=_wfsopen(path,L"wb",_SH_DENYNO);enabled=true;
  if(CanLog()){fprintf(output,"{\"event\":\"init\",\"schema\":1,\"enabled\":true,\"maxRecords\":8192,\"maxDeclarations\":512,\"maxLogBytes\":16777216,\"scope\":\"successful Create to observed final Release/reset; no retained COM references\"}\n");fflush(output);}
}
static void EndFrame() {
  std::lock_guard<std::recursive_mutex> lock(guard);
  if(CanLog()&&(created[0]||created[1]||created[2]||retired[0]||retired[1]||retired[2]||queries||resets||rejected||frameId%300==0)) {
    fprintf(output,"{\"event\":\"frame\",\"frame\":%u,\"records\":%zu,\"declarations\":%u,\"created\":[%u,%u,%u],\"retired\":[%u,%u,%u],\"resets\":%u,\"rejected\":%u,\"reused\":%u,\"queries\":%u,\"accepted\":%u}\n",
      frameId,records.size(),declarationCount,created[0],created[1],created[2],retired[0],retired[1],retired[2],resets,rejected,reused,queries,accepted);fflush(output);
  }
  memset(created,0,sizeof(created));memset(retired,0,sizeof(retired));resets=rejected=reused=queries=accepted=0;
  if(CanLog()&&(textureCreates||textureRetires||textureWrites||textureRejects||textureQueries||frameId%300==0)) {
    fprintf(output,"{\"event\":\"texture_frame\",\"frame\":%u,\"textures\":%zu,\"surfaces\":%zu,\"creates\":%u,\"retired\":%u,\"writes\":%u,\"rejected\":%u,\"queries\":%u,\"accepted\":%u}\n",
      frameId,textures.size(),textureSurfaces.size(),textureCreates,textureRetires,textureWrites,textureRejects,textureQueries,textureAccepted);fflush(output);
  }
  textureCreates=textureRetires=textureWrites=textureRejects=textureQueries=textureAccepted=0;
}
}
