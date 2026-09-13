// Own D3D transition witness. All tracked mutations serialize with draw's
// existing recursive guard. Getters are read-only; reentrant setters and state
// block Apply invalidate the entire read operation, including failed setters.
#pragma once
namespace d3d9_state_witness {
static uint64_t serial=1;
static unsigned mutations;
struct Mutation {
  std::lock_guard<std::recursive_mutex> lock{guard};
  Mutation(){++serial;++mutations;}
  ~Mutation(){--mutations;++serial;}
};
// Reset can synchronize with runtime worker threads. Preserve its existing
// unlocked external call, but keep every observation unqualified until it ends.
struct DetachedMutation {
  DetachedMutation(){std::lock_guard<std::recursive_mutex> lock(guard);++serial;++mutations;}
  ~DetachedMutation(){std::lock_guard<std::recursive_mutex> lock(guard);--mutations;++serial;}
};
struct Device {bool blocked=false;void* vertexShader=nullptr;void* pixelShader=nullptr;void* reset=nullptr;void* release=nullptr;};
static std::map<IDirect3DDevice9*,Device> devices;
static std::map<IDirect3DStateBlock9*,IDirect3DDevice9*> blocks;
static void Block(IDirect3DDevice9* device){++serial;const auto found=devices.find(device);if(found!=devices.end())found->second.blocked=true;}
template<unsigned Slot,class... Args> static HRESULT STDMETHODCALLTYPE Set(IDirect3DDevice9* device,Args... args) {
  Mutation mutation;using F=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,Args...);
  return Original<F>(device,Slot)(device,args...);
}
static bool TrackBlock(IDirect3DDevice9*,IDirect3DStateBlock9*);
static HRESULT STDMETHODCALLTYPE Apply(IDirect3DStateBlock9* block) {
  Mutation mutation;using F=HRESULT(STDMETHODCALLTYPE*)(IDirect3DStateBlock9*);
  // A foreign/unregistered state block using this shared vtable could affect
  // any tracked device. Preserve the original call and disable qualification.
  if(!blocks.count(block))for(auto& item:devices)item.second.blocked=true;
  return Original<F>(block,5)(block);
}
static ULONG STDMETHODCALLTYPE ReleaseBlock(IDirect3DStateBlock9* block) {
  Mutation mutation;
  using F=ULONG(STDMETHODCALLTYPE*)(IDirect3DStateBlock9*);
  const auto refs=Original<F>(block,2)(block);if(!refs)blocks.erase(block);return refs;
}
static HRESULT STDMETHODCALLTYPE QueryBlock(IDirect3DStateBlock9* block,REFIID iid,void** result) {
  std::lock_guard<std::recursive_mutex> lock(guard);
  const auto found=blocks.find(block);const auto device=found==blocks.end()?nullptr:found->second;
  using F=HRESULT(STDMETHODCALLTYPE*)(IDirect3DStateBlock9*,REFIID,void**);
  const auto hr=Original<F>(block,0)(block,iid,result);
  // Identity aliases do not add a lifetime owner. An unfamiliar tear-off may
  // expose Apply through another unobserved table; do not assume its layout.
  if(SUCCEEDED(hr)&&result&&*result&&*result!=block) {
    if(device)Block(device);else{++serial;for(auto& item:devices)item.second.blocked=true;}
  }
  return hr;
}
static bool TrackBlock(IDirect3DDevice9* device,IDirect3DStateBlock9* block) {
  if(!block)return false;
  try {
    if(blocks.size()>=4096&&!blocks.count(block))return false;
    auto table=*reinterpret_cast<void***>(block);
    if(!table||!table[0]||!table[2]||!table[5])return false;
    Patch(block,0,reinterpret_cast<void*>(QueryBlock));Patch(block,2,reinterpret_cast<void*>(ReleaseBlock));
    Patch(block,5,reinterpret_cast<void*>(Apply));
    if(table[0]!=reinterpret_cast<void*>(QueryBlock)||table[2]!=reinterpret_cast<void*>(ReleaseBlock)||table[5]!=reinterpret_cast<void*>(Apply))return false;
    const auto found=blocks.find(block);if(found!=blocks.end()&&found->second!=device)return false;
    blocks[block]=device;return true;
  }catch(const std::bad_alloc&){return false;}
}
static HRESULT STDMETHODCALLTYPE CreateBlock(IDirect3DDevice9* device,D3DSTATEBLOCKTYPE type,IDirect3DStateBlock9** result) {
  Mutation mutation;using F=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,D3DSTATEBLOCKTYPE,IDirect3DStateBlock9**);
  const auto hr=Original<F>(device,59)(device,type,result);
  if(SUCCEEDED(hr)&&(!result||!TrackBlock(device,*result)))Block(device);return hr;
}
static HRESULT STDMETHODCALLTYPE EndBlock(IDirect3DDevice9* device,IDirect3DStateBlock9** result) {
  Mutation mutation;using F=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,IDirect3DStateBlock9**);
  const auto hr=Original<F>(device,61)(device,result);
  if(SUCCEEDED(hr)&&(!result||!TrackBlock(device,*result)))Block(device);return hr;
}
struct Hook {unsigned slot;void* function;};
static const Hook hooks[]={
  {37,reinterpret_cast<void*>(Set<37,DWORD,IDirect3DSurface9*>)},
  {39,reinterpret_cast<void*>(Set<39,IDirect3DSurface9*>)},
  {44,reinterpret_cast<void*>(Set<44,D3DTRANSFORMSTATETYPE,const D3DMATRIX*>)},
  {46,reinterpret_cast<void*>(Set<46,D3DTRANSFORMSTATETYPE,const D3DMATRIX*>)},
  {47,reinterpret_cast<void*>(Set<47,const D3DVIEWPORT9*>)},
  {49,reinterpret_cast<void*>(Set<49,const D3DMATERIAL9*>)},
  {57,reinterpret_cast<void*>(Set<57,D3DRENDERSTATETYPE,DWORD>)},
  {59,reinterpret_cast<void*>(CreateBlock)},
  {60,reinterpret_cast<void*>(Set<60>)},
  {61,reinterpret_cast<void*>(EndBlock)},
  {65,reinterpret_cast<void*>(Set<65,DWORD,IDirect3DBaseTexture9*>)},
  {67,reinterpret_cast<void*>(Set<67,DWORD,D3DTEXTURESTAGESTATETYPE,DWORD>)},
  {69,reinterpret_cast<void*>(Set<69,DWORD,D3DSAMPLERSTATETYPE,DWORD>)},
  {75,reinterpret_cast<void*>(Set<75,const RECT*>)},
  {87,reinterpret_cast<void*>(Set<87,IDirect3DVertexDeclaration9*>)},
  {89,reinterpret_cast<void*>(Set<89,DWORD>)},
  {100,reinterpret_cast<void*>(Set<100,UINT,IDirect3DVertexBuffer9*,UINT,UINT>)},
  {102,reinterpret_cast<void*>(Set<102,UINT,UINT>)},
  {104,reinterpret_cast<void*>(Set<104,IDirect3DIndexBuffer9*>)}
};
static bool Covered(IDirect3DDevice9* device,const Device& entry) {
  const auto table=*reinterpret_cast<void***>(device);
  if(!table||entry.blocked||table[92]!=entry.vertexShader||table[107]!=entry.pixelShader||
     !entry.reset||table[16]!=entry.reset||!entry.release||table[2]!=entry.release)return false;
  for(const auto& hook:hooks)if(table[hook.slot]!=hook.function)return false;
  for(const auto& block:blocks)if(block.second==device) {
    const auto methods=*reinterpret_cast<void***>(block.first);
    if(!methods||methods[0]!=reinterpret_cast<void*>(QueryBlock)||methods[2]!=reinterpret_cast<void*>(ReleaseBlock)||
       methods[5]!=reinterpret_cast<void*>(Apply))return false;
  }
  return true;
}
static bool Install(IDirect3DDevice9* device,void* reset,void* release,void* auditedVS=nullptr,void* auditedPS=nullptr) {
  std::lock_guard<std::recursive_mutex> lock(guard);++serial;
  if(!device||devices.count(device))return false;
  try {
    if(devices.size()>=128)return false;
    auto inserted=devices.emplace(device,Device{});auto& entry=inserted.first->second;
    entry.reset=reset;entry.release=release;
    auto table=*reinterpret_cast<void***>(device);
    for(const auto& hook:hooks) {
      if(!table[hook.slot]){entry.blocked=true;return false;}
      Patch(device,hook.slot,hook.function);
    }
    const auto shader=[&](unsigned slot,void* audited,void* plain){
      if(audited&&table[slot]==audited)return audited;
      if(!table[slot])return static_cast<void*>(nullptr);
      Patch(device,slot,plain);return plain;
    };
    entry.vertexShader=shader(92,auditedVS,reinterpret_cast<void*>(Set<92,IDirect3DVertexShader9*>));
    entry.pixelShader=shader(107,auditedPS,reinterpret_cast<void*>(Set<107,IDirect3DPixelShader9*>));
    entry.blocked=!Covered(device,entry);return !entry.blocked;
  }catch(const std::bad_alloc&){Block(device);return false;}
}
static void Retire(IDirect3DDevice9* device) {
  std::lock_guard<std::recursive_mutex> lock(guard);++serial;devices.erase(device);
  for(auto it=blocks.begin();it!=blocks.end();)if(it->second==device)it=blocks.erase(it);else ++it;
}
struct Witness {IDirect3DDevice9* device=nullptr;uint64_t serial=0;bool valid=false;};
static bool Read(const std::unique_lock<std::recursive_mutex>& borrow,IDirect3DDevice9* device,Witness& out) {
  out={};if(!borrow.owns_lock()||borrow.mutex()!=&guard||mutations||!device)return false;
  const auto found=devices.find(device);
  if(found==devices.end()||!Covered(device,found->second))return false;
  out={device,serial,true};return true;
}
static bool Current(const std::unique_lock<std::recursive_mutex>& borrow,const Witness& witness) {
  Witness now{};return witness.valid&&Read(borrow,witness.device,now)&&witness.serial==now.serial;
}
}
