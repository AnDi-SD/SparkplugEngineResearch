#pragma once
// Own bounded draw-state/texture evidence. No material interpretation, shader
// replacement or Remix submission. Texture pointers are temporarily owned COM
// references; saved data contains only copied bytes and provenance numbers.
#include "winx_skin_draw_state.h"
namespace skin_material_capture {
static unsigned lastReason,lastReadFailure;
static std::array<uint32_t,8> lastTexture{}; // width,height,levels,format,pool,tracked,blocked,coverage.
static bool Fail(unsigned reason){lastReason=reason;return false;}
using winx_remix::skin_draw_state::StateKeys;
using winx_remix::skin_draw_state::StageKeys;
using winx_remix::skin_draw_state::SamplerKeys;
using winx_remix::skin_draw_state::Snapshot;
static bool Read(IDirect3DDevice9* device,Snapshot& output){
  return winx_remix::skin_draw_state::ReadSnapshot(device,output,&lastReadFailure);
}
static bool WriteFresh(const wchar_t* path,const void* bytes,size_t size){
  if(!size||size>8*1024*1024)return false;
  const auto file=CreateFileW(path,GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(file==INVALID_HANDLE_VALUE)return false;
  DWORD written=0;const bool complete=WriteFile(file,bytes,DWORD(size),&written,nullptr)&&written==size;
  const bool closed=CloseHandle(file)!=FALSE;return complete&&closed;
}
struct Texture {unsigned file=0;uint64_t generation=0,content=0;size_t bytes=0;};
static std::vector<Texture> textures;
static size_t textureBytes;
static bool writeStopped;
static bool CopyTexture(const std::unique_lock<std::recursive_mutex>& borrow,IDirect3DDevice9* device,
                        IDirect3DTexture9* texture,std::vector<uint8_t>& output,native_transport_source::TextureWitness& witness){
  using namespace native_transport_source;
  TextureWitness original{};if(!BorrowTexture(borrow,device,texture,original))return false;
  if(!original.width||!original.height||original.width>4096||original.height>4096||original.levels>16)return false;
  size_t total=28;UINT width=original.width,height=original.height;
  for(unsigned level=0;level<original.levels;++level){total+=16+uint64_t(width)*height*4;if(total>8*1024*1024)return false;
    width=(std::max)(1u,width/2);height=(std::max)(1u,height/2);}
  std::vector<uint8_t> bytes(total);const uint32_t header[]={0x31544b53,1,uint32_t(total),original.width,original.height,original.levels,uint32_t(original.format)};
  memcpy(bytes.data(),header,sizeof(header));size_t offset=sizeof(header);width=original.width;height=original.height;
  for(unsigned level=0;level<original.levels;++level){D3DSURFACE_DESC desc{};
    if(FAILED(texture->GetLevelDesc(level,&desc))||desc.Width!=width||desc.Height!=height||desc.Format!=original.format||desc.Pool!=D3DPOOL_MANAGED||desc.Usage)return false;
    const uint32_t mip[]={width,height,width*4,width*height*4};memcpy(bytes.data()+offset,mip,sizeof(mip));offset+=sizeof(mip);
    D3DLOCKED_RECT locked{};if(FAILED(texture->LockRect(level,&locked,nullptr,D3DLOCK_READONLY)))return false;
    const bool valid=locked.pBits&&locked.Pitch>=int(width*4);
    if(valid)for(UINT row=0;row<height;++row)memcpy(bytes.data()+offset+size_t(row)*width*4,static_cast<const uint8_t*>(locked.pBits)+size_t(row)*locked.Pitch,width*4);
    const auto unlock=texture->UnlockRect(level);if(!valid||FAILED(unlock))return false;
    offset+=mip[3];width=(std::max)(1u,width/2);height=(std::max)(1u,height/2);
  }
  if(offset!=bytes.size()||!CurrentTexture(borrow,original))return false;
  output=std::move(bytes);witness=original;return true;
}
static bool Save(const wchar_t* directory,IDirect3DDevice9* device,unsigned packet,unsigned draw,unsigned shader,
                 uint32_t renderer,uint32_t nativeMaterial,IDirect3DVertexShader9* expectedShader){
  lastReason=lastReadFailure=0;lastTexture={};if(writeStopped)return Fail(1);
  std::unique_lock<std::recursive_mutex> borrow(guard);
  Snapshot before{},after{};if(!Read(device,before))return Fail(2);
  if(before.vertexShader!=reinterpret_cast<uintptr_t>(expectedShader))return Fail(3);
  IDirect3DBaseTexture9* base=nullptr;IDirect3DTexture9* texture=nullptr;
  struct Release {IDirect3DBaseTexture9*& base;IDirect3DTexture9*& texture;~Release(){if(texture)texture->Release();if(base)base->Release();}} release{base,texture};
  if(FAILED(device->GetTexture(0,&base))||!base||reinterpret_cast<uintptr_t>(base)!=before.textures[0]||
     FAILED(base->QueryInterface(__uuidof(IDirect3DTexture9),reinterpret_cast<void**>(&texture)))||!texture)return Fail(4);
  D3DSURFACE_DESC details{};
  if(FAILED(texture->GetLevelDesc(0,&details)))return Fail(5);
  lastTexture={details.Width,details.Height,texture->GetLevelCount(),uint32_t(details.Format),uint32_t(details.Pool),
    uint32_t(native_transport_source::textures.count(texture)),uint32_t(native_transport_source::textureBlockedDevices.count(device)),uint32_t(native_transport_source::textureCoverage)};
  native_transport_source::TextureWitness witness{};
  if(!native_transport_source::BorrowTexture(borrow,device,texture,witness))return Fail(6);
  unsigned textureFile=0;
  for(const auto& saved:textures)if(saved.generation==witness.generation&&saved.content==witness.contentGeneration){textureFile=saved.file;break;}
  if(!textureFile){if(textures.size()>=64)return Fail(7);textures.reserve(64);std::vector<uint8_t> bytes;
    if(!CopyTexture(borrow,device,texture,bytes,witness)||textureBytes+bytes.size()>32*1024*1024)return Fail(8);
    if(!Read(device,after)||!(before==after)||!native_transport_source::CurrentTexture(borrow,witness))return Fail(9);
    textureFile=unsigned(textures.size()+1);wchar_t path[MAX_PATH]{};
    if(swprintf_s(path,L"%s/texture-%04u.skt",directory,textureFile)<0||!WriteFresh(path,bytes.data(),bytes.size())){writeStopped=true;return Fail(10);}
    textures.push_back({textureFile,witness.generation,witness.contentGeneration,bytes.size()});textureBytes+=bytes.size();
  }
  if(!Read(device,after)||!(before==after)||!native_transport_source::CurrentTexture(borrow,witness))return Fail(11);
  std::ostringstream out;
  out<<"{\"schema\":1,\"packet\":"<<packet<<",\"frame\":"<<frameId<<",\"draw\":"<<draw<<",\"shader\":"<<shader
     <<",\"renderer\":"<<renderer<<",\"nativeMaterial\":"<<nativeMaterial<<",\"textureFile\":"<<textureFile
     <<",\"textureGeneration\":"<<witness.generation<<",\"textureContentGeneration\":"<<witness.contentGeneration
     <<",\"stateStable\":true,\"textureStable\":true,\"materialQualified\":false,\"pixelShader\":"<<(before.pixelShader?"true":"false")<<",\"states\":[";
  for(size_t i=0;i<std::size(StateKeys);++i)out<<(i?",":"")<<'['<<StateKeys[i]<<','<<before.states[i]<<']';
  out<<"],\"stages\":[";
  for(unsigned stage=0;stage<8;++stage){out<<(stage?",":"")<<'[';
    for(size_t i=0;i<std::size(StageKeys);++i)out<<(i?",":"")<<'['<<StageKeys[i]<<','<<before.stages[stage][i]<<']';out<<']';}
  out<<"],\"samplers\":[";
  for(unsigned stage=0;stage<8;++stage){out<<(stage?",":"")<<'[';
    for(size_t i=0;i<std::size(SamplerKeys);++i)out<<(i?",":"")<<'['<<SamplerKeys[i]<<','<<before.samplers[stage][i]<<']';out<<']';}
  out<<"],\"textureBindings\":[";for(unsigned stage=0;stage<8;++stage)out<<(stage?",":"")<<before.textures[stage];
  out<<"],\"textureTransformBits\":[";for(unsigned i=0;i<16;++i)out<<(i?",":"")<<before.transform[i];out<<"]}\n";
  const auto text=out.str();if(text.size()>16384)return Fail(12);wchar_t path[MAX_PATH]{};
  if(swprintf_s(path,L"%s/packet-%04u-draw-%06u.material.json",directory,packet,draw)<0||!WriteFresh(path,text.data(),text.size())){writeStopped=true;return Fail(13);}
  return true;
}
} // namespace skin_material_capture
