#pragma once
// Own synchronous D3D adapter. The standard-C++ source owns only copied words;
// this adapter owns the temporary COM reference and every successful mip lock.
#include "winx_texture_mip_snapshot.h"
#include <limits>

namespace material_channels {
using TextureMipSource=winx_remix::TextureMipSnapshotSource;
static_assert(TextureMipSource::kFormatArgb==D3DFMT_A8R8G8B8&&TextureMipSource::kFormatXrgb==D3DFMT_X8R8G8B8,
  "Snapshot packed-word formats must match the D3D adapter");

template<class Current>
static inline TextureMipSource::Snapshot CaptureTextureMipSnapshotCurrent(IDirect3DBaseTexture9* base,const Current& current) {
  if(!base||!current())return {};
  TextureMipSource::Snapshot output;
  {
    base->AddRef();
    struct Reference {IDirect3DBaseTexture9* value;~Reference(){value->Release();}} reference{base};
    // AddRef can reenter the adapter. No further texture method is called until
    // its source witness has been checked again under the temporary reference.
    if(!current()||base->GetType()!=D3DRTYPE_TEXTURE||!current())return {};
    auto texture=static_cast<IDirect3DTexture9*>(base);
    const auto levels=texture->GetLevelCount();
    if(!levels||levels>TextureMipSource::kLevelLimit||!current())return {};
    D3DSURFACE_DESC top{};
    if(!current()||FAILED(texture->GetLevelDesc(0,&top))||!current()||
       top.Pool!=D3DPOOL_MANAGED||top.Usage||top.Type!=D3DRTYPE_SURFACE||
       top.MultiSampleType!=D3DMULTISAMPLE_NONE||top.MultiSampleQuality)return {};
    const TextureMipSource::Description description{top.Width,top.Height,uint32_t(top.Format),levels};
    output=TextureMipSource::Capture(description,[&](uint32_t level,uint32_t width,uint32_t height,std::vector<uint32_t>& pixels) {
      if(!current())return false;
      D3DSURFACE_DESC desc{};
      if(FAILED(texture->GetLevelDesc(level,&desc))||!current()||desc.Width!=width||desc.Height!=height||
         desc.Format!=top.Format||desc.Pool!=D3DPOOL_MANAGED||desc.Usage||desc.Type!=D3DRTYPE_SURFACE||
         desc.MultiSampleType!=D3DMULTISAMPLE_NONE||desc.MultiSampleQuality)return false;
      D3DLOCKED_RECT lock{};
      if(FAILED(texture->LockRect(level,&lock,nullptr,D3DLOCK_READONLY)))return false;
      struct Locked {
        IDirect3DTexture9* texture;UINT level;bool open=true;
        HRESULT Close(){open=false;return texture->UnlockRect(level);}
        ~Locked(){if(open)texture->UnlockRect(level);}
      } locked{texture,level};
      // The core allocated this exact destination before LockRect. No borrowed
      // pBits escapes, and false/throwing current still closes the readonly lock.
      bool valid=current()&&lock.pBits&&lock.Pitch>=INT(width*4);
      if(valid) {
        const auto span=uint64_t(height-1)*uint64_t(lock.Pitch)+uint64_t(width)*4;
        valid=span<=(std::numeric_limits<size_t>::max)()&&
          uintptr_t(lock.pBits)<=(std::numeric_limits<uintptr_t>::max)()-(span-1);
      }
      if(valid)for(uint32_t row=0;row<height;++row)
        memcpy(pixels.data()+size_t(row)*width,static_cast<const uint8_t*>(lock.pBits)+size_t(row)*lock.Pitch,size_t(width)*4);
      const auto result=locked.Close();
      return valid&&SUCCEEDED(result)&&current();
    },current);
  }
  // Release may be the final owner or itself reenter Reset. A retained ref is
  // not proof that the game's resource is still current after we let it go.
  if(!output.valid||!current())return {};
  return output;
}

template<class Current>
static inline TextureMipSource::Snapshot CaptureBoundTextureMipSnapshotCurrent(IDirect3DDevice9* device,const Current& current) {
  if(!device||!current())return {};
  TextureMipSource::Snapshot output;
  {
    IDirect3DBaseTexture9* base=nullptr;
    if(FAILED(device->GetTexture(0,&base))||!base)return {};
    struct Reference {IDirect3DBaseTexture9* value;~Reference(){value->Release();}} reference{base};
    output=CaptureTextureMipSnapshotCurrent(base,current);
  }
  if(!output.valid||!current())return {};
  return output;
}
} // namespace material_channels
