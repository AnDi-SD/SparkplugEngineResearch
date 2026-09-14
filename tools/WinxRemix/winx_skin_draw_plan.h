#pragma once
// Own preparation for an independently proven Fixed ColorMode4 draw. No native
// shader recognition, constant-register inference, COM ownership or submission.
// Include after the shared Skin packet/resource adapter and sampler declaration.
#include "winx_skin_draw_state.h"
namespace winx_remix::skin_draw {
enum class Error {None,Shader,Depth,Blend,Alpha,Cull,Write,Effects,Texture,Stage,Sampler,Projection};
struct State {
  SurfaceInstanceState instance{};
  surface_material::Contract texture{};
  native_material_source::Sampler sampler{};
  DWORD srgb=0;
};
struct Color4 {skin_packet_submit::Color4Prepared packet;State state;};
inline bool Fail(Error* error,Error value){if(error)*error=value;return false;}
template<class K,size_t N> inline DWORD Value(const K (&keys)[N],const std::array<DWORD,N>& values,K key) {
  for(size_t i=0;i<N;++i)if(keys[i]==key)return values[i];
  return 0xffffffff; // Missing state cannot silently become an enabled default.
}
inline bool Describe(const skin_draw_state::Snapshot& input,State& output,Error* error=nullptr) {
  using namespace skin_draw_state;if(error)*error=Error::None;
  const auto rs=[&](D3DRENDERSTATETYPE key){return Value(StateKeys,input.states,key);};
  const auto stage=[&](unsigned n,D3DTEXTURESTAGESTATETYPE key){return Value(StageKeys,input.stages[n],key);};
  const auto sampler=[&](D3DSAMPLERSTATETYPE key){return Value(SamplerKeys,input.samplers[0],key);};
  // Presence is a necessary condition only. Neither pointer nor shader key
  // establishes ColorMode4 or the meaning of the caller's diffuse constant.
  if(!input.vertexShader||input.pixelShader)return Fail(error,Error::Shader);
  if(rs(D3DRS_ZENABLE)!=D3DZB_TRUE||rs(D3DRS_ZWRITEENABLE)!=TRUE||rs(D3DRS_ZFUNC)!=D3DCMP_LESSEQUAL)
    return Fail(error,Error::Depth);
  if(rs(D3DRS_ALPHABLENDENABLE)!=TRUE||rs(D3DRS_SRCBLEND)!=D3DBLEND_ONE||rs(D3DRS_DESTBLEND)!=D3DBLEND_ZERO||
     rs(D3DRS_BLENDOP)!=D3DBLENDOP_ADD||rs(D3DRS_SEPARATEALPHABLENDENABLE))return Fail(error,Error::Blend);
  if(rs(D3DRS_ALPHATESTENABLE)>1||rs(D3DRS_ALPHAFUNC)<D3DCMP_NEVER||rs(D3DRS_ALPHAFUNC)>D3DCMP_ALWAYS||
     rs(D3DRS_ALPHAREF)>255)return Fail(error,Error::Alpha);
  if(rs(D3DRS_CULLMODE)<D3DCULL_NONE||rs(D3DRS_CULLMODE)>D3DCULL_CCW)return Fail(error,Error::Cull);
  if(rs(D3DRS_COLORWRITEENABLE)!=15)return Fail(error,Error::Write);
  if(rs(D3DRS_STENCILENABLE)||rs(D3DRS_SCISSORTESTENABLE)||rs(D3DRS_SRGBWRITEENABLE)||
     rs(D3DRS_FOGENABLE)||rs(D3DRS_SPECULARENABLE))return Fail(error,Error::Effects);
  if(!input.textures[0])return Fail(error,Error::Texture);
  for(unsigned n=1;n<8;++n) {
    if(input.textures[n])return Fail(error,Error::Texture);
    if(stage(n,D3DTSS_COLOROP)!=D3DTOP_DISABLE)return Fail(error,Error::Stage);
  }
  if(stage(0,D3DTSS_COLOROP)!=D3DTOP_MODULATE||stage(0,D3DTSS_COLORARG1)!=D3DTA_TEXTURE||
     stage(0,D3DTSS_COLORARG2)!=D3DTA_CURRENT||stage(0,D3DTSS_ALPHAOP)!=D3DTOP_MODULATE||
     stage(0,D3DTSS_ALPHAARG1)!=D3DTA_TEXTURE||stage(0,D3DTSS_ALPHAARG2)!=D3DTA_CURRENT||
     stage(0,D3DTSS_RESULTARG)!=D3DTA_CURRENT||stage(0,D3DTSS_TEXCOORDINDEX)!=0||
     stage(0,D3DTSS_TEXTURETRANSFORMFLAGS)!=D3DTTFF_DISABLE)return Fail(error,Error::Stage);
  // Initial policy has only the already exercised repeat/linear mip contract.
  // Do not silently collapse anisotropy, LOD bias or mixed min/mag/mip filters.
  if(sampler(D3DSAMP_ADDRESSU)!=D3DTADDRESS_WRAP||sampler(D3DSAMP_ADDRESSV)!=D3DTADDRESS_WRAP||
     sampler(D3DSAMP_ADDRESSW)!=D3DTADDRESS_WRAP||sampler(D3DSAMP_MAGFILTER)!=D3DTEXF_LINEAR||
     sampler(D3DSAMP_MINFILTER)!=D3DTEXF_LINEAR||sampler(D3DSAMP_MIPFILTER)!=D3DTEXF_LINEAR||
     sampler(D3DSAMP_MIPMAPLODBIAS)||sampler(D3DSAMP_MAXMIPLEVEL)||sampler(D3DSAMP_MAXANISOTROPY)!=1||
     sampler(D3DSAMP_SRGBTEXTURE))return Fail(error,Error::Sampler);
  State result;
  result.instance={rs(D3DRS_CULLMODE),rs(D3DRS_ALPHATESTENABLE),rs(D3DRS_ALPHAFUNC),rs(D3DRS_ALPHAREF),
    rs(D3DRS_COLORWRITEENABLE),rs(D3DRS_ALPHABLENDENABLE),rs(D3DRS_SRCBLEND),rs(D3DRS_DESTBLEND),rs(D3DRS_BLENDOP),true};
  // Reuse the same texture equation decoder as other Surface consumers.
  if(!surface_material::Decode(stage(0,D3DTSS_COLOROP),stage(0,D3DTSS_COLORARG1),stage(0,D3DTSS_COLORARG2),result.texture.rgb)||
     !surface_material::DecodeAlpha(stage(0,D3DTSS_ALPHAOP),stage(0,D3DTSS_ALPHAARG1),stage(0,D3DTSS_ALPHAARG2),result.texture.alpha))
    return Fail(error,Error::Stage);
  result.texture.factor=rs(D3DRS_TEXTUREFACTOR);
  result.sampler={sampler(D3DSAMP_ADDRESSU),sampler(D3DSAMP_ADDRESSV),sampler(D3DSAMP_MAGFILTER),
    sampler(D3DSAMP_MINFILTER),sampler(D3DSAMP_MIPFILTER)};
  // D3DRS_LIGHTING and disabled FFP transform/fog/separate-alpha inputs do not
  // add lighting to a proven vertex shader. Their values are not RT light data.
  output=result;return true;
}
inline bool PrepareColor4(const skin_packet::Packet& source,const skin_packet::pc::FixedSkinVector& diffuse,
    const skin_draw_state::Snapshot& state,Color4& output,Error* error=nullptr) {
  Color4 result;if(!Describe(state,result.state,error))return false;
  if(!skin_packet_submit::PrepareColor4(source,diffuse,result.state.instance.cull,result.packet))return Fail(error,Error::Projection);
  output=std::move(result);return true;
}
} // namespace winx_remix::skin_draw
