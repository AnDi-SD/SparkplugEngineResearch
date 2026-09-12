// Own FFP -> ray-traced material projection, not recovered game lighting.
// Ambient is a per-material constant input; directional/point/spot light is
// supplied independently by the scene-light adapter. No guessed baked-light removal.
#pragma once
namespace material_channels {
struct RGB {float v[3]{};};
static inline RGB Color(const D3DCOLORVALUE& c) {return {{c.r,c.g,c.b}};}
static inline RGB Color(DWORD c) {return {{float((c>>16)&255)/255.f,float((c>>8)&255)/255.f,float(c&255)/255.f}};}
static inline bool Zero(const RGB& c) {return c.v[0]==0&&c.v[1]==0&&c.v[2]==0;}
static inline bool Unit(const RGB& c) {return c.v[0]==1&&c.v[1]==1&&c.v[2]==1;}
static inline bool Bounded(const RGB& c) {for(float v:c.v)if(!std::isfinite(v)||v<0||v>1)return false;return true;}
struct Input {
  D3DMATERIAL9 material{};
  RGB ambient;
  DWORD diffuseSource=0,ambientSource=0,emissiveSource=0;
};
struct Plan {
  RGB albedo,emission;
  bool vertexRGB=false,vertexAlpha=false;
  uint8_t alpha=255;
};
static inline bool Read(IDirect3DDevice9* d,Input& out) {
  DWORD lighting=0,specular=0,colorVertex=0,ambient=0;
  if(FAILED(d->GetRenderState(D3DRS_LIGHTING,&lighting))||
     FAILED(d->GetRenderState(D3DRS_SPECULARENABLE,&specular))||specular)return false;
  if(!lighting) {
    // Original FFP passes COLOR0 unchanged. Own RT policy uses that authored
    // color as reflectance; do not infer ambient/emission from unused states.
    out={};out.material.Diffuse={1,1,1,1};out.diffuseSource=D3DMCS_COLOR1;return true;
  }
  if(
     FAILED(d->GetRenderState(D3DRS_COLORVERTEX,&colorVertex))||
     FAILED(d->GetRenderState(D3DRS_AMBIENT,&ambient))||FAILED(d->GetMaterial(&out.material))||
     FAILED(d->GetRenderState(D3DRS_DIFFUSEMATERIALSOURCE,&out.diffuseSource))||
     FAILED(d->GetRenderState(D3DRS_AMBIENTMATERIALSOURCE,&out.ambientSource))||
     FAILED(d->GetRenderState(D3DRS_EMISSIVEMATERIALSOURCE,&out.emissiveSource)))return false;
  if(!colorVertex)out.diffuseSource=out.ambientSource=out.emissiveSource=D3DMCS_MATERIAL;
  for(unsigned i=0;i<8;++i) {
    BOOL enabled=FALSE;
    if(SUCCEEDED(d->GetLightEnable(i,&enabled))&&enabled) {
      D3DLIGHT9 light{};
      if(FAILED(d->GetLight(i,&light))||!Zero(Color(light.Ambient)))return false;
    }
  }
  out.ambient=Color(ambient);return true;
}
static inline bool Factor(const Input& input,bool uniform,DWORD vertex,Plan& out) {
  // D(v)=dc+dv*v, E(v)=ec+ev*v. The stock API has ONE vertex multiplier
  // shared by albedo/emission. Accept only an exact common factor, or uniform
  // RGB where it can be moved into separate material textures without loss.
  if(input.diffuseSource>1||input.ambientSource>1||input.emissiveSource>1||
     !Bounded(Color(input.material.Diffuse))||!Bounded(Color(input.material.Ambient))||
     !Bounded(Color(input.material.Emissive))||!Bounded(input.ambient)||
     !std::isfinite(input.material.Diffuse.a)||input.material.Diffuse.a<0||input.material.Diffuse.a>1)return false;
  RGB dc,dv,ec,ev;
  for(unsigned i=0;i<3;++i) {
    if(input.diffuseSource==D3DMCS_COLOR1)dv.v[i]=1;else dc.v[i]=Color(input.material.Diffuse).v[i];
    if(input.emissiveSource==D3DMCS_COLOR1)ev.v[i]=1;else ec.v[i]=Color(input.material.Emissive).v[i];
    if(input.ambientSource==D3DMCS_COLOR1)ev.v[i]+=input.ambient.v[i];
    else ec.v[i]+=input.ambient.v[i]*Color(input.material.Ambient).v[i];
  }
  out={};out.vertexAlpha=input.diffuseSource==D3DMCS_COLOR1;
  out.alpha=static_cast<uint8_t>(std::floor(input.material.Diffuse.a*255.f+.5f));
  if(Zero(dv)&&Zero(ev)){out.albedo=dc;out.emission=ec;}
  else if(Zero(dc)&&Zero(ec)){out.albedo=dv;out.emission=ev;out.vertexRGB=true;}
  else if(uniform) {
    const auto rgb=Color(vertex);
    for(unsigned i=0;i<3;++i){out.albedo.v[i]=dc.v[i]+dv.v[i]*rgb.v[i];out.emission.v[i]=ec.v[i]+ev.v[i]*rgb.v[i];}
  } else return false;
  // This bounded DDS path uses UNORM8. Do not silently clip a HDR coefficient.
  return Bounded(out.albedo)&&Bounded(out.emission);
}
static inline bool Texture(const surface_material::Contract& source,surface_material::Contract& out) {
  // Multiplication distributes over the separate D/E terms. Addition, select,
  // multiple stages and argument modifiers need their own qualified contract.
  if(source.rgb.operation!=3||!((source.rgb.first==1&&source.rgb.second==2)||
      (source.rgb.first==2&&source.rgb.second==1)))return false;
  out=source;
  return true;
}
static inline DWORD Vertex(DWORD original,const Plan& plan) {
  return (plan.vertexRGB?original&0xffffffu:0xffffffu)|
    (plan.vertexAlpha?original&0xff000000u:DWORD(plan.alpha)<<24);
}
static inline DWORD Tint(DWORD original,const RGB& tint) {
  DWORD result=original&0xff000000u;
  for(unsigned i=0;i<3;++i){const unsigned shift=16-8*i;const auto channel=(original>>shift)&255;
    result|=DWORD(std::floor(channel*tint.v[i]+.5f))<<shift;}
  return result;
}
}
