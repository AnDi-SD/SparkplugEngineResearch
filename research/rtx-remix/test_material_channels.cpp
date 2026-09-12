// Own integration test: real system D3D9 objects and production draw/texture
// hooks, recording Remix API. GPU channel values are tested by the stock fixture.
#define WINX_REMIX_TEST
#include "winx_d3d9_probe.cpp"
#include <cstdlib>
static unsigned checks,apiDraws;
static uintptr_t nextHandle=1;
static std::vector<remixapi_HardcodedVertex> lastVertices;
static std::wstring lastAlbedo,lastEmission;
static std::map<remixapi_MaterialHandle,std::pair<std::wstring,std::wstring>> recordedMaterials;
static std::map<remixapi_MeshHandle,remixapi_MaterialHandle> recordedMeshes;
static remixapi_InstanceInfoBlendEXT lastBlend{};
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
  *out=reinterpret_cast<remixapi_MeshHandle>(nextHandle++);recordedMeshes[*out]=s.material;return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL Instance(const remixapi_InstanceInfo* info) {
  ++apiDraws;Check(info->categoryFlags==0,"ordinary material is not a decal");
  lastBlend=*static_cast<const remixapi_InstanceInfoBlendEXT*>(info->pNext);
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
  printf("{\"status\":\"PASS\",\"checks\":%u,\"submitted\":%u,\"rejected\":%u,\"meshCreates\":%u,\"materialCreates\":%u,\"assetBytes\":%zu}\n",checks,materialChannelsSubmitted,materialChannelsRejected,surfaceMeshCreates,surfaceMaterialCreates,material_channels::assetBytes);
}
