// Own integration test: real system D3D9 objects and production draw/texture
// hooks, recording Remix API. GPU channel values are tested by the stock fixture.
#define WINX_REMIX_TEST
#include "winx_d3d9_probe.cpp"
#include <cstdlib>
static unsigned checks,apiDraws,nativeSourceChecks;
static uintptr_t nextHandle=1;
static std::vector<remixapi_HardcodedVertex> lastVertices;
static std::wstring lastAlbedo,lastEmission;
static std::map<remixapi_MaterialHandle,std::pair<std::wstring,std::wstring>> recordedMaterials;
static std::map<remixapi_MeshHandle,remixapi_MaterialHandle> recordedMeshes;
static std::map<remixapi_MeshHandle,std::vector<remixapi_HardcodedVertex>> recordedVertices;
static remixapi_InstanceInfoBlendEXT lastBlend{};
static remixapi_Transform lastTransform{};
static bool rejectDraw;
static void Check(bool ok,const char* message) {++checks;if(!ok){fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
static void Hr(HRESULT hr,const char* message) {Check(SUCCEEDED(hr),message);}
static remixapi_ErrorCode REMIXAPI_CALL Config(const char*,const char*) {return REMIXAPI_ERROR_CODE_SUCCESS;}
static remixapi_ErrorCode REMIXAPI_CALL Material(const remixapi_MaterialInfo* info,remixapi_MaterialHandle* out) {
  lastAlbedo=info->albedoTexture?info->albedoTexture:L"";lastEmission=info->emissiveTexture?info->emissiveTexture:L"";
  Check(!lastAlbedo.empty(),"API material has an explicit albedo texture");
  *out=reinterpret_cast<remixapi_MaterialHandle>(nextHandle++);recordedMaterials[*out]={lastAlbedo,lastEmission};return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL Mesh(const remixapi_MeshInfo* info,remixapi_MeshHandle* out) {
  Check(info->surfaces_count==1,"one complete surface per draw");const auto& s=info->surfaces_values[0];
  lastVertices.assign(s.vertices_values,s.vertices_values+s.vertices_count);
  Check(s.vertices_count==6&&s.indices_count==6,"strip expanded to two complete triangles");
  *out=reinterpret_cast<remixapi_MeshHandle>(nextHandle++);recordedMeshes[*out]=s.material;recordedVertices[*out]=lastVertices;return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL Instance(const remixapi_InstanceInfo* info) {
  ++apiDraws;Check(info->categoryFlags==0,"ordinary material is not a decal");
  lastBlend=*static_cast<const remixapi_InstanceInfoBlendEXT*>(info->pNext);
  lastTransform=info->transform;lastVertices=recordedVertices.at(info->mesh);
  const auto& paths=recordedMaterials.at(recordedMeshes.at(info->mesh));lastAlbedo=paths.first;lastEmission=paths.second;
  return rejectDraw?REMIXAPI_ERROR_CODE_GENERAL_FAILURE:REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL DeleteMesh(remixapi_MeshHandle) {return REMIXAPI_ERROR_CODE_SUCCESS;}
static remixapi_ErrorCode REMIXAPI_CALL DeleteMaterial(remixapi_MaterialHandle) {return REMIXAPI_ERROR_CODE_SUCCESS;}
struct Vertex {float x,y,z,nx,ny,nz;DWORD color;float u,v;};
static std::vector<DWORD> Dds(const std::wstring& path) {
  FILE* file=nullptr;Check(_wfopen_s(&file,path.c_str(),L"rb")==0&&file,"read emitted DDS");
  fseek(file,0,SEEK_END);const auto length=ftell(file);rewind(file);Check(length>128&&length%4==0,"DDS payload size");
  std::vector<DWORD> words(size_t(length)/4);Check(fread(words.data(),4,words.size(),file)==words.size(),"complete DDS read");fclose(file);return words;
}
#if defined(_M_IX86)
static void NativeRanges() {
  namespace source=native_mesh_source;
  source::Bytes partial{};partial.data.resize(16);partial.partial=true;
  Check(!source::Covered(partial,0,1),"uncaptured native buffer has no covered draw bytes");
  source::PutRange(partial,4,std::vector<uint8_t>{41,42,43,44});
  Check(source::Covered(partial,4,4)&&source::Covered(partial,5,3),"partial native interval covers its exact byte range");
  Check(!source::Covered(partial,3,2)&&!source::Covered(partial,7,2)&&!source::Covered(partial,16,1)&&
    !source::Covered(partial,17,0)&&!source::Covered(partial,4,size_t(-1)),"partial coverage rejects gaps and overflowing bounds");
  std::vector<uint8_t> uploaded(16,0xa5);memcpy(uploaded.data()+4,partial.data.data()+4,4);
  Check(source::EqualsUpload(partial,uploaded),"upload comparison ignores bytes not captured from native inputs");
  ++uploaded[5];Check(!source::EqualsUpload(partial,uploaded),"upload comparison detects a changed captured byte");--uploaded[5];
  uploaded.pop_back();Check(!source::EqualsUpload(partial,uploaded),"upload comparison rejects a different buffer size");uploaded.push_back(0xa5);
  source::PutRange(partial,10,std::vector<uint8_t>{51,52});
  Check(!source::Covered(partial,4,8),"separated native ranges do not cover the gap between them");
  source::PutRange(partial,8,std::vector<uint8_t>{49,50});
  Check(source::Covered(partial,4,8)&&partial.ranges.size()==1,"adjacent native ranges merge into continuous coverage");
  partial.verified=true;source::PutRange(partial,6,std::vector<uint8_t>{61,62,63,64});
  Check(!partial.verified&&source::Covered(partial,4,8)&&partial.data[6]==61&&partial.data[9]==64,
    "overlapping native input replaces bytes and requires upload reverification");
  memcpy(uploaded.data()+4,partial.data.data()+4,8);Check(source::EqualsUpload(partial,uploaded),"merged captured ranges match uploaded bytes");
  auto complete=partial;complete.partial=false;
  Check(!source::EqualsUpload(complete,uploaded)&&source::EqualsUpload(complete,complete.data),"complete native captures compare every byte");
}
template<class Draw> static void NativeSource(IDirect3DDevice9* d,IDirect3DVertexBuffer9* vb,
                                             IDirect3DIndexBuffer9* ib,const Draw& draw) {
  // Synthetic ABI fixture for our adapter only. No game method, fixed-address
  // memory or native hook is executed; original/debug qualification is separate.
  namespace source=native_mesh_source;namespace abi=sparkplug::evidence::pc;
  const auto initialChecks=checks;
  NativeRanges();
  Check(!source::enabled&&!source::active&&source::buffers.empty()&&!source::retainedBytes,"fresh native source fixture");
  const bool oldSubmit=source::submitEnabled;const auto oldThread=source::ownerThread;
  const auto oldMatched=source::matched,oldUsed=source::used,oldInvalidations=source::invalidations,oldMismatches=source::uploadMismatches;
  const auto vertexBytes=surfaceBuffers.at(vb).bytes,indexBytes=surfaceBuffers.at(ib).bytes;
  auto address=[](const void* p){return uint32_t(reinterpret_cast<uintptr_t>(p));};
  std::vector<uint8_t> renderer(0xca80);
  abi::spDXMeshObservedLayout mesh{};abi::spDXVertexBufferLayout nativeVB{};abi::spDXIndexBufferLayout nativeIB{};
  abi::spDXSharedMeshDataObservedLayout shared{};abi::spDXMaterialObservedLayout material{};
  abi::spRendererDrawContextObservedLayout state{};
  nativeVB.base.vtableAddress=abi::spDXVertexBufferVTable;nativeVB.direct3DVertexBuffer=address(vb);nativeVB.byteSize=UINT(vertexBytes.size());
  nativeIB.base.vtableAddress=abi::spDXIndexBufferVTable;nativeIB.direct3DIndexBuffer=address(ib);nativeIB.byteSize=UINT(indexBytes.size());
  shared.base.vtableAddress=abi::spDXSharedMeshDataVTable;shared.vertexBuffer=address(&nativeVB);shared.indexBuffer=address(&nativeIB);
  material.base.base.vtableAddress=abi::spDXMaterialPrimaryVTable;material.base.materialVTable=abi::spDXMaterialInterfaceVTable;material.base.passCount=1;
  mesh.base.base.base.base.base.vtableAddress=abi::spDXMeshVTable;mesh.base.base.secondaryVTable=abi::spDXMeshInterfaceVTable;
  mesh.base.base.vertexCount=4;mesh.base.base.primitiveCount=2;mesh.indexType=3;mesh.vertexStride=sizeof(Vertex);
  mesh.vertexBuffer=address(&nativeVB);mesh.indexBuffer=address(&nativeIB);mesh.sharedMeshData=address(&shared);
  IDirect3DVertexDeclaration9* declaration=nullptr;Hr(d->GetVertexDeclaration(&declaration),"native fixture declaration");
  Check(declaration!=nullptr,"native fixture declaration exists");mesh.vertexDeclaration=address(declaration);declaration->Release();
  state.device=address(d);state.vertexBuffer=mesh.vertexBuffer;state.indexBuffer=mesh.indexBuffer;
  state.vertexDeclaration=mesh.vertexDeclaration;state.selectedMaterial=address(&material);
  memcpy(renderer.data(),&abi::spPCRendererPrimaryVTable,4);
  memcpy(renderer.data()+abi::spRendererDrawContextOffset,&state,sizeof(state));
  D3DMATRIX oldWorld{},world{};Hr(d->GetTransform(D3DTS_WORLD,&oldWorld),"save native fixture world");
  world=oldWorld;world._41=.25f;world._42=-.5f;world._43=.75f;
  Hr(d->SetTransform(D3DTS_WORLD,&world),"native fixture translated world");memcpy(renderer.data()+0xca40,&world,sizeof(world));
  source::Scope scope{nullptr,address(&mesh),address(renderer.data()),73};scope.value=mesh;scope.valid=true;
  source::buffers.emplace(vb,source::Bytes{vertexBytes,19,address(&shared),address(&nativeVB)});
  source::buffers.emplace(ib,source::Bytes{indexBytes,19,address(&shared),address(&nativeIB)});
  source::retainedBytes=vertexBytes.size()+indexBytes.size();source::ownerThread=GetCurrentThreadId();
  source::active=&scope;source::enabled=source::submitEnabled=true;
  const source::DrawRange range{D3DPT_TRIANGLESTRIP,0,0,4,0,2};source::Geometry geometry{};
  auto resolve=[&](){geometry={};return source::Resolve(d,range,vb,ib,0,sizeof(Vertex),geometry);};
  Check(resolve(),"matching native mesh resolves against actual COM buffers");
  Check(geometry.vertices==&source::buffers.at(vb)&&geometry.indices==&source::buffers.at(ib)&&
    geometry.mesh==address(&mesh)&&geometry.renderer==address(renderer.data())&&geometry.submission==73&&
    geometry.range.type==range.type&&geometry.range.base==0&&geometry.range.minimum==0&&geometry.range.vertices==4&&
    geometry.range.start==0&&geometry.range.count==2&&geometry.stride==sizeof(Vertex)&&!memcmp(&geometry.world,&world,sizeof(world)),
    "native packet preserves exact source identity, range and world");
  auto submit=[&](bool native,float firstX,const char* message){
    const auto beforeUsed=source::used,beforeApi=apiDraws,beforeSubmitted=materialChannelsSubmitted;draw();
    Check(source::used==beforeUsed+unsigned(native)&&apiDraws==beforeApi+1&&materialChannelsSubmitted==beforeSubmitted+1,message);
    Check(lastVertices.size()==6&&lastVertices[0].position[0]==firstX,"selected source bytes reach recorded API mesh");
    Check(lastTransform.matrix[0][3]==world._41&&lastTransform.matrix[1][3]==world._42&&lastTransform.matrix[2][3]==world._43,
      "selected source world reaches API instance");
  };
  float firstX=0;memcpy(&firstX,vertexBytes.data(),4);
  submit(true,firstX,"matching native draw reaches API exactly once");
  Check(source::buffers.at(vb).verified&&source::buffers.at(ib).verified,"native bytes verified against both original uploads");
  auto& partialVB=source::buffers.at(vb);auto& partialIB=source::buffers.at(ib);
  partialVB.partial=partialIB.partial=true;partialVB.ranges={{0,vertexBytes.size()}};partialIB.ranges={{0,indexBytes.size()}};
  Check(resolve(),"fully covered partial buffers resolve for the draw");
  partialVB.ranges={{0,sizeof(Vertex)},{2*sizeof(Vertex),vertexBytes.size()}};
  Check(!resolve(),"a gap inside the declared vertex range rejects native submission");
  source::PutRange(partialVB,sizeof(Vertex),std::vector<uint8_t>(vertexBytes.begin()+sizeof(Vertex),vertexBytes.begin()+2*sizeof(Vertex)));
  Check(resolve(),"capturing the missing vertex interval restores native resolution");
  partialIB.ranges={{0,2},{4,indexBytes.size()}};Check(!resolve(),"a missing index word rejects native submission");
  source::PutRange(partialIB,2,std::vector<uint8_t>(indexBytes.begin()+2,indexBytes.begin()+4));
  Check(resolve(),"capturing the missing index word restores native resolution");
  scope.value.vertexBegin=1;scope.value.base.base.vertexCount=3;scope.value.base.base.primitiveCount=1;
  partialVB.ranges={{sizeof(Vertex),vertexBytes.size()}};partialIB.ranges={{0,6}};
  source::Geometry shifted{};
  Check(source::Resolve(d,{D3DPT_TRIANGLESTRIP,1,0,3,0,1},vb,ib,0,sizeof(Vertex),shifted)&&shifted.range.base==1,
    "nonzero vertex base checks coverage at base times stride");
  partialVB.ranges={{0,3*sizeof(Vertex)}};
  Check(!source::Resolve(d,{D3DPT_TRIANGLESTRIP,1,0,3,0,1},vb,ib,0,sizeof(Vertex),shifted),"vertex coverage at the wrong offset cannot qualify");
  scope.value=mesh;scope.value.indexBegin=1;scope.value.base.base.primitiveCount=1;
  partialVB.ranges={{0,vertexBytes.size()}};partialIB.ranges={{2,indexBytes.size()}};
  Check(source::Resolve(d,{D3DPT_TRIANGLESTRIP,0,0,4,1,1},vb,ib,0,sizeof(Vertex),shifted)&&shifted.range.start==1,
    "nonzero index start checks coverage in two-byte index elements");
  partialIB.ranges={{0,6}};
  Check(!source::Resolve(d,{D3DPT_TRIANGLESTRIP,0,0,4,1,1},vb,ib,0,sizeof(Vertex),shifted),"index coverage at the wrong offset cannot qualify");
  scope.value=mesh;partialVB.ranges={{0,vertexBytes.size()}};partialIB.ranges={{0,indexBytes.size()}};
  submit(true,firstX,"covered partial native buffers reach the common API converter");
  partialVB.partial=partialIB.partial=false;partialVB.ranges.clear();partialIB.ranges.clear();
  // Fault-inject only the adapter's audit snapshots after verification. Native
  // captured bytes and actual COM buffers remain untouched. Distinct values
  // prove which source the converter reads, including when mesh handles recur.
  auto& auditVertices=surfaceBuffers.at(vb).bytes;auto& auditIndices=surfaceBuffers.at(ib).bytes;
  for(unsigned i=0;i<4;++i){float x=0;memcpy(&x,auditVertices.data()+i*sizeof(Vertex),4);x+=8;memcpy(auditVertices.data()+i*sizeof(Vertex),&x,4);}
  const WORD alternateIndices[]={1,0,3,2};memcpy(auditIndices.data(),alternateIndices,sizeof(alternateIndices));
  float fallbackX=0;memcpy(&fallbackX,auditVertices.data()+sizeof(Vertex),4);
  submit(true,firstX,"verified native source does not read altered audit snapshots");
  ++scope.value.indexBegin;Check(!resolve(),"native range mismatch rejected");submit(false,fallbackX,"range mismatch retains D3D source");--scope.value.indexBegin;
  ++source::buffers.at(vb).owner;Check(!resolve(),"native owner mismatch rejected");submit(false,fallbackX,"owner mismatch retains D3D source");--source::buffers.at(vb).owner;
  ++source::buffers.at(ib).generation;Check(!resolve(),"mixed native buffer generations rejected");submit(false,fallbackX,"generation mismatch retains D3D source");--source::buffers.at(ib).generation;
  D3DMATRIX otherWorld=world;otherWorld._41+=1;memcpy(renderer.data()+0xca40,&otherWorld,sizeof(otherWorld));
  Check(!resolve(),"native world mismatch rejected");submit(false,fallbackX,"world mismatch retains D3D source");memcpy(renderer.data()+0xca40,&world,sizeof(world));
  scope.value.componentWeightCount=4;Check(!resolve(),"skinned mesh remains outside static native cohort");submit(false,fallbackX,"skinned mesh retains D3D source");scope.value.componentWeightCount=0;
  material.base.passCount=2;Check(!resolve(),"multipass mesh remains outside native cohort");submit(false,fallbackX,"multipass mesh retains D3D source");material.base.passCount=1;
  source::buffers.at(vb).verified=source::buffers.at(ib).verified=false;
  const auto mismatches=source::uploadMismatches;submit(false,fallbackX,"unverified different uploads retain D3D source");
  Check(source::uploadMismatches==mismatches+1&&!source::buffers.at(vb).verified&&!source::buffers.at(ib).verified,"failed byte comparison never marks native data verified");
  auditVertices=vertexBytes;auditIndices=indexBytes;submit(true,firstX,"exact source is reverified after audit fixture restoration");
  void* locked=nullptr;
  Hr(vb->Lock(0,0,&locked,D3DLOCK_READONLY),"native readonly VB lock");Hr(vb->Unlock(),"native readonly VB unlock");
  Hr(ib->Lock(0,0,&locked,D3DLOCK_READONLY),"native readonly IB lock");Hr(ib->Unlock(),"native readonly IB unlock");
  Check(resolve()&&source::buffers.at(vb).verified&&source::buffers.at(ib).verified,"readonly locks preserve native provenance");
  submit(true,firstX,"readonly access preserves native submission");
  Hr(vb->Lock(0,0,&locked,0),"native writable VB lock");
  Check(!source::buffers.count(vb)&&source::buffers.count(ib)==1&&source::retainedBytes==indexBytes.size(),"writable VB lock invalidates native capture immediately");
  memcpy(locked,vertexBytes.data(),vertexBytes.size());Hr(vb->Unlock(),"native writable VB unlock");
  Check(!resolve(),"partial native buffer pair cannot resolve");submit(false,firstX,"written VB retains D3D upload source");
  source::buffers.emplace(vb,source::Bytes{vertexBytes,19,address(&shared),address(&nativeVB)});source::retainedBytes+=vertexBytes.size();
  Hr(ib->Lock(0,0,&locked,0),"native writable IB lock");
  Check(!source::buffers.count(ib)&&source::buffers.count(vb)==1&&source::retainedBytes==vertexBytes.size(),"writable IB lock invalidates native capture immediately");
  memcpy(locked,indexBytes.data(),indexBytes.size());Hr(ib->Unlock(),"native writable IB unlock");
  Check(!resolve(),"written index pair cannot resolve");submit(false,firstX,"written IB retains D3D upload source");
  source::Forget(vb);Check(source::buffers.empty()&&!source::retainedBytes,"native fixture releases all captured bytes");
  source::active=nullptr;source::enabled=false;source::submitEnabled=oldSubmit;source::ownerThread=oldThread;
  source::matched=oldMatched;source::used=oldUsed;source::invalidations=oldInvalidations;source::uploadMismatches=oldMismatches;
  Hr(d->SetTransform(D3DTS_WORLD,&oldWorld),"restore world after native fixture");ClearSurfaceBases();
  nativeSourceChecks=checks-initialChecks;
}
#endif
int main() {
  frameId=1;autoSurfaceRoles=materialChannelsEnabled=true;preserveUnlitColor=false;
  Check(GetFullPathNameW(L"assets",MAX_PATH,surfaceAssetDirectory,nullptr)!=0,"asset directory");
  Check(CreateDirectoryW(surfaceAssetDirectory,nullptr)!=0,"fresh evidence directory");
  remixapi_Interface recording{};recording.SetConfigVariable=Config;recording.CreateMaterial=Material;recording.CreateMesh=Mesh;
  recording.DrawInstance=Instance;recording.DestroyMesh=DeleteMesh;recording.DestroyMaterial=DeleteMaterial;testRemixApi=&recording;
  auto module=LoadLibraryExW(L"d3d9.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);Check(module!=nullptr,"system D3D9");
  auto factory=reinterpret_cast<IDirect3D9*(WINAPI*)(UINT)>(GetProcAddress(module,"Direct3DCreate9"));Check(factory!=nullptr,"D3D factory");
  auto d3d=factory(D3D_SDK_VERSION);Check(d3d!=nullptr,"D3D object");
  HWND hwnd=CreateWindowW(L"STATIC",L"Winx channel integration",WS_OVERLAPPED,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
  Check(hwnd!=nullptr,"owned hidden window");Patch(d3d,16,reinterpret_cast<void*>(CreateDevice));
  D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=hwnd;
  pp.BackBufferWidth=64;pp.BackBufferHeight=64;pp.BackBufferFormat=D3DFMT_X8R8G8B8;pp.EnableAutoDepthStencil=TRUE;pp.AutoDepthStencilFormat=D3DFMT_D24S8;
  IDirect3DDevice9* d=nullptr;Hr(d3d->CreateDevice(0,D3DDEVTYPE_HAL,hwnd,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&d),"real device with production hooks");
  D3DMATRIX identity{},projection{};identity._11=identity._22=identity._33=identity._44=1;
  projection._11=projection._22=projection._33=projection._34=1;projection._43=-.1f;
  Hr(d->SetTransform(D3DTS_WORLD,&identity),"world");Hr(d->SetTransform(D3DTS_VIEW,&identity),"view");Hr(d->SetTransform(D3DTS_PROJECTION,&projection),"projection");
  for(auto state:{D3DRS_LIGHTING,D3DRS_SPECULARENABLE,D3DRS_ALPHATESTENABLE,D3DRS_ALPHABLENDENABLE})Hr(d->SetRenderState(state,0),"disabled state");
  Hr(d->SetRenderState(D3DRS_ZENABLE,TRUE),"depth");Hr(d->SetRenderState(D3DRS_ZWRITEENABLE,TRUE),"depth write");
  Hr(d->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL),"depth compare");Hr(d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE),"two sided");
  // Disabled blending must ignore the stale additive factors.
  Hr(d->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_ONE),"blend src");Hr(d->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_ONE),"blend dst");
  constexpr DWORD format=D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_DIFFUSE|D3DFVF_TEX1;
  const Vertex vertices[]={{-1,1,3,0,0,-1,0x00336699,0,0},{1,1,3,0,0,-1,0x8012a5ef,1,0},
    {-1,-1,3,0,0,-1,0xffe08020,0,1},{1,-1,3,0,0,-1,0x40996633,1,1}};
  IDirect3DVertexBuffer9* vb=nullptr;IDirect3DIndexBuffer9* ib=nullptr;void* bytes=nullptr;
  Hr(d->CreateVertexBuffer(sizeof(vertices),0,format,D3DPOOL_MANAGED,&vb,nullptr),"VB");
  Hr(vb->Lock(0,0,&bytes,0),"VB write");memcpy(bytes,vertices,sizeof(vertices));Hr(vb->Unlock(),"VB complete");
  const WORD indices[]={0,1,2,3};Hr(d->CreateIndexBuffer(sizeof(indices),0,D3DFMT_INDEX16,D3DPOOL_MANAGED,&ib,nullptr),"IB");
  Hr(ib->Lock(0,0,&bytes,0),"IB write");memcpy(bytes,indices,sizeof(indices));Hr(ib->Unlock(),"IB complete");
  Hr(d->SetStreamSource(0,vb,0,sizeof(Vertex)),"vertex stream");Hr(d->SetIndices(ib),"indices");Hr(d->SetFVF(format),"declaration");
  IDirect3DTexture9* texture=nullptr;Hr(d->CreateTexture(4,4,3,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture,nullptr),"three mip texture");
  D3DLOCKED_RECT lock{};
  for(UINT level=0;level<3;++level){Hr(texture->LockRect(level,&lock,nullptr,0),"mip write");for(UINT y=0;y<(4u>>level);++y)for(UINT x=0;x<(4u>>level);++x)
    reinterpret_cast<DWORD*>(static_cast<uint8_t*>(lock.pBits)+y*lock.Pitch)[x]=0x8080c040u+level;Hr(texture->UnlockRect(level),"mip complete");}
  Hr(d->SetTexture(0,texture),"texture bind");Hr(d->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE),"RGB operation");
  Hr(d->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE),"RGB texture");Hr(d->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE),"RGB diffuse");
  Hr(d->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_MODULATE),"alpha operation");Hr(d->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE),"alpha texture");
  Hr(d->SetTextureStageState(0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE),"alpha diffuse");Hr(d->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE),"single stage");
  Hr(d->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_POINT),"point sampler");
  auto draw=[&](){ClearSurfaceBases();Hr(d->BeginScene(),"begin");Hr(d->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP,0,0,4,0,2),"production draw");Hr(d->EndScene(),"end");};
  draw();Check(materialChannelsSubmitted==1&&apiDraws==1,"production draw reaches API");
  Check(lastEmission.empty(),"unlit input creates no inferred emission");Check(!lastBlend.alphaBlendEnabled&&lastBlend.alphaTestCompareOp==7,"disabled alpha and stale blend state preserved");
  const unsigned order[]={0,1,2,2,1,3};for(unsigned i=0;i<6;++i){Check(lastVertices[i].color==vertices[order[i]].color,"variable RGB and alpha unchanged");
    Check(lastVertices[i].texcoord[0]==vertices[order[i]].u&&lastVertices[i].texcoord[1]==vertices[order[i]].v,"UV unchanged");}
  auto dds=Dds(lastAlbedo);Check(dds.size()==53&&dds[7]==3&&dds[32]==0x8080c040&&dds[48]==0x8080c041&&dds[52]==0x8080c042,"all mips and texture alpha preserved");
  const auto initialHash=BoundChannelTextureHash(d);const auto initialPath=lastAlbedo;
  draw();Check(surfaceMeshCreates==1&&surfaceMaterialCreates==1,"unchanged content reuses API resources");
  Hr(texture->LockRect(1,&lock,nullptr,D3DLOCK_READONLY),"readonly mip");Hr(texture->UnlockRect(1),"readonly complete");
  Check(surfaceChannelTextureHashes.count(texture)==1&&BoundChannelTextureHash(d)==initialHash,"read-only access preserves cache");
  Hr(texture->LockRect(1,&lock,nullptr,0),"lower mip update");*static_cast<DWORD*>(lock.pBits)=0x12345678;Hr(texture->UnlockRect(1),"lower mip commit");
  Check(surfaceChannelTextureHashes.count(texture)==0,"writable texture lock invalidates identity");draw();
  Check(BoundChannelTextureHash(d)!=initialHash&&lastAlbedo!=initialPath,"lower mip change creates a new material identity");
  dds=Dds(lastAlbedo);Check(dds[48]==0x12345678,"lower mip write reaches new DDS");
  IDirect3DSurface9* surface=nullptr;Hr(texture->GetSurfaceLevel(2,&surface),"mip surface");
  Hr(surface->LockRect(&lock,nullptr,0),"surface update");*static_cast<DWORD*>(lock.pBits)=0x90abcdef;Hr(surface->UnlockRect(),"surface commit");surface->Release();
  Check(surfaceChannelTextureHashes.count(texture)==0,"surface lock invalidates owning texture");draw();dds=Dds(lastAlbedo);Check(dds[52]==0x90abcdef,"surface write reaches DDS");
  const auto submitted=materialChannelsSubmitted;keepMaterialChannelsForComparison=true;draw();Check(materialChannelsSubmitted==submitted,"live comparison uses original draw");keepMaterialChannelsForComparison=false;
  rejectDraw=true;draw();Check(materialChannelsSubmitted==submitted&&materialChannelsRejected==1,"API draw failure falls back to completed D3D9 draw");rejectDraw=false;
  Hr(d->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_ADD),"unsupported combiner");draw();Check(materialChannelsSubmitted==submitted,"unsupported combiner retains original draw");
  Hr(d->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE),"restore combiner");draw();Check(materialChannelsSubmitted==submitted+1,"API path returns after fallback");
  const auto reflectancePixels=Dds(lastAlbedo);
  SetPreserveUnlitColor(true);Check(surfaceMeshes.empty()&&surfaceMaterials.empty(),"policy switch retires old resources before drawing");draw();
  Check(!lastEmission.empty()&&Dds(lastAlbedo)==reflectancePixels,"preserving unlit signal adds emission without changing reflectance");
  const auto unlitEmission=lastEmission;dds=Dds(lastEmission);
  Check(dds[32]==0x8080c040&&dds[48]==0x12345678&&dds[52]==0x90abcdef,"original unlit texels and all mips reach emission");
  for(unsigned i=0;i<6;++i)Check(lastVertices[i].color==vertices[order[i]].color,"unlit emission shares original interpolated RGBA");
  Hr(d->SetRenderState(D3DRS_AMBIENT,0xff12ef45),"unused ambient");draw();
  Check(lastEmission==unlitEmission,"unused ambient cannot alter unlit signal");
  SetPreserveUnlitColor(false);draw();Check(lastEmission.empty(),"unlit preservation can be disabled without restart");
  SetPreserveUnlitColor(true);draw();Check(lastEmission==unlitEmission,"unlit assets can be reused after restoring preservation");
