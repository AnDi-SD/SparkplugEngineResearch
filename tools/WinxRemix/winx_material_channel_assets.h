// Own bounded DDS output for independent material coefficients. Source textures
// are only read. UNORM8 introduces at most half a channel unit per saved texel.
#pragma once
#include "winx_texture_mip_capture.h"
namespace material_channels {
static size_t assetBytes;
static inline bool TextureMipDescriptionMatches(const TextureMipSource::Snapshot& snapshot,
                                                const TextureMipSource::Description* expected) {
  const auto& actual=snapshot.description;
  return !expected||(actual.width==expected->width&&actual.height==expected->height&&
    actual.format==expected->format&&actual.levels==expected->levels);
}
// Exact old channel identity: layout then tightly packed raw rows of every mip.
// XRGB high bytes are preserved here and normalized only for DDS output.
static inline uint64_t TextureMipSnapshotHash(const TextureMipSource::Snapshot& snapshot) {
  if(!TextureMipSource::IsValid(snapshot))return 0;
  const auto& description=snapshot.description;
  const uint32_t layout[]={description.width,description.height,description.format,description.levels};
  XXH3_state_t state{};XXH3_64bits_reset(&state);XXH3_64bits_update(&state,layout,sizeof(layout));
  for(const auto& mip:snapshot.mips)XXH3_64bits_update(&state,mip.pixels.data(),mip.pixels.size()*sizeof(mip.pixels[0]));
  return XXH3_64bits_digest(&state);
}
// File publication consumes only owned pixels. No native/COM method is called.
template<class Current>
static inline bool WriteTextureMipSnapshotCurrent(const TextureMipSource::Snapshot& snapshot,const wchar_t* path,const RGB& tint,const Current& current) {
  if(!current()||!Bounded(tint))return false;
  if(GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES)return true;
  if(!TextureMipSource::IsValid(snapshot))return false;
  size_t total=32;
  for(const auto& mip:snapshot.mips)total+=mip.pixels.size();
  if(total>6*1024*1024||assetBytes+total*4>128*1024*1024)return false;
  std::vector<uint32_t> bytes(total,0);
  const auto& description=snapshot.description;const auto levels=description.levels;
  bytes[0]=0x20534444;bytes[1]=124;bytes[2]=0x100f|(levels>1?0x20000:0);
  bytes[3]=description.height;bytes[4]=description.width;bytes[5]=description.width*4;bytes[7]=levels;
  bytes[19]=32;bytes[20]=0x41;bytes[22]=32;bytes[23]=0xff0000;bytes[24]=0xff00;bytes[25]=0xff;bytes[26]=0xff000000;
  bytes[27]=0x1000|(levels>1?0x400008:0);
  size_t offset=32;
  for(const auto& mip:snapshot.mips)for(auto pixel:mip.pixels) {
    if(description.format==TextureMipSource::kFormatXrgb)pixel|=0xff000000u;
    bytes[offset++]=Tint(pixel,tint);
  }
  if(!current())return false;
  std::wstring temporary=std::wstring(path)+L".tmp";FILE* output=nullptr;
  if(_wfopen_s(&output,temporary.c_str(),L"wb")||!output)return false;
  const bool written=fwrite(bytes.data(),4,bytes.size(),output)==bytes.size();
  const bool closed=fclose(output)==0;
  if(!written||!closed||!MoveFileExW(temporary.c_str(),path,MOVEFILE_WRITE_THROUGH)){DeleteFileW(temporary.c_str());return false;}
  assetBytes+=bytes.size()*4;return true;
}
// A reference or current transport borrow must outlive this synchronous read.
// This function neither takes ownership nor changes device texture bindings.
template<class Current>
static inline bool WriteTextureSourceCurrent(IDirect3DBaseTexture9* base,const wchar_t* path,const RGB& tint,const Current& current) {
  if(!current()||!Bounded(tint))return false;
  if(GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES)return true;
  if(!base)return false;
  if(base->GetType()!=D3DRTYPE_TEXTURE)return false;
  auto texture=static_cast<IDirect3DTexture9*>(base);D3DSURFACE_DESC top{};
  const auto levels=texture->GetLevelCount();
  if(!levels||levels>13||FAILED(texture->GetLevelDesc(0,&top))||top.Pool!=D3DPOOL_MANAGED||
     !top.Width||!top.Height||top.Width>2048||top.Height>2048||
     (top.Format!=D3DFMT_A8R8G8B8&&top.Format!=D3DFMT_X8R8G8B8))return false;
  std::vector<uint32_t> bytes(32,0);bytes[0]=0x20534444;bytes[1]=124;
  bytes[2]=0x100f|(levels>1?0x20000:0);bytes[3]=top.Height;bytes[4]=top.Width;bytes[5]=top.Width*4;bytes[7]=levels;
  bytes[19]=32;bytes[20]=0x41;bytes[22]=32;bytes[23]=0xff0000;bytes[24]=0xff00;bytes[25]=0xff;bytes[26]=0xff000000;
  bytes[27]=0x1000|(levels>1?0x400008:0);
  for(unsigned level=0;level<levels;++level) {
    D3DSURFACE_DESC desc{};D3DLOCKED_RECT lock{};
    if(FAILED(texture->GetLevelDesc(level,&desc))||desc.Width!=(std::max)(1u,top.Width>>level)||
       desc.Height!=(std::max)(1u,top.Height>>level)||desc.Format!=top.Format)return false;
    const auto first=bytes.size(),count=size_t(desc.Width)*desc.Height;
    if(first+count>6*1024*1024||assetBytes+(first+count)*4>128*1024*1024)return false;
    bytes.resize(first+count);
    if(FAILED(texture->LockRect(level,&lock,nullptr,D3DLOCK_READONLY)))return false;
    const bool valid=lock.pBits&&lock.Pitch>=INT(desc.Width*4);
    if(valid)for(unsigned y=0;y<desc.Height;++y)for(unsigned x=0;x<desc.Width;++x) {
      DWORD pixel=0;memcpy(&pixel,static_cast<const uint8_t*>(lock.pBits)+size_t(y)*lock.Pitch+x*4,4);
      if(top.Format==D3DFMT_X8R8G8B8)pixel|=0xff000000u;
      bytes[first+size_t(y)*desc.Width+x]=Tint(pixel,tint);
    }
    if(FAILED(texture->UnlockRect(level))||!valid)return false;
  }
  // All mip bytes are owned before this fence. A reentrant content/lifetime
  // transition must not publish an asset under the earlier content hash.
  if(!current())return false;
  std::wstring temporary=std::wstring(path)+L".tmp";FILE* output=nullptr;
  if(_wfopen_s(&output,temporary.c_str(),L"wb")||!output)return false;
  const bool written=fwrite(bytes.data(),4,bytes.size(),output)==bytes.size();
  const bool closed=fclose(output)==0;
  if(!written||!closed||!MoveFileExW(temporary.c_str(),path,MOVEFILE_WRITE_THROUGH)){DeleteFileW(temporary.c_str());return false;}
  assetBytes+=bytes.size()*4;return true;
}
static inline bool WriteTextureSource(IDirect3DBaseTexture9* base,const wchar_t* path,const RGB& tint) {
  return WriteTextureSourceCurrent(base,path,tint,[]{return true;});
}
template<class Current>
static inline bool WriteTextureCurrent(IDirect3DDevice9* d,const wchar_t* path,const RGB& tint,const Current& current) {
  if(!current()||!Bounded(tint))return false;
  if(GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES)return true;
  IDirect3DBaseTexture9* base=nullptr;
  if(FAILED(d->GetTexture(0,&base))||!base)return false;
  struct Release {IDirect3DBaseTexture9* p;~Release(){p->Release();}} release{base};
  return WriteTextureSourceCurrent(base,path,tint,current);
}
static inline bool WriteTexture(IDirect3DDevice9* d,const wchar_t* path,const RGB& tint) {
  return WriteTextureCurrent(d,path,tint,[]{return true;});
}
}
