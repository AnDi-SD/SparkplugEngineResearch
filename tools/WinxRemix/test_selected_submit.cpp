// Own selected-draw integration: reuse the existing owned CPU ABI/COM fixture.
// No original instructions, system D3D device, bridge or GPU are executed.
#define WINX_INDEPENDENT_SUBMIT_FIXTURE_ONLY
#include "test_independent_submit.cpp"

namespace selected_test {
using namespace direct_test;
namespace state=d3d9_state_witness;
struct Fixture;
static Fixture* selected;
static unsigned setters;
static bool failSetter;
static void (*afterReferenceRelease)();
static std::vector<remixapi_HardcodedVertex> meshVertices;
static remixapi_MaterialInfoOpaqueEXT materialOpaque{};
static uint8_t materialWrapU,materialWrapV,materialFilter;
static float materialEmission;
struct FakeBlock {void** table=nullptr;ULONG refs=1;};
static unsigned blockApplies;
static void (*onBlockRelease)();
static HRESULT STDMETHODCALLTYPE CreateBlock(IDirect3DDevice9*,D3DSTATEBLOCKTYPE,IDirect3DStateBlock9**);
static HRESULT STDMETHODCALLTYPE EndBlock(IDirect3DDevice9*,IDirect3DStateBlock9**);
static HRESULT STDMETHODCALLTYPE QueryBlock(FakeBlock*,REFIID,void**);
static ULONG STDMETHODCALLTYPE ReleaseBlock(FakeBlock*);
static HRESULT STDMETHODCALLTYPE ApplyBlock(FakeBlock*);
static HRESULT STDMETHODCALLTYPE SetRender(IDirect3DDevice9*,D3DRENDERSTATETYPE,DWORD);
static HRESULT STDMETHODCALLTYPE SetStream(IDirect3DDevice9*,UINT,IDirect3DVertexBuffer9*,UINT,UINT);
static HRESULT STDMETHODCALLTYPE SetIndices(IDirect3DDevice9*,IDirect3DIndexBuffer9*);
static HRESULT STDMETHODCALLTYPE SetTexture(IDirect3DDevice9*,DWORD,IDirect3DBaseTexture9*);
static HRESULT STDMETHODCALLTYPE SetDeclaration(IDirect3DDevice9*,IDirect3DVertexDeclaration9*);
static HRESULT STDMETHODCALLTYPE SetTransform(IDirect3DDevice9*,D3DTRANSFORMSTATETYPE,const D3DMATRIX*);
static HRESULT STDMETHODCALLTYPE SetStage(IDirect3DDevice9*,DWORD,D3DTEXTURESTAGESTATETYPE,DWORD);
static HRESULT STDMETHODCALLTYPE SetSampler(IDirect3DDevice9*,DWORD,D3DSAMPLERSTATETYPE,DWORD);
static HRESULT STDMETHODCALLTYPE GetStream(IDirect3DDevice9*,UINT,IDirect3DVertexBuffer9**,UINT*,UINT*);
static HRESULT STDMETHODCALLTYPE GetIndices(IDirect3DDevice9*,IDirect3DIndexBuffer9**);
static HRESULT STDMETHODCALLTYPE GetDeclaration(IDirect3DDevice9*,IDirect3DVertexDeclaration9**);
static HRESULT STDMETHODCALLTYPE GetElements(IDirect3DVertexDeclaration9*,D3DVERTEXELEMENT9*,UINT*);
static HRESULT STDMETHODCALLTYPE GetTexture(IDirect3DDevice9*,DWORD,IDirect3DBaseTexture9**);
static HRESULT STDMETHODCALLTYPE GetTransform(IDirect3DDevice9*,D3DTRANSFORMSTATETYPE,D3DMATRIX*);
static HRESULT STDMETHODCALLTYPE GetSampler(IDirect3DDevice9*,DWORD,D3DSAMPLERSTATETYPE,DWORD*);
static ULONG STDMETHODCALLTYPE ReleaseBuffer(void*);
static ULONG STDMETHODCALLTYPE ReleaseTexture(IDirect3DBaseTexture9*);
template<class... A> static HRESULT STDMETHODCALLTYPE UnusedSet(IDirect3DDevice9*,A...){++setters;return failSetter?D3DERR_INVALIDCALL:D3D_OK;}
static ULONG STDMETHODCALLTYPE DeviceRelease(IDirect3DDevice9*){return 1;}
static HRESULT STDMETHODCALLTYPE DeviceReset(IDirect3DDevice9*,D3DPRESENT_PARAMETERS*){return D3D_OK;}
static HRESULT STDMETHODCALLTYPE GetVS(IDirect3DDevice9*,IDirect3DVertexShader9** p){++comCalls;*p=nullptr;return D3D_OK;}
static HRESULT STDMETHODCALLTYPE GetPS(IDirect3DDevice9*,IDirect3DPixelShader9** p){++comCalls;*p=nullptr;return D3D_OK;}
static remixapi_ErrorCode REMIXAPI_CALL CreateMaterial(const remixapi_MaterialInfo* info,remixapi_MaterialHandle* out) {
  Check(info&&info->pNext&&info->sType==REMIXAPI_STRUCT_TYPE_MATERIAL_INFO,"selected material exposes complete synchronous recipe");
  materialOpaque=*static_cast<const remixapi_MaterialInfoOpaqueEXT*>(info->pNext);
  materialWrapU=info->wrapModeU;materialWrapV=info->wrapModeV;materialFilter=info->filterMode;materialEmission=info->emissiveIntensity;
  return direct_test::CreateMaterial(info,out);
}
static remixapi_ErrorCode REMIXAPI_CALL CreateMesh(const remixapi_MeshInfo* info,remixapi_MeshHandle* out) {
  meshVertices.assign(info->surfaces_values[0].vertices_values,info->surfaces_values[0].vertices_values+info->surfaces_values[0].vertices_count);
  return direct_test::CreateMesh(info,out);
}
struct Fixture : direct_test::Fixture {
  void* bufferTable[14]{};void* declTable[5]{};void* blockTable[6]{};FakeBlock block{};
  ULONG refs[3]={1,1,1},textureRefs=1;
  IDirect3DVertexBuffer9* boundVB=nullptr;IDirect3DIndexBuffer9* boundIB=nullptr;
  IDirect3DVertexDeclaration9* boundDecl=nullptr;IDirect3DBaseTexture9* boundTexture=nullptr;
  UINT streamOffset=0,streamStride=36;
  D3DMATRIX deviceWorld{};std::array<DWORD,14> sampler{};
  Fixture() {
    selected=this;setters=blockApplies=0;failSetter=false;afterReferenceRelease=nullptr;onBlockRelease=nullptr;meshVertices.clear();
    source::selectedSubmitEnabled=true;source::keepSelectedForComparison=source::selectedSubmissionFailed=false;
    source::selectedAttempts=source::selectedCalls=source::selectedInstances=source::selectedRejected=source::selectedApiFailures=source::selectedReentries=source::selectedPostCommitFaults=0;
    source::selectedRunning=false;native_mesh_source::submitEnabled=native_material_source::submitEnabled=true;
    bufferTable[2]=reinterpret_cast<void*>(ReleaseBuffer);declTable[2]=reinterpret_cast<void*>(ReleaseBuffer);declTable[4]=reinterpret_cast<void*>(GetElements);
    bufferTokens[0]=bufferTokens[1]=Ptr(bufferTable);bufferTokens[2]=Ptr(declTable);
    boundVB=reinterpret_cast<IDirect3DVertexBuffer9*>(&bufferTokens[0]);boundIB=reinterpret_cast<IDirect3DIndexBuffer9*>(&bufferTokens[1]);
    boundDecl=reinterpret_cast<IDirect3DVertexDeclaration9*>(&bufferTokens[2]);boundTexture=Texture();deviceWorld=world;
    textureTable[2]=reinterpret_cast<void*>(ReleaseTexture);
    deviceTable[2]=reinterpret_cast<void*>(DeviceRelease);deviceTable[16]=reinterpret_cast<void*>(DeviceReset);
    deviceTable[37]=reinterpret_cast<void*>(UnusedSet<DWORD,IDirect3DSurface9*>);
    deviceTable[39]=reinterpret_cast<void*>(UnusedSet<IDirect3DSurface9*>);
    deviceTable[44]=reinterpret_cast<void*>(SetTransform);deviceTable[45]=reinterpret_cast<void*>(GetTransform);
    deviceTable[46]=reinterpret_cast<void*>(UnusedSet<D3DTRANSFORMSTATETYPE,const D3DMATRIX*>);
    deviceTable[47]=reinterpret_cast<void*>(UnusedSet<const D3DVIEWPORT9*>);deviceTable[49]=reinterpret_cast<void*>(UnusedSet<const D3DMATERIAL9*>);
    deviceTable[57]=reinterpret_cast<void*>(SetRender);deviceTable[59]=reinterpret_cast<void*>(CreateBlock);
    deviceTable[60]=reinterpret_cast<void*>(UnusedSet<>);deviceTable[61]=reinterpret_cast<void*>(EndBlock);
    deviceTable[64]=reinterpret_cast<void*>(GetTexture);deviceTable[65]=reinterpret_cast<void*>(SetTexture);
    deviceTable[67]=reinterpret_cast<void*>(SetStage);deviceTable[68]=reinterpret_cast<void*>(GetSampler);deviceTable[69]=reinterpret_cast<void*>(SetSampler);
    deviceTable[75]=reinterpret_cast<void*>(UnusedSet<const RECT*>);
    deviceTable[87]=reinterpret_cast<void*>(SetDeclaration);deviceTable[88]=reinterpret_cast<void*>(GetDeclaration);deviceTable[89]=reinterpret_cast<void*>(UnusedSet<DWORD>);
    deviceTable[92]=reinterpret_cast<void*>(UnusedSet<IDirect3DVertexShader9*>);deviceTable[93]=reinterpret_cast<void*>(GetVS);
    deviceTable[100]=reinterpret_cast<void*>(SetStream);deviceTable[101]=reinterpret_cast<void*>(GetStream);
    deviceTable[102]=reinterpret_cast<void*>(UnusedSet<UINT,UINT>);deviceTable[104]=reinterpret_cast<void*>(SetIndices);deviceTable[105]=reinterpret_cast<void*>(GetIndices);
    deviceTable[107]=reinterpret_cast<void*>(UnusedSet<IDirect3DPixelShader9*>);deviceTable[108]=reinterpret_cast<void*>(GetPS);
    block.table=blockTable;blockTable[0]=reinterpret_cast<void*>(QueryBlock);blockTable[2]=reinterpret_cast<void*>(ReleaseBlock);blockTable[5]=reinterpret_cast<void*>(ApplyBlock);
    originals.erase(deviceTable);state::Retire(Device());
    Check(state::Install(Device(),deviceTable[16],deviceTable[2]),"owned actual setter vtable is patched and covered");
    api.CreateMaterial=CreateMaterial;api.CreateMesh=CreateMesh;
  }
  ~Fixture() {
    afterReferenceRelease=nullptr;onBlockRelease=nullptr;
    Check(refs[0]==1&&refs[1]==1&&refs[2]==1&&textureRefs==1&&target.refs==1,"all temporary selected COM references balanced");
    Check(source::selectedAttempts==source::selectedRejected+source::selectedCalls,"selected attempts partition into precommit rejection and API call");
    Check(source::selectedCalls==source::selectedInstances+source::selectedApiFailures,"postcommit faults never double-count successful API calls as errors");
    state::Retire(Device());originals.erase(deviceTable);originals.erase(bufferTable);originals.erase(declTable);originals.erase(blockTable);
    source::selectedSubmitEnabled=false;native_mesh_source::submitEnabled=native_material_source::submitEnabled=false;selected=nullptr;
  }
  source::Input& Input(unsigned model=0){return source::inputs.at({Support(),Ptr(&models[model])});}
  void Prepare(scene_geometry::Scope& scope) {
    direct_test::Fixture::Inputs(scope,true);originalSelection[0]=Support();Vector(selection,0x28,originalSelection,1,1);
    auto& input=Input();
    auto& context=*reinterpret_cast<abi::spRendererDrawContextObservedLayout*>(renderer+abi::spRendererDrawContextOffset);
    context.selectedMaterial=Ptr(&material);memcpy(context.renderStates,input.materialStates,sizeof(input.materialStates));memcpy(context.textureStates,input.material.raw,sizeof(input.material.raw));
    reinterpret_cast<abi::spDXRendererMaterialCacheObservedLayout*>(renderer+abi::spDXRendererMaterialCacheOffset)->installedMaterial=Ptr(&material);
    for(unsigned i=0;i<input.mappedPresent.size();++i)if(input.mappedPresent[i])states[i]=input.mappedStates[i];
    native_material_source::Cache cache{};native_material_source::Mapped mapped;
    for(unsigned i=1;i<9;++i)Check(sparkplug::reconstruction::ApplyPCTextureStateForAnalysis(cache,0,i,texture.textureStates[i],false,native_material_source::Map,&mapped),"shared native texture mapper seeds test device state");
    for(unsigned i=0;i<mapped.stage.size();++i)if(mapped.stage[i]!=~0u)stages[i]=mapped.stage[i];
    for(unsigned i=0;i<mapped.sampler.size();++i)if(mapped.sampler[i]!=~0u)sampler[i]=mapped.sampler[i];
  }
};
static HRESULT STDMETHODCALLTYPE CreateBlock(IDirect3DDevice9*,D3DSTATEBLOCKTYPE,IDirect3DStateBlock9** p){*p=reinterpret_cast<IDirect3DStateBlock9*>(&selected->block);return D3D_OK;}
static HRESULT STDMETHODCALLTYPE EndBlock(IDirect3DDevice9* d,IDirect3DStateBlock9** p){return CreateBlock(d,D3DSBT_ALL,p);}
static HRESULT STDMETHODCALLTYPE QueryBlock(FakeBlock* b,REFIID,void** p){*p=b;++b->refs;return S_OK;}
static ULONG STDMETHODCALLTYPE ReleaseBlock(FakeBlock* b){const auto count=--b->refs;if(onBlockRelease)onBlockRelease();return count;}
static HRESULT STDMETHODCALLTYPE ApplyBlock(FakeBlock*){++blockApplies;selected->states[D3DRS_LIGHTING]=1;return D3D_OK;}
static ULONG STDMETHODCALLTYPE ReleaseBuffer(void* p){++comCalls;for(unsigned i=0;i<3;++i)if(p==&selected->bufferTokens[i]){Check(selected->refs[i]>1,"borrowed buffer release owns temporary ref");const auto refs=--selected->refs[i];if(afterReferenceRelease)afterReferenceRelease();return refs;}Check(false,"unknown fake resource release");return 0;}
static ULONG STDMETHODCALLTYPE ReleaseTexture(IDirect3DBaseTexture9* p){++comCalls;Check(p==selected->Texture()&&selected->textureRefs>1,"borrowed texture release owns temporary ref");const auto refs=--selected->textureRefs;if(afterReferenceRelease)afterReferenceRelease();return refs;}
static HRESULT STDMETHODCALLTYPE GetStream(IDirect3DDevice9*,UINT stream,IDirect3DVertexBuffer9** p,UINT* offset,UINT* stride){++comCalls;Check(!stream,"only stream0");*p=selected->boundVB;*offset=selected->streamOffset;*stride=selected->streamStride;if(*p)++selected->refs[0];return D3D_OK;}
static HRESULT STDMETHODCALLTYPE GetIndices(IDirect3DDevice9*,IDirect3DIndexBuffer9** p){++comCalls;*p=selected->boundIB;if(*p)++selected->refs[1];return D3D_OK;}
static HRESULT STDMETHODCALLTYPE GetDeclaration(IDirect3DDevice9*,IDirect3DVertexDeclaration9** p){++comCalls;*p=selected->boundDecl;if(*p)++selected->refs[2];return D3D_OK;}
static HRESULT STDMETHODCALLTYPE GetElements(IDirect3DVertexDeclaration9*,D3DVERTEXELEMENT9* p,UINT* n){++comCalls;if(!n||*n<5)return D3DERR_INVALIDCALL;*n=5;memcpy(p,selected->layout,sizeof(selected->layout));return D3D_OK;}
static HRESULT STDMETHODCALLTYPE GetTexture(IDirect3DDevice9*,DWORD stage,IDirect3DBaseTexture9** p){++comCalls;Check(!stage,"only source texture0");*p=selected->boundTexture;if(*p)++selected->textureRefs;return D3D_OK;}
static HRESULT STDMETHODCALLTYPE GetTransform(IDirect3DDevice9* d,D3DTRANSFORMSTATETYPE t,D3DMATRIX* out){if(t==D3DTS_WORLD){++comCalls;*out=selected->deviceWorld;return D3D_OK;}return direct_test::GetTransform(reinterpret_cast<FakeDevice*>(d),t,out);}
static HRESULT STDMETHODCALLTYPE GetSampler(IDirect3DDevice9*,DWORD stage,D3DSAMPLERSTATETYPE t,DWORD* out){++comCalls;if(stage||unsigned(t)>=selected->sampler.size())return D3DERR_INVALIDCALL;*out=selected->sampler[t];return D3D_OK;}
static HRESULT STDMETHODCALLTYPE SetRender(IDirect3DDevice9*,D3DRENDERSTATETYPE t,DWORD v){++setters;if(failSetter)return D3DERR_INVALIDCALL;selected->states[t]=v;return D3D_OK;}
static HRESULT STDMETHODCALLTYPE SetStream(IDirect3DDevice9*,UINT stream,IDirect3DVertexBuffer9* p,UINT offset,UINT stride){++setters;Check(!stream,"owned setter stream0");if(failSetter)return D3DERR_INVALIDCALL;selected->boundVB=p;selected->streamOffset=offset;selected->streamStride=stride;return D3D_OK;}
static HRESULT STDMETHODCALLTYPE SetIndices(IDirect3DDevice9*,IDirect3DIndexBuffer9* p){++setters;if(failSetter)return D3DERR_INVALIDCALL;selected->boundIB=p;return D3D_OK;}
static HRESULT STDMETHODCALLTYPE SetTexture(IDirect3DDevice9*,DWORD stage,IDirect3DBaseTexture9* p){++setters;Check(!stage,"owned setter texture0");if(failSetter)return D3DERR_INVALIDCALL;selected->boundTexture=p;return D3D_OK;}
static HRESULT STDMETHODCALLTYPE SetDeclaration(IDirect3DDevice9*,IDirect3DVertexDeclaration9* p){++setters;if(failSetter)return D3DERR_INVALIDCALL;selected->boundDecl=p;return D3D_OK;}
static HRESULT STDMETHODCALLTYPE SetTransform(IDirect3DDevice9*,D3DTRANSFORMSTATETYPE t,const D3DMATRIX* p){++setters;if(failSetter)return D3DERR_INVALIDCALL;if(t==D3DTS_WORLD)selected->deviceWorld=*p;else if(t==D3DTS_VIEW)selected->deviceView=*p;else if(t==D3DTS_PROJECTION)selected->deviceProjection=*p;else return D3DERR_INVALIDCALL;return D3D_OK;}
static HRESULT STDMETHODCALLTYPE SetStage(IDirect3DDevice9*,DWORD stage,D3DTEXTURESTAGESTATETYPE t,DWORD v){++setters;if(failSetter||stage)return D3DERR_INVALIDCALL;selected->stages[t]=v;return D3D_OK;}
static HRESULT STDMETHODCALLTYPE SetSampler(IDirect3DDevice9*,DWORD stage,D3DSAMPLERSTATETYPE t,DWORD v){++setters;if(failSetter||stage)return D3DERR_INVALIDCALL;selected->sampler[t]=v;return D3D_OK;}
struct DrawScope {
  native_owner_source::ModelScope model;native_mesh_source::Scope mesh{};
  DrawScope(Fixture& f,unsigned index=0):model(Ptr(&f.models[index]),Ptr(&f.camera),f.Support()) {
    ++drawId;auto owner=f.Input(index).owner;owner.modelCall=model.sequence;owner.submission=++native_mesh_source::nextSubmission;owner.selectedMaterial=Ptr(&f.material);
    mesh.parent=native_mesh_source::active;mesh.mesh=owner.mesh;mesh.renderer=owner.renderer;mesh.sequence=owner.submission;mesh.value=f.mesh;mesh.valid=true;mesh.owner=owner;native_mesh_source::active=&mesh;
  }
  ~DrawScope(){native_mesh_source::active=mesh.parent;}
};
static bool Submit(Fixture& f,const ScopedOpaqueAlphaTest* alpha=nullptr){return source::TrySelectedDraw(f.Device(),f.Input().resources.range,alpha);}
static void Positive() {
  Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Prepare(scope);
  {DrawScope draw(f);Check(Submit(f),"qualified selected draw commits once");}
  Check(drawCalls==1&&source::selectedCalls==1&&source::selectedInstances==1&&!source::selectedSubmissionFailed,"one original selected occurrence gives one client instance");
  Check(meshVertices.size()==3,"selected mesh payload has exact triangle");
  const float positions[3][3]={{0,0,0},{1,0,0},{0,1,0}},uv[3][2]={{0,0},{1,0},{0,1}},normal[3]={0,0,1};
  for(unsigned i=0;i<3;++i)Check(!memcmp(meshVertices[i].position,positions[i],12)&&!memcmp(meshVertices[i].texcoord,uv[i],8)&&!memcmp(meshVertices[i].normal,normal,12)&&meshVertices[i].color==0xffffffff,"exact positions/normals/UV/RGBA reach recording CreateMesh");
  Check(instances.size()==1&&instances[0].transform.matrix[0][3]==12&&blends[0].textureColorOperation==f.Input().material.contract.rgb.operation,"selected exact world and native texture operation reach instance");
  Check(preserveUnlitColor&&materialOpaque.albedoConstant.x==1&&materialOpaque.opacityConstant==1&&materialOpaque.roughnessConstant==.5f&&materialOpaque.useDrawCallAlphaState==1&&materialWrapU==0&&materialWrapV==0&&materialFilter==1&&materialEmission==1,"selected material recipe preserves enabled unlit policy and native sampler");
  {DrawScope draw(f);Check(Submit(f),"second actual Model call submits its own occurrence");}
  Check(drawCalls==2&&materialCalls==1&&meshCalls==1,"repeated actual draw reuses resources but retains both instances");
}
static void Rejections() {
  for(unsigned reason=0;reason<14;++reason) {
    Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Prepare(scope);DrawScope draw(f);
    switch(reason) {
    case 0:f.Input().originallySelected=false;break;
    case 1:++native_camera_source::selected.sequence;break;
    case 2:f.material.base.renderStates[8]=1;break;
    case 3:reinterpret_cast<abi::spDXRendererMaterialCacheObservedLayout*>(f.renderer+abi::spDXRendererMaterialCacheOffset)->installedMaterial=0;break;
    case 4:f.Device()->SetStreamSource(0,nullptr,0,36);break;
    case 5:f.Device()->SetIndices(nullptr);break;
    case 6:f.Device()->SetVertexDeclaration(nullptr);break;
    case 7:f.Device()->SetTexture(0,nullptr);break;
    case 8:f.Device()->SetRenderState(D3DRS_LIGHTING,1);break;
    case 9:native_mesh_source::Forget(&f.bufferTokens[0]);break;
    case 10:transport::TextureLockResult(f.Texture(),0,0,D3D_OK);transport::TextureUnlockResult(f.Texture(),0,D3D_OK);break;
    case 11:++native_mesh_source::buffers.at(&f.bufferTokens[0]).generation;break;
    case 12:++draw.mesh.owner.modelCall;break;
    case 13:++f.models[0].base.callback2C;break;
    }
    Check(!Submit(f)&&!drawCalls&&!source::selectedCalls,"stale input, native material, binding, resource or Model call preserves precommit fallback");
  }
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Prepare(scope);DrawScope draw(f);
    auto range=f.Input().resources.range;++range.count;
    Check(!source::TrySelectedDraw(f.Device(),range,nullptr)&&!drawCalls,"exact actual indexed range required");}
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Prepare(scope);DrawScope draw(f);
    f.deviceTable[100]=reinterpret_cast<void*>(SetStream);
    Check(!Submit(f)&&!drawCalls,"missing current setter coverage rejects even identical current bindings");}
}
static void LateTransitions() {
  for(unsigned reason=0;reason<6;++reason) {
    Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Prepare(scope);DrawScope draw(f);
    // Arm only after CreateMesh: each transition occurs at the final target
    // getter of a later current-state read, after its binding/state queries.
    switch(reason) {
    case 0:onApi=[](char op,unsigned){if(op=='m')onTarget=[](){onTarget=nullptr;selected->Device()->SetStreamSource(0,nullptr,0,36);};};break;
    case 1:onApi=[](char op,unsigned){if(op=='m')onTarget=[](){onTarget=nullptr;selected->Device()->SetRenderState(D3DRS_LIGHTING,1);};};break;
    case 2:onApi=[](char op,unsigned){if(op=='m')onTarget=[](){onTarget=nullptr;failSetter=true;Check(FAILED(selected->Device()->SetRenderState(D3DRS_LIGHTING,1)),"failed wrapped setter reaches original once");};};break;
    case 3:onApi=[](char op,unsigned){if(op=='m')onTarget=[](){onTarget=nullptr;Check(!Submit(*selected),"nested selected entry preserves original path and invalidates outer operation");};};break;
    case 4:onApi=[](char op,unsigned){if(op=='m')onTarget=[](){onTarget=nullptr;native_mesh_source::Forget(&selected->bufferTokens[0]);};};break;
    case 5:onApi=[](char op,unsigned){if(op=='m')afterReferenceRelease=[](){afterReferenceRelease=nullptr;selected->Device()->SetTexture(0,nullptr);};};break;
    }
    Check(!Submit(f)&&!drawCalls&&meshCalls==1,"late COM/release mutation after resource creation cannot commit stale selected packet");
    if(reason<3)Check(setters==1,"each late wrapped setter calls its original once");
    if(reason==3)Check(source::selectedReentries==1,"reentry counted independently from API calls");
  }
}
static void CommitAndAllocation() {
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Prepare(scope);DrawScope draw(f);failDrawAt=1;
    Check(Submit(f)&&source::selectedApiFailures==1&&!source::selectedPostCommitFaults&&source::selectedSubmissionFailed,"normal API failure has no independent late fault and must not duplicate through fallback");
    Check(!Submit(f)&&drawCalls==1,"failure latch blocks later selected API calls");}
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Prepare(scope);DrawScope draw(f);
    onApi=[](char op,unsigned){if(op=='d')++frameId;};
    Check(Submit(f)&&source::selectedInstances==1&&source::selectedPostCommitFaults==1&&source::selectedSubmissionFailed,"late frame change keeps one committed call and separate postcommit fault");}
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Prepare(scope);DrawScope draw(f);failAllocationAfter=0;
    Check(!Submit(f)&&!drawCalls,"precommit allocation failure preserves fallback");failAllocationAfter=-1;}
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Prepare(scope);DrawScope draw(f);
    onApi=[](char op,unsigned){if(op=='d')throw std::bad_alloc();};
    Check(Submit(f)&&source::selectedApiFailures==1&&source::selectedSubmissionFailed,"allocation exception inside entered API cannot retry through fallback");}
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Prepare(scope);DrawScope draw(f);
    onApi=[](char op,unsigned){if(op=='d'){native_mesh_source::layouts.clear();failAllocationAfter=0;}};
    Check(Submit(f)&&source::selectedInstances==1&&source::selectedPostCommitFaults==1&&source::selectedSubmissionFailed,"allocation failure after successful API retains success and reports postcommit fault only");failAllocationAfter=-1;}
}
static DWORD WINAPI TryOtherThread(void* result){auto out=static_cast<bool*>(result);*out=guard.try_lock();if(*out)guard.unlock();return 0;}
static void AlphaNormalization() {
  for(unsigned policy=0;policy<3;++policy) {
    Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));
    f.material.base.renderStates[10]=6;f.states[D3DRS_ALPHATESTENABLE]=1;f.states[D3DRS_ALPHAFUNC]=D3DCMP_GREATEREQUAL;
    opaqueAlphaTest=true;keepTrivialAlphaTestForComparison=policy==1;
    f.Prepare(scope);DrawScope draw(f);const auto original=f.material;
    {ScopedOpaqueAlphaTest alpha(f.Device());
      Check(alpha.changed==(policy!=1),"owned alpha normalization uses the actual wrapped setter");
      const bool submitted=Submit(f,policy==2?nullptr:&alpha);
      Check(submitted==(policy!=2),"only the actual scoped normalization witness recovers native alpha input");
      if(submitted)Check(blends.back().alphaTestCompareOp==(policy==0?7u:6u),"selected API alpha policy matches normalized or comparison mode");
    }
    Check(f.states[D3DRS_ALPHAFUNC]==D3DCMP_GREATEREQUAL&&!memcmp(&original,&f.material,sizeof(original)),"alpha scope restores device state and leaves native material unchanged");
  }
}
static void StateTransitions() {
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Prepare(scope);DrawScope draw(f);
    IDirect3DStateBlock9* block=nullptr;Check(SUCCEEDED(f.Device()->CreateStateBlock(D3DSBT_ALL,&block))&&block,"owned successful stateblock creation is tracked");
    onApi=[](char op,unsigned){if(op=='m')onTarget=[](){onTarget=nullptr;reinterpret_cast<IDirect3DStateBlock9*>(&selected->block)->Apply();};};
    Check(!Submit(f)&&!drawCalls&&blockApplies==1,"StateBlock Apply from late target read invalidates selected input");block->Release();}
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Prepare(scope);DrawScope draw(f);
    IDirect3DStateBlock9* block=nullptr;Check(SUCCEEDED(f.Device()->EndStateBlock(&block))&&block,"owned EndStateBlock result is tracked");
    onBlockRelease=[](){selected->block.table=nullptr;std::unique_lock<std::recursive_mutex> lock(guard);state::Witness w{};
      Check(!state::Read(lock,selected->Device(),w),"ReleaseBlock active mutation rejects before dereferencing destroyed null vtable");
      Check(!Submit(*selected)&&!drawCalls,"selected reentry inside ReleaseBlock cannot inspect freed stateblock table");};
    Check(!block->Release()&&state::blocks.empty(),"final stateblock release retires the observed record once");}
  {Fixture f;scene_geometry::Scope scope(Ptr(&f.scene),Ptr(&f.camera));f.Prepare(scope);DrawScope draw(f);state::Witness before{};
    {std::unique_lock<std::recursive_mutex> lock(guard);Check(state::Read(lock,f.Device(),before),"valid device witness before detached Reset window");}
    {state::DetachedMutation reset;bool acquired=false;HANDLE thread=CreateThread(nullptr,0,TryOtherThread,&acquired,0,nullptr);Check(thread!=nullptr,"owned Reset-lock check thread created");
      Check(WaitForSingleObject(thread,2000)==WAIT_OBJECT_0,"bounded Reset-lock check thread completed");CloseHandle(thread);Check(acquired,"detached Reset mutation releases guard across external operation");
      std::unique_lock<std::recursive_mutex> lock(guard);state::Witness now{};Check(!state::Read(lock,f.Device(),now)&&!Submit(f),"active detached Reset invalidates reads and selected entry");}
    {std::unique_lock<std::recursive_mutex> lock(guard);Check(!state::Current(lock,before)&&!state::mutations,"Reset end closes active count but never revives pre-Reset witness");}}
}
static void Run(){Positive();Rejections();LateTransitions();CommitAndAllocation();AlphaNormalization();StateTransitions();Check(liveMeshes.empty()&&liveMaterials.empty(),"all selected recording API ownership retired");}
}
int main(){SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);try{direct_test::Watchdog watchdog;selected_test::Run();printf("{\"status\":\"PASS\",\"checks\":%u,\"gpu\":false,\"nativeGameCodeExecuted\":false,\"ownedComCalls\":%u}\n",direct_test::checks,direct_test::comCalls);return 0;}catch(const std::exception& e){fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
