// Own common instance description. State acquisition is outside this helper.
#pragma once
struct SurfaceInstanceState {
  DWORD cull=0,test=0,function=0,reference=0,writeMask=0;
  DWORD blendEnabled=1,source=D3DBLEND_SRCALPHA,destination=D3DBLEND_INVSRCALPHA,operation=D3DBLENDOP_ADD;
  bool channels=false;
};
static bool DescribeSurfaceInstance(const SurfaceInstanceState& state,const surface_material::Contract& contract,
                                    const D3DMATRIX& world,remixapi_MeshHandle mesh,
                                    remixapi_InstanceInfoBlendEXT& blend,remixapi_InstanceInfo& instance) {
  blend={};instance={};
  if(!mesh||state.function<1||state.function>8)return false;
  blend.sType=REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_BLEND_EXT;
  if(!surface_material::AlphaTest(state.test!=0,state.reference,state.function,blend))return false;
  if(state.channels&&state.blendEnabled&&(state.operation!=D3DBLENDOP_ADD||
     !((state.source==D3DBLEND_ONE&&state.destination==D3DBLEND_ZERO)||
       (state.source==D3DBLEND_SRCALPHA&&state.destination==D3DBLEND_INVSRCALPHA))))return false;
  blend.alphaBlendEnabled=state.blendEnabled;
  blend.srcColorBlendFactor=state.source==D3DBLEND_ONE?1:6;blend.dstColorBlendFactor=state.destination==D3DBLEND_ZERO?0:7;
  blend.srcAlphaBlendFactor=blend.srcColorBlendFactor;blend.dstAlphaBlendFactor=blend.dstColorBlendFactor;blend.writeMask=state.writeMask;
  surface_material::Apply(contract,blend);
  instance.sType=REMIXAPI_STRUCT_TYPE_INSTANCE_INFO;instance.pNext=&blend;
  instance.categoryFlags=state.channels?0:REMIXAPI_INSTANCE_CATEGORY_BIT_DECAL_STATIC;
  instance.mesh=mesh;instance.doubleSided=state.cull==D3DCULL_NONE;
  for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c) {
    const float value=world.m[c][r];if(!std::isfinite(value))return false;instance.transform.matrix[r][c]=value;
  }
  return true;
}