#if defined(_M_IX86)
  NativeSource(d,vb,ib,draw);
#endif
  D3DMATERIAL9 material{};material.Diffuse={.6f,.4f,.2f,.5f};material.Ambient={.5f,.25f,.125f,1};material.Emissive={.1f,.2f,.3f,1};
  Hr(d->SetRenderState(D3DRS_LIGHTING,TRUE),"lit material");Hr(d->SetRenderState(D3DRS_COLORVERTEX,FALSE),"constant material sources");
  Hr(d->SetMaterial(&material),"lit coefficients");Hr(d->SetRenderState(D3DRS_AMBIENT,0xff408020),"lit ambient");
  draw();Check(!lastEmission.empty(),"lit material creates independent emission");
  dds=Dds(lastAlbedo);const auto aPixel=dds[32];dds=Dds(lastEmission);const auto ePixel=dds[32];
  // A=(128,192,64)*(.6,.4,.2); E=T*(emissive+ambient*materialAmbient), rounded to UNORM8.
  Check(aPixel==0x804d4d0d&&ePixel==0x801d3e14,"independent albedo and additive coefficient textures reach API");
  for(const auto& vertex:lastVertices)Check(vertex.color==0x80ffffff,"material alpha replaces vertex alpha without tinting albedo");
  Hr(d->SetRenderState(D3DRS_AMBIENT,0xffc02080),"changed ambient");draw();dds=Dds(lastAlbedo);Check(dds[32]==aPixel,"ambient cannot modify base albedo");
  dds=Dds(lastEmission);Check(dds[32]!=ePixel,"ambient change updates additive channel");
  const auto litSubmitted=materialChannelsSubmitted;Hr(d->SetRenderState(D3DRS_COLORVERTEX,TRUE),"vertex ambient mode");
  Hr(d->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE,D3DMCS_MATERIAL),"constant diffuse source");
  Hr(d->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE,D3DMCS_COLOR1),"varying ambient source");
  Hr(d->SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE,D3DMCS_MATERIAL),"constant emission source");
  draw();Check(materialChannelsSubmitted==litSubmitted,"unrepresentable independent vertex ambient retains original draw");
  Hr(d->SetTexture(0,nullptr),"unbind texture");Check(texture->Release()==0,"texture final release");Check(surfaceChannelTextureHashes.empty(),"released texture identity removed");
  Hr(d->SetStreamSource(0,nullptr,0,0),"unbind vertices");Hr(d->SetIndices(nullptr),"unbind indices");vb->Release();ib->Release();
  frameId=330;RetireSurfaceResources();Check(surfaceMeshes.empty()&&surfaceMaterials.empty()&&surfaceMeshBytes==0,"all API resources retired");
  d->Release();d3d->Release();DestroyWindow(hwnd);
  printf("{\"status\":\"PASS\",\"checks\":%u,\"nativeSourceChecks\":%u,\"submitted\":%u,\"rejected\":%u,\"meshCreates\":%u,\"materialCreates\":%u,\"assetBytes\":%zu}\n",checks,nativeSourceChecks,materialChannelsSubmitted,materialChannelsRejected,surfaceMeshCreates,surfaceMaterialCreates,material_channels::assetBytes);
}
