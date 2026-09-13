// Own bounded CPU fixture. Registry identities are owned literal bytes only;
// successful Create/final Release observations are supplied directly to the
// registry. No game, real COM/device method, window, bridge or GPU executes.
#include <cstddef>
#include <cstdlib>
#include <new>
static thread_local int transportAllocationsUntilFailure=-1;
void* operator new(std::size_t size) {
  if(transportAllocationsUntilFailure==0){transportAllocationsUntilFailure=-1;throw std::bad_alloc();}
  if(transportAllocationsUntilFailure>0)--transportAllocationsUntilFailure;
  if(void* result=std::malloc(size?size:1))return result;throw std::bad_alloc();
}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete(void* pointer) noexcept{std::free(pointer);}
void operator delete[](void* pointer) noexcept{std::free(pointer);}
void operator delete(void* pointer,std::size_t) noexcept{std::free(pointer);}
void operator delete[](void* pointer,std::size_t) noexcept{std::free(pointer);}
#define WINX_REMIX_TEST
#include "winx_d3d9_probe.cpp"
#include <stdexcept>

namespace transport_test {
namespace source=native_transport_source;
static unsigned checks;
static void Check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
struct AllocationFault {
  explicit AllocationFault(int after){transportAllocationsUntilFailure=after;}
  ~AllocationFault(){transportAllocationsUntilFailure=-1;}
};
struct Watchdog {
  HANDLE event=nullptr,thread=nullptr;
  static DWORD WINAPI Wait(void* event){if(WaitForSingleObject(event,30000)==WAIT_TIMEOUT)TerminateProcess(GetCurrentProcess(),0xe0520c30u);return 0;}
  Watchdog(){event=CreateEventW(nullptr,TRUE,FALSE,nullptr);Check(event!=nullptr,"watchdog event");thread=CreateThread(nullptr,0,Wait,event,0,nullptr);Check(thread!=nullptr,"watchdog thread");}
  ~Watchdog(){SetEvent(event);WaitForSingleObject(thread,1000);CloseHandle(thread);CloseHandle(event);}
};
struct Fixture {
  uint32_t identities[8]{0xdead0001,0xdead0002,0xdead0003,0xdead0004,0xdead0005,0xdead0006,0xdead0007,0xdead0008};
  D3DVERTEXELEMENT9 elements[2]={{0,0,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_POSITION,0},{0xff,0,D3DDECLTYPE_UNUSED,0,0,0}};
  IDirect3DDevice9* Device(unsigned which=0){return reinterpret_cast<IDirect3DDevice9*>(&identities[which]);}
  void* Vertex(){return &identities[2];}void* Index(){return &identities[3];}void* Declaration(){return &identities[4];}
  void Fill(){
    Check(source::Remember(Device(),Vertex(),source::Kind::Vertex,96,D3DFMT_VERTEXDATA),"remember successful observed VB Create");
    Check(source::Remember(Device(),Index(),source::Kind::Index,12,D3DFMT_INDEX16),"remember successful observed IB Create");
    Check(source::Remember(Device(),Declaration(),source::Kind::Declaration,0,D3DFMT_UNKNOWN,elements,2),"remember successful observed declaration Create");
  }
};
static void Basic(std::unique_lock<std::recursive_mutex>& borrow,Fixture& f){
  native_mesh_source::TransportWitness witness{};
  auto query=[&](){return source::Witness(borrow,f.Device(),f.Vertex(),f.Index(),f.Declaration(),witness);};
  Check(!query()&&!witness.valid,"missing Create observations cannot grant witness");f.Fill();
  Check(query()&&witness.valid,"same-device typed observations grant witness");
  Check(witness.device==f.Device()&&witness.vertexBuffer==f.Vertex()&&witness.indexBuffer==f.Index()&&witness.declaration==f.Declaration()&&
    witness.vertexBytes==96&&witness.indexBytes==12&&witness.indexFormat==D3DFMT_INDEX16,"witness retains exact identities and sizes");
  Check(witness.vertexGeneration&&witness.indexGeneration&&witness.declarationGeneration&&
    witness.vertexGeneration!=witness.indexGeneration&&witness.indexGeneration!=witness.declarationGeneration,"independent Create observations have nonzero generations");
  Check(witness.elementCount==2&&!memcmp(witness.elements,f.elements,sizeof(f.elements))&&witness.elements!=f.elements,"registry owns copied actual declaration elements");
  ++f.elements[0].Offset;Check(witness.elements[0].Offset==0,"later caller array mutation cannot alter captured declaration");--f.elements[0].Offset;
  Check(source::Current(borrow,witness),"fresh witness current");
  auto stale=witness;const auto oldGeneration=witness.declarationGeneration;
  Check(source::Remember(f.Device(),f.Declaration(),source::Kind::Declaration,0,D3DFMT_UNKNOWN,f.elements,2),"repeat/interned declaration Create observed");
  Check(!source::Current(borrow,stale)&&query()&&witness.declarationGeneration>oldGeneration&&source::declarationCount==1,"interned address gives new observation generation without duplicate count");
  stale=witness;source::Forget(f.Vertex());Check(!source::Current(borrow,stale)&&!query()&&!witness.valid&&!witness.elements,"final Release invalidates witness and clears failed output");
  Check(source::Remember(f.Device(),f.Vertex(),source::Kind::Vertex,128,D3DFMT_VERTEXDATA)&&query()&&witness.vertexBytes==128&&
    witness.vertexGeneration>stale.vertexGeneration&&!source::Current(borrow,stale),"address reuse never revives old creation generation");
  auto good=witness;
  Check(!source::Witness(borrow,f.Device(1),f.Vertex(),f.Index(),f.Declaration(),witness)&&!witness.valid,"wrong queried device rejects");
  Check(!source::Witness(borrow,f.Device(),f.Index(),f.Vertex(),f.Declaration(),witness),"wrong resource kinds reject");
  source::Forget(f.Index());Check(source::Remember(f.Device(1),f.Index(),source::Kind::Index,12,D3DFMT_INDEX16),"observe other-device index buffer");
  Check(!query(),"one foreign-device record rejects the triple");source::Forget(f.Index());
  Check(source::Remember(f.Device(),f.Index(),source::Kind::Index,12,D3DFMT_INDEX16)&&query(),"same-device triple restored");
  borrow.unlock();Check(!query()&&!witness.valid&&!source::Current(borrow,good),"unlocked borrow rejects both query and fence");borrow.lock();
  std::recursive_mutex other;std::unique_lock<std::recursive_mutex> wrong(other);
  Check(!source::Witness(wrong,f.Device(),f.Vertex(),f.Index(),f.Declaration(),witness),"different mutex cannot grant witness");
  source::enabled=false;Check(!query()&&!witness.valid,"disabled registry rejects");source::enabled=true;
  const auto before=source::records.size();source::Forget(&f.identities[7]);Check(source::records.size()==before,"unknown final Release is harmless");
}
static void Reset(std::unique_lock<std::recursive_mutex>& borrow,Fixture& f){
  native_mesh_source::TransportWitness witness{};
  Check(source::Witness(borrow,f.Device(),f.Vertex(),f.Index(),f.Declaration(),witness),"witness before reset");
  Check(source::Remember(f.Device(1),&f.identities[5],source::Kind::Vertex,64,D3DFMT_VERTEXDATA),"other-device live resource before reset");
  source::BeginReset(f.Device());
  Check(source::records.size()==1&&!source::declarationCount&&source::records.count(&f.identities[5]),"reset immediately retires only target device records");
  Check(!source::Current(borrow,witness)&&!source::Witness(borrow,f.Device(),f.Vertex(),f.Index(),f.Declaration(),witness),"reset invalidates old witness");
  Check(!source::Remember(f.Device(),f.Vertex(),source::Kind::Vertex,96,D3DFMT_VERTEXDATA),"Create observation while Reset in flight cannot qualify");
  // Reset's original is deliberately outside guard. A second caller can reach
  // BeginReset; one End must not unblock an independently outstanding Reset.
  source::BeginReset(f.Device());source::EndReset(f.Device());
  Check(!source::Remember(f.Device(),f.Vertex(),source::Kind::Vertex,96,D3DFMT_VERTEXDATA),"overlapping reset remains blocked until both operations end");
  source::EndReset(f.Device());
  Check(!source::Witness(borrow,f.Device(),f.Vertex(),f.Index(),f.Declaration(),witness),"EndReset does not resurrect retired resources, independent of HRESULT");
  f.Fill();Check(source::Witness(borrow,f.Device(),f.Vertex(),f.Index(),f.Declaration(),witness),"new successful Creates restore qualification after reset");
  source::RetireDevice(f.Device());Check(source::records.size()==1&&!source::declarationCount&&!source::Current(borrow,witness),"final device Release retires dependent records");
  source::RetireDevice(f.Device(1));source::EndReset(f.Device(1));Check(source::records.empty(),"other device independently retired");
}
static void Bounds(Fixture& f){
  Check(!source::Remember(nullptr,f.Vertex(),source::Kind::Vertex,96,D3DFMT_VERTEXDATA),"null device rejects");
  Check(!source::Remember(f.Device(),nullptr,source::Kind::Vertex,96,D3DFMT_VERTEXDATA),"null result rejects");
  Check(!source::Remember(f.Device(),f.Vertex(),source::Kind::Vertex,0,D3DFMT_VERTEXDATA),"zero-sized buffer rejects");
  Check(!source::Remember(f.Device(),f.Declaration(),source::Kind::Declaration,0,D3DFMT_UNKNOWN,nullptr,2),"null declaration array rejects");
  Check(!source::Remember(f.Device(),f.Declaration(),source::Kind::Declaration,0,D3DFMT_UNKNOWN,f.elements,0),"empty declaration rejects");
  Check(!source::Remember(f.Device(),f.Declaration(),source::Kind::Declaration,0,D3DFMT_UNKNOWN,f.elements,MAXD3DDECLLENGTH+2),"declaration count bound checked before reading array");
  std::vector<uint32_t> identities(source::recordLimit+1);
  bool filled=true;for(size_t n=0;n<source::recordLimit;++n)filled=source::Remember(f.Device(),&identities[n],source::Kind::Vertex,96,D3DFMT_VERTEXDATA)&&filled;
  Check(filled&&source::records.size()==source::recordLimit,"record capacity can be reached exactly");
  Check(!source::Remember(f.Device(),&identities.back(),source::Kind::Vertex,96,D3DFMT_VERTEXDATA)&&source::records.size()==source::recordLimit,"new record beyond capacity fails closed");
  const auto generation=source::records.at(&identities[0]).generation;
  Check(source::Remember(f.Device(),&identities[0],source::Kind::Vertex,128,D3DFMT_VERTEXDATA)&&source::records.at(&identities[0]).generation>generation&&
    source::records.size()==source::recordLimit,"replacement at capacity succeeds with new generation and no growth");source::RetireDevice(f.Device());
  filled=true;for(size_t n=0;n<source::declarationLimit;++n)filled=source::Remember(f.Device(),&identities[n],source::Kind::Declaration,0,D3DFMT_UNKNOWN,f.elements,2)&&filled;
  Check(filled&&source::declarationCount==source::declarationLimit,"declaration capacity reached exactly");
  Check(!source::Remember(f.Device(),&identities.back(),source::Kind::Declaration,0,D3DFMT_UNKNOWN,f.elements,2),"declarations have separate bound");
  Check(source::Remember(f.Device(),&identities[0],source::Kind::Declaration,0,D3DFMT_UNKNOWN,f.elements,2)&&source::declarationCount==source::declarationLimit,"interned declaration replacement at cap does not falsely exhaust it");
  source::RetireDevice(f.Device());Check(source::records.empty()&&!source::declarationCount,"retirement restores capacity counters");
}
static void Allocation(Fixture& f){
  bool result=true;auto rejected=source::rejected;
  {AllocationFault fail(0);result=source::Remember(f.Device(),f.Vertex(),source::Kind::Vertex,96,D3DFMT_VERTEXDATA);}
  Check(!result&&source::records.empty()&&source::rejected==rejected+1,"failed buffer map allocation leaves no live token");
  rejected=source::rejected;
  {AllocationFault fail(0);result=source::Remember(f.Device(),f.Declaration(),source::Kind::Declaration,0,D3DFMT_UNKNOWN,f.elements,2);}
  Check(!result&&source::records.empty()&&!source::declarationCount&&source::rejected==rejected+1,"failed declaration copy leaves no record or count");
  rejected=source::rejected;
  {AllocationFault fail(1);result=source::Remember(f.Device(),f.Declaration(),source::Kind::Declaration,0,D3DFMT_UNKNOWN,f.elements,2);}
  Check(!result&&source::records.empty()&&!source::declarationCount&&source::rejected==rejected+1,"failed map insertion after copied declaration leaves no record or count");
  Check(source::Remember(f.Device(),f.Vertex(),source::Kind::Vertex,96,D3DFMT_VERTEXDATA),"prior record for failed replacement");
  {AllocationFault fail(0);result=source::Remember(f.Device(),f.Vertex(),source::Kind::Vertex,128,D3DFMT_VERTEXDATA);}
  Check(!result&&!source::records.count(f.Vertex()),"failed replacement cannot preserve stale prior generation");
  f.Fill();
  Check(source::Remember(f.Device(1),&f.identities[6],source::Kind::Vertex,64,D3DFMT_VERTEXDATA),"other device record before Reset allocation failure");
  {AllocationFault fail(0);source::BeginReset(f.Device());}
  Check(!source::enabled&&source::records.empty()&&!source::declarationCount,"Reset bookkeeping allocation failure disables registry and retires all borrowed records");
  source::EndReset(f.Device());source::enabled=true;
}
struct TextureFixture {
  // These addresses are distinct owned storage, never callable COM objects.
  uint32_t identities[12]{0xcafe0000,0xcafe0001,0xcafe0002,0xcafe0003,0xcafe0004,0xcafe0005,
    0xcafe0006,0xcafe0007,0xcafe0008,0xcafe0009,0xcafe000a,0xcafe000b};
  D3DSURFACE_DESC levels[3]{};
  TextureFixture(){for(UINT level=0;level<3;++level){auto& desc=levels[level];
    desc.Format=D3DFMT_X8R8G8B8;desc.Type=D3DRTYPE_SURFACE;desc.Pool=D3DPOOL_MANAGED;
    desc.Width=4u>>level;desc.Height=level?1u:2u;}}
  IDirect3DTexture9* Texture(unsigned which=0){return reinterpret_cast<IDirect3DTexture9*>(&identities[which]);}
  void* Identity(unsigned which=0){return &identities[2+which];}
  IDirect3DSurface9* Surface(unsigned which=0){return reinterpret_cast<IDirect3DSurface9*>(&identities[4+which]);}
  void* SurfaceIdentity(unsigned which=0){return &identities[6+which];}
  HDC DC(unsigned which=0){return reinterpret_cast<HDC>(&identities[8+which]);}
  bool Remember(IDirect3DDevice9* device,unsigned which=0){return source::RememberTexture(device,Texture(which),Identity(which),levels,3);}
};
static void TextureBasic(std::unique_lock<std::recursive_mutex>& borrow,Fixture& f){
  TextureFixture t;source::TextureWitness witness{};
  auto query=[&](){return source::BorrowTexture(borrow,f.Device(),t.Texture(),witness);};
  witness.valid=true;witness.texture=t.Texture();witness.generation=99;
  Check(!query()&&!witness.valid&&!witness.texture&&!witness.generation,"missing texture Create clears borrowed output");
  Check(t.Remember(f.Device())&&query()&&witness.valid,"observed managed texture Create grants witness");
  Check(witness.device==f.Device()&&witness.texture==t.Texture()&&witness.generation&&witness.contentGeneration&&
    witness.width==4&&witness.height==2&&witness.levels==3&&witness.format==D3DFMT_X8R8G8B8,"texture witness retains actual descriptor and two generations");
  const auto original=witness;t.levels[0].Width=8;
  Check(query()&&witness.width==4&&source::CurrentTexture(borrow,original),"registry copies texture descriptor input");t.levels[0].Width=4;
  Check(!source::BorrowTexture(borrow,f.Device(1),t.Texture(),witness)&&!witness.valid,"foreign device cannot borrow texture");
  Check(query(),"same-device texture remains available after rejected query");
  borrow.unlock();Check(!query()&&!witness.valid&&!source::CurrentTexture(borrow,original),"unlocked texture borrow and final fence reject");borrow.lock();
  std::recursive_mutex other;std::unique_lock<std::recursive_mutex> wrong(other);
  Check(!source::BorrowTexture(wrong,f.Device(),t.Texture(),witness)&&!source::CurrentTexture(wrong,original),"different mutex cannot qualify texture");
  source::enabled=false;Check(!query()&&!witness.valid&&!source::CurrentTexture(borrow,original),"disabled registry cannot qualify texture");source::enabled=true;
  source::ForgetTexture(t.Texture(1));source::ForgetTextureSurface(t.Surface(1));
  Check(source::CurrentTexture(borrow,original),"unknown texture and surface Release leave live witness intact");
  for(auto& desc:t.levels)desc.Format=D3DFMT_A8R8G8B8;
  Check(t.Remember(f.Device())&&query()&&witness.format==D3DFMT_A8R8G8B8&&witness.generation>original.generation&&
    !source::CurrentTexture(borrow,original),"same texture address replacement changes creation generation and accepts alpha format");
  const auto replacement=witness;source::ForgetTexture(t.Texture());
  Check(!source::CurrentTexture(borrow,replacement)&&!query()&&!witness.valid,"texture final Release retires its witness");
  Check(t.Remember(f.Device())&&query()&&witness.generation>replacement.generation&&!source::CurrentTexture(borrow,replacement),"texture address reuse cannot revive a released generation");
  source::RetireDevice(f.Device());
}
static void TextureWrites(std::unique_lock<std::recursive_mutex>& borrow,Fixture& f){
  TextureFixture t;source::TextureWitness witness{};
  auto query=[&](){return source::BorrowTexture(borrow,f.Device(),t.Texture(),witness);};
  Check(t.Remember(f.Device())&&query(),"texture access fixture created");auto prior=witness;
  source::TextureLockResult(t.Texture(),0,0,D3DERR_INVALIDCALL);
  Check(source::CurrentTexture(borrow,prior)&&query()&&witness.contentGeneration==prior.contentGeneration,"failed texture Lock does not change content");
  source::TextureLockResult(t.Texture(),1,D3DLOCK_READONLY,D3D_OK);source::TextureUnlockResult(t.Texture(),1,D3D_OK);
  Check(source::CurrentTexture(borrow,prior)&&query()&&witness.contentGeneration==prior.contentGeneration,"readonly mip Lock and Unlock preserve texture token");
  source::TextureLockResult(t.Texture(),0,0,D3D_OK);
  Check(!source::CurrentTexture(borrow,prior)&&!query()&&!witness.valid,"successful writable texture Lock invalidates old content and blocks borrow");
  source::TextureUnlockResult(t.Texture(),0,D3DERR_INVALIDCALL);
  Check(!query(),"failed texture Unlock leaves writer outstanding");
  source::TextureUnlockResult(t.Texture(),0,D3D_OK);
  Check(query()&&witness.generation==prior.generation&&witness.contentGeneration>prior.contentGeneration&&
    !source::CurrentTexture(borrow,prior),"successful texture Unlock exposes new content without changing resource generation");prior=witness;
  source::TextureLockResult(t.Texture(),2,D3DLOCK_NO_DIRTY_UPDATE,D3D_OK);
  Check(!query()&&!source::CurrentTexture(borrow,prior),"NO_DIRTY_UPDATE on a sublevel remains a writable lock");
  source::TextureUnlockResult(t.Texture(),2,D3D_OK);
  Check(query()&&witness.contentGeneration>prior.contentGeneration,"sublevel write changes whole texture content witness");prior=witness;
  source::TextureLockResult(t.Texture(),0,D3DLOCK_READONLY|D3DLOCK_NO_DIRTY_UPDATE,D3D_OK);
  source::TextureUnlockResult(t.Texture(),0,D3D_OK);
  Check(source::CurrentTexture(borrow,prior),"READONLY remains non-writing when combined with NO_DIRTY_UPDATE");
  source::RetireDevice(f.Device());
}
static void TextureSurfaces(std::unique_lock<std::recursive_mutex>& borrow,Fixture& f){
  TextureFixture t;source::TextureWitness witness{};
  auto query=[&](){return source::BorrowTexture(borrow,f.Device(),t.Texture(),witness);};
  Check(t.Remember(f.Device())&&query(),"surface owner texture created");auto prior=witness;
  Check(!source::RememberSurface(t.Texture(1),t.Surface(),t.SurfaceIdentity(),0),"surface cannot invent an unobserved texture owner");
  Check(!source::RememberSurface(t.Texture(),nullptr,t.SurfaceIdentity(),0)&&
    !source::RememberSurface(t.Texture(),t.Surface(),nullptr,0),"surface and canonical identities are required");
  Check(!source::RememberSurface(t.Texture(),t.Surface(),t.SurfaceIdentity(),3),"surface level must exist in observed texture layout");
  Check(source::RememberSurface(t.Texture(),t.Surface(),t.SurfaceIdentity(),1)&&source::CurrentTexture(borrow,prior),"observed level surface does not change texture content");
  Check(source::RememberSurface(t.Texture(),t.Surface(),t.SurfaceIdentity(),1)&&source::textureSurfaces.size()==1,"repeated GetSurfaceLevel observation does not duplicate relationship");
  source::SurfaceLockResult(t.Surface(),0,D3DERR_INVALIDCALL);
  Check(source::CurrentTexture(borrow,prior),"failed level-surface Lock preserves texture token");
  source::SurfaceLockResult(t.Surface(),D3DLOCK_READONLY,D3D_OK);source::SurfaceUnlockResult(t.Surface(),D3D_OK);
  Check(source::CurrentTexture(borrow,prior),"readonly surface access preserves parent texture token");
  source::SurfaceLockResult(t.Surface(),D3DLOCK_NO_DIRTY_UPDATE,D3D_OK);
  Check(!query()&&!source::CurrentTexture(borrow,prior),"writable surface alias invalidates and blocks its parent texture");
  source::SurfaceUnlockResult(t.Surface(),D3DERR_INVALIDCALL);Check(!query(),"failed surface Unlock leaves parent blocked");
  source::SurfaceUnlockResult(t.Surface(),D3D_OK);
  Check(query()&&witness.generation==prior.generation&&witness.contentGeneration>prior.contentGeneration,"surface Unlock exposes new parent content");prior=witness;
  // Runtime forwarding may visit both front doors for one physical mip lock.
  source::TextureLockResult(t.Texture(),1,0,D3D_OK);source::SurfaceLockResult(t.Surface(),0,D3D_OK);
  source::SurfaceUnlockResult(t.Surface(),D3D_OK);source::TextureUnlockResult(t.Texture(),1,D3D_OK);
  Check(query()&&!source::CurrentTexture(borrow,prior),"texture and surface forwarding share one mip access rather than leaking lock depth");prior=witness;
  source::SurfaceDCResult(t.Surface(),t.DC(),D3DERR_INVALIDCALL);
  Check(source::CurrentTexture(borrow,prior),"failed GetDC does not change parent texture token");
  source::SurfaceDCResult(t.Surface(),t.DC(),D3D_OK);
  Check(!query()&&!source::CurrentTexture(borrow,prior),"successful X8R8G8B8 GetDC invalidates and blocks parent content");
  source::SurfaceReleaseDCResult(t.Surface(),t.DC(),D3DERR_INVALIDCALL);Check(!query(),"failed ReleaseDC keeps parent blocked");
  source::SurfaceReleaseDCResult(t.Surface(),t.DC(),D3D_OK);
  Check(query()&&witness.contentGeneration>prior.contentGeneration&&witness.generation==prior.generation,"successful ReleaseDC exposes new content");prior=witness;
  source::ForgetTextureSurface(t.Surface());
  Check(source::CurrentTexture(borrow,prior)&&source::textureSurfaces.empty(),"closed level-surface final Release leaves parent texture usable");
  Check(source::RememberSurface(t.Texture(),t.Surface(),t.SurfaceIdentity(),0),"surface relationship recreated for access retirement");
  source::SurfaceLockResult(t.Surface(),D3DLOCK_READONLY,D3D_OK);source::ForgetTextureSurface(t.Surface());
  Check(!query()&&!source::CurrentTexture(borrow,prior)&&source::textureSurfaces.empty(),"surface Release with tracked readonly access fails closed for texture");
  Check(t.Remember(f.Device())&&source::RememberSurface(t.Texture(),t.Surface(),t.SurfaceIdentity(),0)&&query(),"fresh Create restores a retired surface owner");prior=witness;
  source::SurfaceDCResult(t.Surface(),t.DC(),D3D_OK);source::ForgetTextureSurface(t.Surface());
  Check(!query()&&!source::CurrentTexture(borrow,prior)&&source::textureSurfaces.empty(),"surface Release with outstanding DC cannot silently unblock texture");
  source::RetireDevice(f.Device());
}
static void TextureBounds(Fixture& f){
  TextureFixture t;
  auto remember=[&](const D3DSURFACE_DESC* desc,UINT count){return source::RememberTexture(f.Device(),t.Texture(),t.Identity(),desc,count);};
  Check(!source::RememberTexture(nullptr,t.Texture(),t.Identity(),t.levels,3),"texture Create requires device identity");
  Check(!source::RememberTexture(f.Device(),nullptr,t.Identity(),t.levels,3)&&
    !source::RememberTexture(f.Device(),t.Texture(),nullptr,t.levels,3),"texture and canonical identities are required");
  Check(!remember(nullptr,3)&&!remember(t.levels,0),"texture layout must be nonempty and present");
  Check(!remember(t.levels,33),"texture mip count above 32 rejects before reading caller array");
  D3DSURFACE_DESC invalid[3]{};
  auto reset=[&](){memcpy(invalid,t.levels,sizeof(invalid));};
  reset();invalid[0].Width=0;Check(!remember(invalid,3),"zero texture width rejects");
  reset();invalid[0].Height=0;Check(!remember(invalid,3),"zero texture height rejects");
  reset();invalid[1].Width=3;Check(!remember(invalid,3),"mip width must halve from previous level");
  reset();invalid[1].Height=2;Check(!remember(invalid,3),"mip height must halve independently");
  reset();invalid[1].Format=D3DFMT_A8R8G8B8;Check(!remember(invalid,3),"mixed mip formats reject");
  reset();for(auto& desc:invalid)desc.Format=D3DFMT_DXT1;Check(!remember(invalid,3),"compressed format remains outside ordinary texture cohort");
  reset();invalid[0].Pool=D3DPOOL_DEFAULT;Check(!remember(invalid,3),"default pool remains outside texture cohort");
  reset();invalid[2].Pool=D3DPOOL_SYSTEMMEM;Check(!remember(invalid,3),"every mip must belong to managed pool");
  reset();invalid[0].Usage=D3DUSAGE_RENDERTARGET;Check(!remember(invalid,3),"render-target usage rejects");
  reset();invalid[0].Usage=D3DUSAGE_AUTOGENMIPMAP;Check(!remember(invalid,3),"autogenerated mipmaps remain excluded");
  reset();invalid[0].Usage=D3DUSAGE_DYNAMIC;Check(!remember(invalid,3),"dynamic usage remains excluded");
  reset();invalid[0].Type=D3DRTYPE_TEXTURE;Check(!remember(invalid,3),"level descriptor must describe a surface");
  reset();invalid[0].MultiSampleType=D3DMULTISAMPLE_2_SAMPLES;Check(!remember(invalid,3),"multisampled level rejects");
  reset();invalid[0].MultiSampleQuality=1;Check(!remember(invalid,3),"ordinary level requires zero multisample quality");
  reset();for(auto& desc:invalid){desc.Width=1;desc.Height=1;}
  Check(!remember(invalid,2),"no mip may follow terminal one-by-one level");
  Check(remember(invalid,1),"single one-by-one texture is supported");source::ForgetTexture(t.Texture());
  reset();invalid[0].Width=5;invalid[0].Height=3;
  Check(remember(invalid,3),"odd dimensions use floor-halving independently down to one");source::ForgetTexture(t.Texture());
  D3DSURFACE_DESC maximum[32]{};
  for(UINT level=0;level<32;++level){maximum[level]=t.levels[0];maximum[level].Width=0x80000000u>>level;maximum[level].Height=1;}
  Check(remember(maximum,32),"32-entry metadata boundary accepts a complete halving chain without pixel allocation");source::ForgetTexture(t.Texture());
  Check(source::textures.empty()&&source::textureSurfaces.empty(),"rejected descriptors leave no partial texture records");
}
static void TextureLifetime(std::unique_lock<std::recursive_mutex>& borrow,Fixture& f){
  TextureFixture t;source::TextureWitness first{},second{},now{};
  Check(t.Remember(f.Device())&&t.Remember(f.Device(1),1)&&
    source::BorrowTexture(borrow,f.Device(),t.Texture(),first)&&source::BorrowTexture(borrow,f.Device(1),t.Texture(1),second),"independent devices own texture generations");
  Check(source::RememberSurface(t.Texture(),t.Surface(),t.SurfaceIdentity(),0)&&
    source::RememberSurface(t.Texture(1),t.Surface(1),t.SurfaceIdentity(1),2),"both devices have observed level surfaces");
  source::BeginReset(f.Device());
  Check(!source::CurrentTexture(borrow,first)&&source::CurrentTexture(borrow,second)&&source::textures.size()==1&&
    source::textureSurfaces.size()==1&&!source::textureSurfaces.count(t.Surface()),"Reset retires only target-device textures and related surfaces");
  Check(!t.Remember(f.Device()),"texture Create during Reset cannot qualify");source::EndReset(f.Device());
  Check(!source::BorrowTexture(borrow,f.Device(),t.Texture(),now),"EndReset does not resurrect managed texture witness");
  Check(t.Remember(f.Device())&&source::RememberSurface(t.Texture(),t.Surface(),t.SurfaceIdentity(),1)&&
    source::BorrowTexture(borrow,f.Device(),t.Texture(),now)&&now.generation>first.generation&&!source::CurrentTexture(borrow,first),"new successful texture Create establishes fresh post-reset generation");first=now;
  source::ForgetTexture(t.Texture());
  Check(!source::CurrentTexture(borrow,first)&&!source::textureSurfaces.count(t.Surface())&&source::CurrentTexture(borrow,second),"texture final Release also retires its surface relationships");
  source::RetireDevice(f.Device(1));
  Check(source::textures.empty()&&source::textureSurfaces.empty()&&!source::CurrentTexture(borrow,second),"device final Release retires dependent texture and surface records");
}
static void TextureAllocation(std::unique_lock<std::recursive_mutex>& borrow,Fixture& f){
  TextureFixture t;bool result=true;source::TextureWitness prior{},now{};
  {AllocationFault fail(0);result=t.Remember(f.Device());}
  Check(!result&&source::textures.empty()&&source::textureSurfaces.empty(),"texture descriptor allocation failure grants no live token");
  {AllocationFault fail(1);result=t.Remember(f.Device());}
  Check(!result&&source::textures.empty()&&source::textureSurfaces.empty(),"texture map allocation failure after descriptor copy leaves no partial record");
  Check(t.Remember(f.Device())&&source::RememberSurface(t.Texture(),t.Surface(),t.SurfaceIdentity(),0)&&
    source::BorrowTexture(borrow,f.Device(),t.Texture(),prior),"texture and surface before failed replacement");
  {AllocationFault fail(0);result=t.Remember(f.Device());}
  Check(!result&&!source::CurrentTexture(borrow,prior)&&source::textures.empty()&&source::textureSurfaces.empty(),"failed texture replacement cannot retain old generation or surface relationships");
  Check(t.Remember(f.Device())&&source::BorrowTexture(borrow,f.Device(),t.Texture(),prior),"texture before failed surface tracking");
  {AllocationFault fail(0);result=source::RememberSurface(t.Texture(),t.Surface(),t.SurfaceIdentity(),0);}
  Check(!result&&!source::CurrentTexture(borrow,prior)&&!source::BorrowTexture(borrow,f.Device(),t.Texture(),now)&&
    source::textures.empty()&&source::textureSurfaces.empty(),"surface relationship allocation failure fails closed for escaped texture writer");
  Check(t.Remember(f.Device())&&t.Remember(f.Device(1),1),"textures before Reset bookkeeping allocation failure");
  {AllocationFault fail(0);source::BeginReset(f.Device());}
  Check(!source::enabled&&source::textures.empty()&&source::textureSurfaces.empty(),"Reset allocation failure disables registry and retires all texture records");
  source::EndReset(f.Device());source::enabled=true;
}
static void TextureCases(std::unique_lock<std::recursive_mutex>& borrow,Fixture& f){
  Check(source::textures.empty()&&source::textureSurfaces.empty(),"texture cases start with empty owned registry");
  TextureBasic(borrow,f);TextureWrites(borrow,f);TextureSurfaces(borrow,f);TextureBounds(f);
  TextureLifetime(borrow,f);TextureAllocation(borrow,f);
  TextureFixture t;source::TextureWitness first{},other{};
  Check(t.Remember(f.Device())&&t.Remember(f.Device(1),1)&&source::BorrowTexture(borrow,f.Device(),t.Texture(),first)&&
    source::BorrowTexture(borrow,f.Device(1),t.Texture(1),other),"texture identities before permanent alias coverage block");
  Check(source::Remember(f.Device(),f.Vertex(),source::Kind::Vertex,96,D3DFMT_VERTEXDATA),"independent buffer record before texture-only block");
  source::BlockTextureDevice(f.Device());
  Check(!source::CurrentTexture(borrow,first)&&source::CurrentTexture(borrow,other)&&source::records.count(f.Vertex()),
    "escaped texture/device interface blocks only affected device textures, preserving ordinary buffer records");
  source::BeginReset(f.Device());source::EndReset(f.Device());
  Check(!t.Remember(f.Device())&&!source::BorrowTexture(borrow,f.Device(),t.Texture(),first),"Reset cannot revive a device with escaped writer coverage");
  source::RetireDevice(f.Device(1));source::textureBlockedDevices.clear();
  Check(t.Remember(f.Device())&&t.Remember(f.Device(1),1),"textures before alias-block bookkeeping allocation failure");
  {AllocationFault fail(0);source::BlockTextureDevice(f.Device());}
  Check(!source::textureCoverage&&source::textures.empty()&&source::textureSurfaces.empty()&&!t.Remember(f.Device(1)),
    "failed alias-block allocation fails closed for all texture qualification");
  source::textureCoverage=true;source::textureBlockedDevices.clear();
  Check(source::textures.empty()&&source::textureSurfaces.empty(),"texture cases restore owned records without COM methods");
}
static void Run(){
  Check(!source::enabled&&source::records.empty()&&source::resetting.empty()&&!testRemixApi,"fresh registry fixture without API");
  void* table[17]{};void** tablePointer=table;
  auto device=reinterpret_cast<IDirect3DDevice9*>(&tablePointer);
  Check(!TransportDeviceHooked(device),"missing installed device lifetime hooks cannot qualify");
  table[2]=reinterpret_cast<void*>(DeviceRelease);
  Check(!TransportDeviceHooked(device),"Release without Reset hook is insufficient");
  table[16]=reinterpret_cast<void*>(::Reset);
  Check(TransportDeviceHooked(device),"actual owned table has both expected device lifetime hooks");
  table[2]=nullptr;Check(!TransportDeviceHooked(device),"Reset without Release hook is insufficient");
  source::enabled=true;Fixture fixture;std::unique_lock<std::recursive_mutex> borrow(guard);
  Basic(borrow,fixture);Reset(borrow,fixture);Bounds(fixture);Allocation(fixture);
  TextureCases(borrow,fixture);
  source::records.clear();source::resetting.clear();source::declarationCount=0;source::enabled=false;source::nextGeneration=0;
  memset(source::created,0,sizeof(source::created));memset(source::retired,0,sizeof(source::retired));
  source::resets=source::rejected=source::reused=source::queries=source::accepted=0;
  Check(!testRemixApi&&source::records.empty()&&source::resetting.empty(),"fixture restores own registry state without invoking COM or API");
}
}
int main(){try{transport_test::Watchdog watchdog;transport_test::Run();
  printf("{\"status\":\"PASS\",\"checks\":%u,\"gpu\":false,\"nativeGameCodeExecuted\":false,\"comMethodsCalled\":0}\n",transport_test::checks);return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL %s\n",error.what());return 1;}}
