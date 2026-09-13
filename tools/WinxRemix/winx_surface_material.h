// Own D3D9 -> Remix material boundary. No game material IDs or lighting inference.
#pragma once
namespace surface_material {
struct Channel { uint32_t operation=0,first=0,second=0; };
struct Contract {
  Channel rgb,alpha;
  DWORD factor=0xffffffff,coordinates=0,transformFlags=0;
  D3DMATRIX transform{};
};
// Transient observation of the stage states already read for the contract.
// Kept separate from the material recipe and any native source claim.
struct ObservedStage {
  DWORD rgb=0,r1=0,r2=0,alpha=0,a1=0,a2=0,next=0,result=0;
};
static inline bool AlphaTest(bool enabled,DWORD reference,DWORD function,remixapi_InstanceInfoBlendEXT& blend) {
  // Stock Remix derives the effective test from compareOp even when the API
  // enabled flag is false. D3D9's disabled test must therefore be ALWAYS.
  if(enabled && (function<D3DCMP_NEVER || function>D3DCMP_ALWAYS))return false;
  blend.alphaTestEnabled=enabled;
  blend.alphaTestCompareOp=enabled?function-1:7;
  blend.alphaTestReferenceValue=enabled?static_cast<uint8_t>(reference):0;
  return true;
}
static inline bool Argument(DWORD input,uint32_t& output) {
  switch(input) {
  case D3DTA_TEXTURE:output=1;return true;
  case D3DTA_DIFFUSE:case D3DTA_CURRENT:output=2;return true; // stage zero only, unlit FFP
  case D3DTA_TFACTOR:output=3;return true;
  default:return false; // Includes complement/alpha replicate, TEMP and specular.
  }
}
static inline bool Decode(DWORD operation,DWORD first,DWORD second,Channel& output) {
  output={};
  switch(operation) {
  case D3DTOP_SELECTARG1:output.operation=1;return Argument(first,output.first);
  case D3DTOP_SELECTARG2:output.operation=2;return Argument(second,output.second);
  case D3DTOP_MODULATE:output.operation=3;return Argument(first,output.first)&&Argument(second,output.second);
  case D3DTOP_MODULATE2X:output.operation=7;return Argument(first,output.first)&&Argument(second,output.second);
  // Force_Modulate2x preserves the factor and saturation; ordinary Remix
  // Modulate2x/4x omit the factor. RGB ADD does not saturate and stays unsupported.
  default:return false;
  }
}
static inline bool DecodeAlpha(DWORD operation,DWORD first,DWORD second,Channel& output) {
  if(operation==D3DTOP_MODULATE2X || operation==D3DTOP_MODULATE4X || operation==D3DTOP_ADD) {
    output={operation==D3DTOP_MODULATE2X?4u:operation==D3DTOP_MODULATE4X?5u:6u,0,0};
    return Argument(first,output.first)&&Argument(second,output.second);
  }
  return Decode(operation,first,second,output);
}
static inline bool Coordinates(const Contract& contract,const float input[2],float output[2]) {
  if(!std::isfinite(input[0]) || !std::isfinite(input[1])) return false;
  if(contract.transformFlags==D3DTTFF_DISABLE) {output[0]=input[0];output[1]=input[1];return true;}
  if(contract.transformFlags!=D3DTTFF_COUNT2) return false;
  // Native FFP pads a FLOAT2 texture coordinate to (u,v,1,0) for COUNT2.
  // Translation therefore comes from row 3, not the world-transform row 4.
  for(unsigned c=0;c<2;++c) {
    output[c]=input[0]*contract.transform.m[0][c]+input[1]*contract.transform.m[1][c]+contract.transform.m[2][c];
    if(!std::isfinite(output[c])) return false;
  }
  return true;
}
static inline bool ReadTexture(IDirect3DDevice9* d,Contract& out,ObservedStage* observation=nullptr) {
  ObservedStage values{};
  auto& rgb=values.rgb;auto& r1=values.r1;auto& r2=values.r2;
  auto& alpha=values.alpha;auto& a1=values.a1;auto& a2=values.a2;
  auto& next=values.next;auto& result=values.result;
  auto get=[&](D3DTEXTURESTAGESTATETYPE state,DWORD& value){return SUCCEEDED(d->GetTextureStageState(0,state,&value));};
  if(!get(D3DTSS_COLOROP,rgb)||!get(D3DTSS_COLORARG1,r1)||!get(D3DTSS_COLORARG2,r2)||
     !get(D3DTSS_ALPHAOP,alpha)||!get(D3DTSS_ALPHAARG1,a1)||!get(D3DTSS_ALPHAARG2,a2)||
     !get(D3DTSS_RESULTARG,result)||result!=D3DTA_CURRENT||
     !get(D3DTSS_TEXCOORDINDEX,out.coordinates)||!get(D3DTSS_TEXTURETRANSFORMFLAGS,out.transformFlags)||
     FAILED(d->GetTextureStageState(1,D3DTSS_COLOROP,&next))||next!=D3DTOP_DISABLE||
     FAILED(d->GetRenderState(D3DRS_TEXTUREFACTOR,&out.factor))||out.coordinates>7||
     !Decode(rgb,r1,r2,out.rgb)||!DecodeAlpha(alpha,a1,a2,out.alpha)) return false;
  if(observation)*observation=values;
  if(out.transformFlags==D3DTTFF_DISABLE) return true;
  if(out.transformFlags!=D3DTTFF_COUNT2||FAILED(d->GetTransform(D3DTS_TEXTURE0,&out.transform))) return false;
  for(const auto& row:out.transform.m) for(float f:row) if(!std::isfinite(f)) return false;
  return true;
}
static inline bool Read(IDirect3DDevice9* d,Contract& out,ObservedStage* observation=nullptr) {
  DWORD lighting=0,specular=0;
  return SUCCEEDED(d->GetRenderState(D3DRS_LIGHTING,&lighting))&&!lighting&&
    SUCCEEDED(d->GetRenderState(D3DRS_SPECULARENABLE,&specular))&&!specular&&ReadTexture(d,out,observation);
}
static inline void Apply(const Contract& contract,remixapi_InstanceInfoBlendEXT& blend) {
  blend.textureColorOperation=contract.rgb.operation;
  blend.textureColorArg1Source=contract.rgb.first;blend.textureColorArg2Source=contract.rgb.second;
  blend.textureAlphaOperation=contract.alpha.operation;
  blend.textureAlphaArg1Source=contract.alpha.first;blend.textureAlphaArg2Source=contract.alpha.second;
  blend.tFactor=contract.factor;
  blend.isTextureFactorBlend=0; // Already represented in the actual equation.
  blend.isVertexColorBakedLighting=0; // Preserve the original RGBA.
}
}
