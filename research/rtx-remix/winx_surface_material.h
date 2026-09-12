// Own D3D9 -> Remix material boundary. No game material IDs or lighting inference.
#pragma once
namespace surface_material {
struct Channel { uint32_t operation=0,first=0,second=0; };
struct Contract {
  Channel rgb,alpha;
  DWORD factor=0xffffffff,coordinates=0,transformFlags=0;
  D3DMATRIX transform{};
};
static bool Argument(DWORD input,uint32_t& output) {
  switch(input) {
  case D3DTA_TEXTURE:output=1;return true;
  case D3DTA_DIFFUSE:case D3DTA_CURRENT:output=2;return true; // stage zero only, unlit FFP
  case D3DTA_TFACTOR:output=3;return true;
  default:return false; // Includes complement/alpha replicate, TEMP and specular.
  }
}
static bool Decode(DWORD operation,DWORD first,DWORD second,Channel& output) {
  output={};
  switch(operation) {
  case D3DTOP_SELECTARG1:output.operation=1;return Argument(first,output.first);
  case D3DTOP_SELECTARG2:output.operation=2;return Argument(second,output.second);
  case D3DTOP_MODULATE:output.operation=3;return Argument(first,output.first)&&Argument(second,output.second);
  // Remix RGB MODULATE2X/4X intentionally omit the multiplier; ADD does not
  // saturate RGB. Do not claim those as equivalent by enum conversion alone.
  default:return false;
  }
}
static bool Coordinates(const Contract& contract,const float input[2],float output[2]) {
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
static bool Read(IDirect3DDevice9* d,Contract& out) {
  DWORD rgb=0,r1=0,r2=0,alpha=0,a1=0,a2=0,next=0,lighting=0,specular=0,result=0;
  auto get=[&](D3DTEXTURESTAGESTATETYPE state,DWORD& value){return SUCCEEDED(d->GetTextureStageState(0,state,&value));};
  if(!get(D3DTSS_COLOROP,rgb)||!get(D3DTSS_COLORARG1,r1)||!get(D3DTSS_COLORARG2,r2)||
     !get(D3DTSS_ALPHAOP,alpha)||!get(D3DTSS_ALPHAARG1,a1)||!get(D3DTSS_ALPHAARG2,a2)||
     !get(D3DTSS_RESULTARG,result)||result!=D3DTA_CURRENT||
     !get(D3DTSS_TEXCOORDINDEX,out.coordinates)||!get(D3DTSS_TEXTURETRANSFORMFLAGS,out.transformFlags)||
     FAILED(d->GetTextureStageState(1,D3DTSS_COLOROP,&next))||next!=D3DTOP_DISABLE||
     FAILED(d->GetRenderState(D3DRS_LIGHTING,&lighting))||lighting||
     FAILED(d->GetRenderState(D3DRS_SPECULARENABLE,&specular))||specular||
     FAILED(d->GetRenderState(D3DRS_TEXTUREFACTOR,&out.factor))||out.coordinates>7||
     !Decode(rgb,r1,r2,out.rgb)||!Decode(alpha,a1,a2,out.alpha)) return false;
  if(out.transformFlags==D3DTTFF_DISABLE) return true;
  if(out.transformFlags!=D3DTTFF_COUNT2||FAILED(d->GetTransform(D3DTS_TEXTURE0,&out.transform))) return false;
  for(const auto& row:out.transform.m) for(float f:row) if(!std::isfinite(f)) return false;
  return true;
}
static void Apply(const Contract& contract,remixapi_InstanceInfoBlendEXT& blend) {
  blend.textureColorOperation=contract.rgb.operation;
  blend.textureColorArg1Source=contract.rgb.first;blend.textureColorArg2Source=contract.rgb.second;
  blend.textureAlphaOperation=contract.alpha.operation;
  blend.textureAlphaArg1Source=contract.alpha.first;blend.textureAlphaArg2Source=contract.alpha.second;
  blend.tFactor=contract.factor;
  blend.isTextureFactorBlend=0; // Already represented in the actual equation.
  blend.isVertexColorBakedLighting=0; // Preserve the original RGBA.
}
}
