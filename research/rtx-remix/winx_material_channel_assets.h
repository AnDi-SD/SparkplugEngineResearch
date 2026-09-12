// Own bounded DDS output for independent material coefficients. Source textures
// are only read. UNORM8 introduces at most half a channel unit per saved texel.
#pragma once
namespace material_channels {
static size_t assetBytes;
static inline bool WriteTexture(IDirect3DDevice9* d,const wchar_t* path,const RGB& tint) {
  if(!Bounded(tint))return false;
  if(GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES)return true;
  IDirect3DBaseTexture9* base=nullptr;
  if(FAILED(d->GetTexture(0,&base))||!base)return false;
  struct Release {IDirect3DBaseTexture9* p;~Release(){p->Release();}} release{base};
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
  std::wstring temporary=std::wstring(path)+L".tmp";FILE* output=nullptr;
  if(_wfopen_s(&output,temporary.c_str(),L"wb")||!output)return false;
  const bool written=fwrite(bytes.data(),4,bytes.size(),output)==bytes.size();
  const bool closed=fclose(output)==0;
  if(!written||!closed||!MoveFileExW(temporary.c_str(),path,MOVEFILE_WRITE_THROUGH)){DeleteFileW(temporary.c_str());return false;}
  assetBytes+=bytes.size()*4;return true;
}
}
