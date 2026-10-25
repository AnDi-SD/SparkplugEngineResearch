#pragma once
// Own copied D3D9 state shared by evidence and explicit Skin preparation.
// Binding values are identity tokens, not owning COM references. Read alone
// does not prove that state stayed unchanged across reentrant external calls.
namespace winx_remix::skin_draw_state {
constexpr D3DRENDERSTATETYPE StateKeys[]={D3DRS_ZENABLE,D3DRS_ZWRITEENABLE,D3DRS_ZFUNC,
  D3DRS_ALPHABLENDENABLE,D3DRS_SRCBLEND,D3DRS_DESTBLEND,D3DRS_BLENDOP,
  D3DRS_ALPHATESTENABLE,D3DRS_ALPHAFUNC,D3DRS_ALPHAREF,D3DRS_CULLMODE,D3DRS_COLORWRITEENABLE,
  D3DRS_SEPARATEALPHABLENDENABLE,D3DRS_SRCBLENDALPHA,D3DRS_DESTBLENDALPHA,D3DRS_BLENDOPALPHA,
  D3DRS_STENCILENABLE,D3DRS_SCISSORTESTENABLE,D3DRS_SRGBWRITEENABLE,D3DRS_TEXTUREFACTOR,
  D3DRS_FOGENABLE,D3DRS_FOGCOLOR,D3DRS_LIGHTING,D3DRS_SPECULARENABLE};
constexpr D3DTEXTURESTAGESTATETYPE StageKeys[]={D3DTSS_COLOROP,D3DTSS_COLORARG1,D3DTSS_COLORARG2,D3DTSS_COLORARG0,
  D3DTSS_ALPHAOP,D3DTSS_ALPHAARG1,D3DTSS_ALPHAARG2,D3DTSS_ALPHAARG0,D3DTSS_RESULTARG,D3DTSS_TEXCOORDINDEX,D3DTSS_TEXTURETRANSFORMFLAGS};
constexpr D3DSAMPLERSTATETYPE SamplerKeys[]={D3DSAMP_ADDRESSU,D3DSAMP_ADDRESSV,D3DSAMP_ADDRESSW,
  D3DSAMP_MAGFILTER,D3DSAMP_MINFILTER,D3DSAMP_MIPFILTER,D3DSAMP_MIPMAPLODBIAS,D3DSAMP_MAXMIPLEVEL,
  D3DSAMP_MAXANISOTROPY,D3DSAMP_SRGBTEXTURE,D3DSAMP_BORDERCOLOR};
struct Snapshot {
  std::array<DWORD,std::size(StateKeys)> states{};
  std::array<std::array<DWORD,std::size(StageKeys)>,8> stages{};
  std::array<std::array<DWORD,std::size(SamplerKeys)>,8> samplers{};
  std::array<uintptr_t,8> textures{};
  std::array<DWORD,16> transform{};
  uintptr_t vertexShader=0,pixelShader=0;
  bool operator==(const Snapshot& b)const{return states==b.states&&stages==b.stages&&samplers==b.samplers&&
    textures==b.textures&&transform==b.transform&&vertexShader==b.vertexShader&&pixelShader==b.pixelShader;}
};
static bool ReadSnapshot(IDirect3DDevice9* device,Snapshot& output,unsigned* failure=nullptr){
  Snapshot result;unsigned lastReadFailure=0;
  struct Report {unsigned* out;unsigned& value;~Report(){if(out)*out=value;}} report{failure,lastReadFailure};
  if(!device){lastReadFailure=1;return false;}
  for(size_t i=0;i<std::size(StateKeys);++i)if(FAILED(device->GetRenderState(StateKeys[i],&result.states[i]))){lastReadFailure=100+unsigned(i);return false;}
  for(unsigned stage=0;stage<8;++stage){
    for(size_t i=0;i<std::size(StageKeys);++i)if(FAILED(device->GetTextureStageState(stage,StageKeys[i],&result.stages[stage][i]))){lastReadFailure=200+stage*16+unsigned(i);return false;}
    for(size_t i=0;i<std::size(SamplerKeys);++i)if(FAILED(device->GetSamplerState(stage,SamplerKeys[i],&result.samplers[stage][i]))){lastReadFailure=400+stage*16+unsigned(i);return false;}
    IDirect3DBaseTexture9* texture=nullptr;const auto hr=device->GetTexture(stage,&texture);
    result.textures[stage]=reinterpret_cast<uintptr_t>(texture);if(texture)texture->Release();if(FAILED(hr)){lastReadFailure=600+stage;return false;}
  }
  D3DMATRIX transform{};if(FAILED(device->GetTransform(D3DTS_TEXTURE0,&transform))){lastReadFailure=700;return false;}
  memcpy(result.transform.data(),&transform,sizeof(transform));
  IDirect3DVertexShader9* vs=nullptr;IDirect3DPixelShader9* ps=nullptr;
  const auto vhr=device->GetVertexShader(&vs),phr=device->GetPixelShader(&ps);
  result.vertexShader=reinterpret_cast<uintptr_t>(vs);result.pixelShader=reinterpret_cast<uintptr_t>(ps);
  if(vs)vs->Release();if(ps)ps->Release();if(FAILED(vhr)||FAILED(phr)){lastReadFailure=701;return false;}
  output=result;return true;
}
} // namespace winx_remix::skin_draw_state
