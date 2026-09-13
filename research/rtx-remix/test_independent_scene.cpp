// Own CPU boundary checks. Literal observed records live in owned storage;
// neither original executable instructions nor GPU rendering are executed.
#define WINX_REMIX_TEST
#include "winx_d3d9_probe.cpp"
#include <stdexcept>
#include <limits>
namespace independent_test {
namespace source=independent_scene_source;
namespace abi=sparkplug::evidence::pc;
static unsigned checks;
static void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
static uint32_t Ptr(const void* pointer){return uint32_t(reinterpret_cast<uintptr_t>(pointer));}
static void Put(void* object,unsigned offset,uint32_t value){memcpy(static_cast<unsigned char*>(object)+offset,&value,4);}
struct Fixture {
  alignas(4) unsigned char renderer[0xf368]{},scene[0x54]{},object[0x10c]{},mesh[0x88]{},fakeTable[0x34]{};
  abi::spRenderNodeLayout node{};
  abi::spModelLayout model{};
  abi::spDXMaterialObservedLayout material{};
  abi::spMaterialPassLayerObservedLayout pass{};
  abi::spStdLayerObservedLayout layer{};
  abi::spMaterialTextureObservedLayout texture{};
  abi::spDXTextureObservedLayout nativeTexture{};
  source::Input input{};
  Fixture() {
    Put(object,0,0x6e65e8);Put(object,0x14,0x6e6604);Put(object,0x84,Ptr(object));Put(object,0x88,Ptr(scene));
    Put(object,0x48,Ptr(object)+0x8c);Put(object,0x4c,Ptr(object)+0xcc);
    D3DMATRIX matrix{};matrix._11=matrix._22=matrix._33=matrix._44=1;matrix._41=12;
    memcpy(object+0x8c,&matrix,64);memcpy(object+0xcc,&matrix,64);
    input.owner.object=Ptr(object);input.owner.support=Ptr(object)+0x14;input.owner.scene=Ptr(scene);input.owner.renderer=Ptr(renderer);
    input.witness.scene=Ptr(scene);input.witness.systemRoot=Ptr(&node);
    model.base.base.base.vtableAddress=0x6eaa58;model.base.material=Ptr(&material);model.baseMeshData=Ptr(mesh);
    material.base.base.vtableAddress=abi::spDXMaterialPrimaryVTable;material.base.materialVTable=abi::spDXMaterialInterfaceVTable;
    material.base.passCount=1;material.base.passes[0]=Ptr(&pass);material.base.renderStates[8]=2;material.base.renderStates[7]=9;
    pass.base.vtableAddress=abi::spMaterialPassLayerVTable;pass.layerCount=1;pass.layers[0]=Ptr(&layer);
    layer.base.base.vtableAddress=abi::spStdLayerVTable;layer.base.materialTexture=Ptr(&texture);
    texture.base.vtableAddress=abi::spMaterialTextureVTable;texture.fallbackTexture=Ptr(&nativeTexture);
    const uint32_t states[9]={0,3,3,0,0,2,2,0,0};memcpy(texture.textureStates,states,sizeof(states));
    nativeTexture.base.base.base.base.vtableAddress=abi::spDXTexturePrimaryVTable;nativeTexture.device=0x123400;nativeTexture.texture=0x234500;
    node.prefix.base.base.base.vtableAddress=Ptr(fakeTable);Put(fakeTable,0x30,0x4250f0);
    node.prefix.base.flags=0x200;node.prefix.base.sceneLink=Ptr(scene);node.prefix.supportVTable=abi::spRenderNodeSupportVTable;
    node.self=Ptr(&node);node.worldMatrixPointer=Ptr(&node)+0x138;node.inverseMatrixPointer=Ptr(&node)+0x178;
    for(unsigned i=0;i<3;++i){node.prefix.base.cachedWorldScale[i]=1;node.inverseWorldScale[i]=1;node.prefix.base.cachedWorldOrientation[i*3+i]=1;}
    node.prefix.base.cachedWorldPosition[0]=31;node.prefix.base.cachedWorldPosition[1]=-7;node.prefix.base.cachedWorldPosition[2]=9;
    node.matrixDirty=1;
  }
  bool Material(uint8_t outer=0){return source::MaterialInput(input,model,nativeTexture.device,outer);}
  bool World(){abi::spRenderSupportObservedLayout support{};Check(source::Read(input.owner.support,support),"read owned support");return source::WorldInput(input,support);}
  void UseNode(){input.owner.object=Ptr(&node);input.owner.support=Ptr(&node)+0xb4;}
};
static void Material() {
  Fixture f;const auto before=f.material;
  Check(f.Material(),"ordinary before-render material qualifies without a draw");
  Check(!memcmp(&before,&f.material,sizeof(before))&&f.input.materialStates[7]==0,"pass blend substitutes local copy only");
  Check(f.input.material.material==Ptr(&f.material)&&f.input.textureCOM==f.nativeTexture.texture,"native identities copied from current objects");
  Check(f.input.material.sampler.u==1&&f.input.material.sampler.v==1&&f.input.material.sampler.mag==2,"ordinary sampler uses shared mapper");
  Check(f.input.mappedPresent[137]&&f.input.mappedStates[137]==0&&f.input.mappedPresent[29]&&f.input.mappedStates[29]==0,
    "shared mode2 mapper supplies disabled lighting and specular");
  Check(f.input.mappedPresent[19]&&f.input.mappedStates[19]==2&&f.input.mappedStates[20]==1,
    "shared finalBlend0 supplies native ONE ZERO factors");
  f.material.base.materialColorController=1;Check(!f.Material(),"color controller rejects even unlit");f.material.base.materialColorController=0;
  f.texture.uvController=1;Check(!f.Material(),"UV clock consumer rejects");f.texture.uvController=0;
  f.texture.animationController=1;Check(!f.Material(),"animated texture consumer rejects");f.texture.animationController=0;
  f.texture.hasStaticUV=1;Check(!f.Material(),"static UV transform remains separate cohort");f.texture.hasStaticUV=0;
  f.material.base.renderStates[8]=1;Check(!f.Material(),"lit path not inferred from colors");f.material.base.renderStates[8]=2;
  f.pass.finalBlendOperation=1;Check(!f.Material(),"alpha queue blend path rejects");f.pass.finalBlendOperation=0;
  f.pass.layerCount=2;Check(!f.Material(),"multipass layer route rejects");f.pass.layerCount=1;
  f.texture.base.vtableAddress=0x6e8444;Check(!f.Material(),"custom derived texture getter rejects");f.texture.base.vtableAddress=abi::spMaterialTextureVTable;
  f.nativeTexture.device=0x333333;Check(!source::MaterialInput(f.input,f.model,0x123400,0),"foreign device texture rejects");f.nativeTexture.device=0x123400;
  f.nativeTexture.texture=0;Check(!f.Material(),"null COM transport rejects");f.nativeTexture.texture=0x234500;
  Put(f.renderer,0xc71c+4*8,2);
  Check(!f.Material(1),"effective override with foreign material selector rejects");
  Check(f.Material(0),"disabled outer override ignores material selectors");
  f.material.base.renderOverride=1;Check(f.Material(1)&&!f.input.effectiveOverride,"material renderOverride derives zero effective flag without Pre");
  Check(f.material.base.renderOverride==1&&f.material.base.renderStates[7]==9,"no native Pre/pass mutation");
  f.material.base.renderOverride=0;Put(f.renderer,0xc71c+4*8,0);
  Check(f.Material(1)&&f.input.effectiveOverride==1,"outer override1 with source0 is supported");
  f.texture.textureStates[7]=1;Check(!f.Material(),"nonzero coordinate source rejects");f.texture.textureStates[7]=0;
  f.model.base.material=0;Check(!f.Material(),"fallback material selection is explicit unsupported boundary");
  abi::spRenderableCallbackVectorLayout callbacks{};
  Check(source::EmptyCallbacks(callbacks),"unallocated empty callback vector");callbacks.begin=0x123400;callbacks.end=callbacks.begin;callbacks.capacityEnd=callbacks.begin+8;
  Check(source::EmptyCallbacks(callbacks),"allocated empty callback vector");callbacks.end+=8;
  Check(!source::EmptyCallbacks(callbacks),"nonempty callbacks excluded even disabled elsewhere");callbacks.capacityEnd=callbacks.begin;
  Check(!source::EmptyCallbacks(callbacks),"invalid callback bounds rejected");
}
static void DrawBootstrap() {
  Fixture f;Check(f.Material()&&f.World(),"draw fixture current material/world inputs");
  source::DrawBootstrap bootstrap{};bootstrap.instance.channels=true;
  Check(!source::DrawInput(bootstrap,f.input),"unqualified bootstrap never creates a draw packet");
  bootstrap.valid=true;bootstrap.instance.cull=D3DCULL_CCW;bootstrap.instance.test=1;bootstrap.factor=0x12345678;
  Check(source::DrawInput(bootstrap,f.input),"current native mapped states join current bootstrap");
  Check(f.input.instance.cull==D3DCULL_NONE&&f.input.instance.source==D3DBLEND_ONE&&f.input.instance.destination==D3DBLEND_ZERO,
    "native cull/factors replace previous bound material values");
  Check(f.input.instance.test==1&&f.input.material.contract.factor==bootstrap.factor,"unmapped alpha enable and factor retain bootstrap origin");
  for(unsigned index:{22u,25u,24u,19u,20u}) {
    f.input.mappedPresent[index]=0;Check(!source::DrawInput(bootstrap,f.input),"missing native mapped input cannot fall back to previous material");
    f.input.mappedPresent[index]=1;
  }
  bootstrap.instance.operation=D3DBLENDOP_SUBTRACT;Check(!source::DrawInput(bootstrap,f.input),"active unsupported blend operation rejects");
  bootstrap.instance.blendEnabled=0;Check(source::DrawInput(bootstrap,f.input),"disabled blending does not consume stale operation");
  f.input.mappedStates[25]=0;Check(!source::DrawInput(bootstrap,f.input),"invalid alpha function rejects");f.input.mappedStates[25]=1;
  f.input.material.contract.rgb.operation=7;Check(!source::DrawInput(bootstrap,f.input),"unfactorable texture multiplication remains unsupported");
}
static void DefaultStage() {
  Fixture f;Put(f.renderer,0xc9c0,Ptr(&f.material));f.pass.layers[1]=Ptr(&f.layer);
  f.texture.textureStates[1]=0;f.texture.fallbackTexture=0;
  source::DefaultStage value{};
  Check(source::ReadDefaultStage(Ptr(f.renderer),value)&&value.operation==D3DTOP_DISABLE,
    "default fixed layer1 disables unused stage through shared mapper");
  Check(f.pass.layerCount==1&&value.layer==Ptr(&f.layer),"native default fixed slot1 is read even when layerCount1");
  Check(source::DefaultStageCurrent(Ptr(f.renderer),value),"fresh default chain has stable pointer/raw provenance");
  f.texture.textureStates[1]=1;Check(!source::ReadDefaultStage(Ptr(f.renderer),value),"non-disabled native default stage rejects");f.texture.textureStates[1]=0;
  Put(f.renderer,0xc770,1);Check(!source::ReadDefaultStage(Ptr(f.renderer),value),"foreign unused-stage selector rejects");Put(f.renderer,0xc770,0);
  f.pass.layers[1]=0;Check(!source::ReadDefaultStage(Ptr(f.renderer),value),"missing fixed default slot has no invented fallback");f.pass.layers[1]=Ptr(&f.layer);
  Check(source::ReadDefaultStage(Ptr(f.renderer),value),"default chain restored");
  auto replacement=f.texture;f.layer.base.materialTexture=Ptr(&replacement);
  Check(!source::DefaultStageCurrent(Ptr(f.renderer),value),"replacement holder invalidates the earlier chain even with equal values");
}
static void Geometry() {
  struct Vertex {float p[3],n[3];DWORD color;float uv[2];};
  const Vertex vertices[]={{{0,0,0},{0,0,1},0x80402010,{0,0}},{{1,0,0},{0,0,1},0x80402010,{1,0}},
                           {{0,1,0},{0,0,1},0x80402010,{0,1}},{{1,1,0},{0,0,1},0x80402010,{1,1}}};
  const uint16_t indices[]={0,1,2,3};
  std::vector<uint8_t> vb(sizeof(vertices)),ib(sizeof(indices));memcpy(vb.data(),vertices,sizeof(vertices));memcpy(ib.data(),indices,sizeof(indices));
  const auto before=vb;
  D3DVERTEXELEMENT9 layout[]={{0,0,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_POSITION,0},
    {0,12,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_NORMAL,0},{0,24,D3DDECLTYPE_D3DCOLOR,0,D3DDECLUSAGE_COLOR,0},
    {0,28,D3DDECLTYPE_FLOAT2,0,D3DDECLUSAGE_TEXCOORD,0},D3DDECL_END()};
  surface_material::Contract contract{};contract.rgb={3,1,2};contract.alpha={3,1,2};
  auto channels=material_channels::UnlitInput(true);
  SurfaceGeometryInput input{&vb,&ib,layout,5,sizeof(Vertex),0,2,{D3DPT_TRIANGLESTRIP,0,0,4,0,2},D3DCULL_CCW,&contract,&channels};
  std::vector<remixapi_HardcodedVertex> expanded;material_channels::Plan plan{};
  Check(ExpandSurfaceGeometry(input,expanded,plan)&&expanded.size()==6,"unbound native bytes expand a two-triangle strip");
  Check(expanded[3].position[0]==0&&expanded[3].position[1]==1&&expanded[4].position[0]==1&&expanded[4].position[1]==0,
    "odd strip triangle keeps native winding");
  Check(expanded[5].texcoord[0]==1&&expanded[5].texcoord[1]==1&&expanded[5].normal[2]==1,"layout supplies normal and UV without D3D bindings");
  Check(expanded[0].color==vertices[0].color&&plan.vertexRGB&&plan.vertexAlpha&&!material_channels::Zero(plan.emission),
    "current unlit preservation policy and alpha pass through the common converter");
  Check(vb==before,"conversion never rewrites source vertices");
  input.cull=D3DCULL_CW;Check(ExpandSurfaceGeometry(input,expanded,plan)&&expanded[0].position[0]==1,"clockwise cull reverses the common triangle order");
  input.range.minimum=1;Check(!ExpandSurfaceGeometry(input,expanded,plan),"index outside declared native minimum rejects");input.range.minimum=0;
  input.range.base=-1;Check(!ExpandSurfaceGeometry(input,expanded,plan),"negative effective vertex rejects");input.range.base=0;
  input.offset=sizeof(Vertex);Check(!ExpandSurfaceGeometry(input,expanded,plan),"stream offset cannot read past current resource bytes");input.offset=0;
  input.indexSize=1;Check(!ExpandSurfaceGeometry(input,expanded,plan),"unsupported index width rejects");input.indexSize=2;
  layout[3].Stream=1;Check(!ExpandSurfaceGeometry(input,expanded,plan),"unsupported second vertex stream rejects");layout[3].Stream=0;
  layout[4].Stream=0;Check(!ExpandSurfaceGeometry(input,expanded,plan),"unterminated declaration rejects");layout[4].Stream=0xff;
  const float invalid=(std::numeric_limits<float>::infinity)();memcpy(vb.data()+12,&invalid,4);
  Check(!ExpandSurfaceGeometry(input,expanded,plan),"nonfinite native normal rejects before API submission");
}
static void World() {
  Fixture f;std::array<unsigned char,sizeof(f.object)> before{};memcpy(before.data(),f.object,sizeof(f.object));
  Check(f.World()&&!f.input.computedWorld&&f.input.owner.world._41==12,"static current matrices copied before drawing");
  Check(!memcmp(before.data(),f.object,sizeof(f.object)),"static reader leaves native storage unchanged");
  f.UseNode();const auto nodeBefore=f.node;
  Check(f.World()&&f.input.computedWorld,"dirty ordinary RenderNode computes its own matrices");
  Check(f.input.owner.world._41==31&&f.input.owner.world._42==-7&&f.input.inverse._41==-31,"shared PRS and inverse preserve current world position");
  Check(!memcmp(&nodeBefore,&f.node,sizeof(f.node))&&f.node.matrixDirty==1,"computed packet does not clear native lazy dirty");
  memcpy(f.node.worldMatrix,&f.input.owner.world,64);memcpy(f.node.inverseWorldMatrix,&f.input.inverse,64);f.node.matrixDirty=0;
  Check(f.World()&&!f.input.computedWorld,"clean RenderNode copies current inline cache");
  f.node.prefix.base.flags|=1;Check(!f.World(),"uncleared Node update dirty rejects");f.node.prefix.base.flags=0x200;
  f.node.prefix.base.flags|=0x100000;Check(!f.World(),"billboard render-time transform excluded");f.node.prefix.base.flags=0x200;
  f.node.prefix.base.flags=0;Check(!f.World(),"disabled logical owner excluded");f.node.prefix.base.flags=0x200;
  f.node.callbackBegin=0x123400;f.node.callbackEnd=0x123404;f.node.callbackCapacity=0x123404;
  Check(f.World(),"nonempty reciprocal partition listeners are ordinary owner membership, not render callbacks");
  f.node.callbackCapacity=f.node.callbackBegin;Check(!f.World(),"malformed reciprocal listener bounds rejected");
  f.node.callbackBegin=f.node.callbackEnd=f.node.callbackCapacity=0;
  f.node.worldMatrixPointer=Ptr(&f.node)+0x178;Check(!f.World(),"custom world pointer excluded");f.node.worldMatrixPointer=Ptr(&f.node)+0x138;
  f.node.prefix.base.cachedWorldScale[0]=0;Check(!f.World(),"zero reciprocal scale cohort rejected");f.node.prefix.base.cachedWorldScale[0]=1;
  f.node.inverseWorldScale[0]=(std::numeric_limits<float>::infinity)();Check(!f.World(),"nonfinite reciprocal rejected");f.node.inverseWorldScale[0]=1;
  Put(f.fakeTable,0x30,0x112233);Check(!f.World(),"unknown descendant world override rejected");Put(f.fakeTable,0x30,0x4250f0);
  f.input.witness.systemRoot=0;f.node.prefix.base.parent=Ptr(&f.node);Check(!f.World(),"parent cycle or missing actual membership rejected");
  D3DMATRIX a{},b{};a._11=b._11=1;float error=0;
  Check(source::MatrixMatches(a,b,false,error),"clean exact matrix match");b._11=1.0000002f;
  Check(!source::MatrixMatches(a,b,false,error)&&source::MatrixMatches(a,b,true,error),"only computed analytical path permits explicit numerical tolerance");
  b._11=1.01f;Check(!source::MatrixMatches(a,b,true,error),"material transform difference cannot pass tolerance");
  b._11=(std::numeric_limits<float>::quiet_NaN)();Check(!source::MatrixMatches(a,b,true,error),"nonfinite actual matrix cannot pass comparison");
}
}
int main() {
  try {independent_test::Material();independent_test::World();independent_test::DrawBootstrap();independent_test::Geometry();independent_test::DefaultStage();
    printf("{\"status\":\"PASS\",\"checks\":%u,\"gpu\":false,\"nativeGameCodeExecuted\":false}\n",independent_test::checks);return 0;
  }catch(const std::exception& error){fprintf(stderr,"FAIL %s\n",error.what());return 1;}
}
