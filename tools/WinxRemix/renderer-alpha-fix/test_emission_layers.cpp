// Own paired linear-emission source: zero albedo and black background. No game assets.
#include "fixture_helpers.h"
#include "winx_surface_instance.h"
int main(int argc,char** argv) {
  if(!OpenJournal())return 1;
  const bool system=argc==2&&strcmp(argv[1],"--system")==0;
  if(argc!=1&&!system)Fail("arguments",0);
  SetProcessDPIAware();WNDCLASSW wc{};wc.lpfnWndProc=WindowProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"WinxFloorBlendFixture";
  if(!RegisterClassW(&wc))Fail("window class",GetLastError());
  RECT size{0,0,960,540};AdjustWindowRect(&size,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,FALSE);
  window=CreateWindowW(wc.lpszClassName,L"Winx opacity fixture - owned synthetic panels",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,
    40,40,size.right-size.left,size.bottom-size.top,nullptr,nullptr,wc.hInstance,nullptr);
  if(!window)Fail("window",GetLastError());ShowWindow(window,SW_SHOW);ShowWindow(window,SW_SHOW);Focus();
  auto runtime=system?LoadLibraryExW(L"d3d9.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32):LoadLibraryW(L".\\d3d9.dll");
  if(!runtime)Fail("runtime",GetLastError());
  auto factory=reinterpret_cast<IDirect3D9*(WINAPI*)(UINT)>(GetProcAddress(runtime,"Direct3DCreate9"));if(!factory)Fail("factory",0);
  auto d3d=factory(D3D_SDK_VERSION);if(!d3d)Fail("D3D",0);
  D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;
  pp.BackBufferWidth=960;pp.BackBufferHeight=540;pp.BackBufferFormat=D3DFMT_X8R8G8B8;pp.EnableAutoDepthStencil=TRUE;pp.AutoDepthStencilFormat=D3DFMT_D24S8;pp.PresentationInterval=D3DPRESENT_INTERVAL_ONE;
  Hr(d3d->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,&device),"device");
  if(!system){
    auto initialize=reinterpret_cast<PFN_remixapi_InitializeLibrary>(GetProcAddress(runtime,"remixapi_InitializeLibrary"));if(!initialize)Fail("Remix entry",0);
    remixapi_InitializeLibraryInfo init{};init.sType=REMIXAPI_STRUCT_TYPE_INITIALIZE_LIBRARY_INFO;
    init.version=REMIXAPI_VERSION_MAKE(REMIXAPI_VERSION_MAJOR,REMIXAPI_VERSION_MINOR,REMIXAPI_VERSION_PATCH);Api(initialize(&init,&api),"API initialize");
  }
  D3DMATRIX identity{},projection{};identity._11=identity._22=identity._33=identity._44=1;
  projection._11=1;projection._22=960.f/540;projection._33=100.f/99.9f;projection._34=1;projection._43=-.1f*100/99.9f;
  Hr(device->SetTransform(D3DTS_WORLD,&identity),"world");Hr(device->SetTransform(D3DTS_VIEW,&identity),"view");Hr(device->SetTransform(D3DTS_PROJECTION,&projection),"projection");Hr(device->SetFVF(fvf),"FVF");
  for(auto state:{D3DRS_LIGHTING,D3DRS_SPECULARENABLE,D3DRS_ALPHATESTENABLE,D3DRS_FOGENABLE,D3DRS_SRGBWRITEENABLE,D3DRS_SEPARATEALPHABLENDENABLE})Hr(device->SetRenderState(state,0),"disable state");
  Hr(device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE),"cull");Hr(device->SetRenderState(D3DRS_ZENABLE,TRUE),"depth");Hr(device->SetRenderState(D3DRS_ZWRITEENABLE,TRUE),"write depth");Hr(device->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL),"depth compare");
  Hr(device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA),"source blend");Hr(device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA),"destination blend");Hr(device->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD),"blend op");
  Hr(device->SetRenderState(D3DRS_TEXTUREFACTOR,0xffffffff),"texture factor");
  IDirect3DTexture9* texture=nullptr;Hr(device->CreateTexture(4,4,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture,nullptr),"white texture");
  D3DLOCKED_RECT lock{};Hr(texture->LockRect(0,&lock,nullptr,0),"texture lock");
  for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x)reinterpret_cast<DWORD*>(static_cast<char*>(lock.pBits)+y*lock.Pitch)[x]=0xffffffff;
  Hr(texture->UnlockRect(0),"texture unlock");Hr(device->SetTexture(0,texture),"texture bind");
  for(auto state:{D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER})Hr(device->SetSamplerState(0,state,D3DTEXF_POINT),"point sampler");
  for(auto state:{D3DTSS_COLOROP,D3DTSS_ALPHAOP})Hr(device->SetTextureStageState(0,state,D3DTOP_MODULATE),"modulate RGBA");
  for(auto state:{D3DTSS_COLORARG1,D3DTSS_ALPHAARG1})Hr(device->SetTextureStageState(0,state,D3DTA_TEXTURE),"texture RGBA");
  for(auto state:{D3DTSS_COLORARG2,D3DTSS_ALPHAARG2})Hr(device->SetTextureStageState(0,state,D3DTA_DIFFUSE),"vertex RGBA");
  Hr(device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE),"single stage");
  if(system)Fail("This is a Remix-only paired emission fixture",0);
  struct Layer {float r,g,b;unsigned alpha;};
  const std::vector<Layer> groups[]={
    {{.2f,.1f,.05f,216}},
    {{.2f,.1f,.05f,128},{.01f,.02f,.08f,255}},
    {{.25f,0,0,128},{0,.3f,0,64},{0,0,.1f,255}},
    {{.05f,.01f,0,229},{0,.2f,.01f,229},{.01f,0,.3f,229}}
  };
  struct Draw {remixapi_MeshHandle mesh;remixapi_MaterialHandle material;SurfaceInstanceState state;};
  std::vector<Draw> draws;
  surface_material::Contract contract{};
  if(!surface_material::Read(device,contract))Fail("FFP material contract",0);
  auto add=[&](unsigned panel,const Layer& layer,float depth){
    const auto key=uint64_t(draws.size()+1);
    remixapi_MaterialInfoOpaqueEXT opaque{};opaque.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_OPAQUE_EXT;
    opaque.albedoConstant={0,0,0};opaque.opacityConstant=1;opaque.roughnessConstant=.5f;opaque.useDrawCallAlphaState=1;
    remixapi_MaterialInfo info{};info.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO;info.pNext=&opaque;
    // Pinned Remix converts this authored constant with pow(color, 2.2).
    // Layer values and the analytic accumulation are linear radiance.
    const auto gamma=[](float linear){return std::pow(linear,1.f/2.2f);};
    info.hash=0x57454d49544c0000ull+key;info.emissiveIntensity=1;info.emissiveColorConstant={gamma(layer.r),gamma(layer.g),gamma(layer.b)};
    remixapi_MaterialHandle material=nullptr;Api(api.CreateMaterial(&info,&material),"create material");
    const float x=-3.f+float(panel%4)*2,y=panel<4?1.15f:-1.15f;
    remixapi_HardcodedVertex v[4]{};
    for(unsigned i=0;i<4;++i){
      v[i].position[0]=(x+(i&1?.7f:-.7f))*depth/5;
      v[i].position[1]=(y+(i&2?-.7f:.7f))*depth/5;v[i].position[2]=depth;
      v[i].normal[2]=-1;v[i].color=(layer.alpha<<24)|0xffffff;
      v[i].texcoord[0]=float(i&1);v[i].texcoord[1]=float((i>>1)&1);
    }
    const uint32_t ix[]={0,1,2,2,1,3};
    remixapi_MeshInfoSurfaceTriangles surface{};surface.vertices_values=v;surface.vertices_count=4;
    surface.indices_values=ix;surface.indices_count=6;surface.material=material;
    remixapi_MeshInfo mesh{};mesh.sType=REMIXAPI_STRUCT_TYPE_MESH_INFO;mesh.hash=0x57454d49544d0000ull+key;
    mesh.surfaces_values=&surface;mesh.surfaces_count=1;remixapi_MeshHandle handle=nullptr;
    Api(api.CreateMesh(&mesh,&handle),"create mesh");
    SurfaceInstanceState state{D3DCULL_NONE,TRUE,D3DCMP_ALWAYS,0,15,layer.alpha<255?1u:0u,D3DBLEND_SRCALPHA,D3DBLEND_INVSRCALPHA,D3DBLENDOP_ADD,true};
    draws.push_back({handle,material,state});
  };
  for(unsigned group=0;group<4;++group){
    Layer expected{0,0,0,255};float transmission=1;
    for(unsigned i=0;i<groups[group].size();++i){
      const auto& layer=groups[group][i];const float alpha=float(layer.alpha)/255;
      expected.r+=transmission*alpha*layer.r;expected.g+=transmission*alpha*layer.g;expected.b+=transmission*alpha*layer.b;
      transmission*=1-alpha;add(group*2,layer,4.2f+.15f*i);
    }
    add(group*2+1,expected,4.2f);
    fprintf(journal,"{\"event\":\"pair\",\"index\":%u,\"sourcePanel\":%u,\"referencePanel\":%u,\"layers\":%zu,\"expectedEmission\":[%.9g,%.9g,%.9g],\"transmission\":%.9g}\n",group,group*2,group*2+1,groups[group].size(),expected.r,expected.g,expected.b,transmission);
  }
  fflush(journal);
  const Vertex background[]={{-10,6,6,0,0,-1,0xff000000,0,0},{10,6,6,0,0,-1,0xff000000,1,0},
    {-10,-6,6,0,0,-1,0xff000000,0,1},{10,-6,6,0,0,-1,0xff000000,1,1}};
  auto begin=[&](){MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){if(message.message==WM_QUIT)Fail("interrupted",0);TranslateMessage(&message);DispatchMessageW(&message);}
    Hr(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff000000,1,0),"clear");Hr(device->BeginScene(),"begin");
    Hr(device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE),"background blend");Hr(device->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE),"background test");
    Hr(device->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST,0,4,2,indices,D3DFMT_INDEX16,background,sizeof(Vertex)),"background and camera");};
  const char* captions[]={"forward.bmp","reverse.bmp"};
  for(unsigned phase=0;phase<2;++phase){
    Focus();const auto started=GetTickCount64();unsigned phaseFrames=0;
    do{begin();
      for(size_t i=0;i<draws.size();++i){const auto& draw=draws[phase?draws.size()-1-i:i];
        remixapi_InstanceInfoBlendEXT blend{};remixapi_InstanceInfo instance{};
        if(!DescribeSurfaceInstance(draw.state,contract,identity,draw.mesh,blend,instance))Fail("instance",unsigned(i));
        Api(api.DrawInstance(&instance),"draw");
      }
      Hr(device->EndScene(),"end");Hr(device->Present(nullptr,nullptr,nullptr,nullptr),"present");++frame;++phaseFrames;
    }while(phaseFrames<180||GetTickCount64()-started<5000);
    Capture(captions[phase]);
  }
  for(const auto& draw:draws)Api(api.DestroyMesh(draw.mesh),"destroy mesh");
  for(const auto& draw:draws)Api(api.DestroyMaterial(draw.material),"destroy material");
  // Complete a real device event after the queued draws and retire commands.
  // API success alone only proves that the bridge accepted those commands.
  IDirect3DQuery9* completion=nullptr;
  Hr(device->CreateQuery(D3DQUERYTYPE_EVENT,&completion),"completion query");
  Hr(completion->Issue(D3DISSUE_END),"completion issue");
  const auto completionStarted=GetTickCount64();unsigned completionPolls=0;
  for(;;){
    BOOL done=FALSE;const auto result=completion->GetData(&done,sizeof(done),D3DGETDATA_FLUSH);++completionPolls;
    Hr(result,"completion read");if(result==S_OK&&done)break;
    if(GetTickCount64()-completionStarted>10000)Fail("completion timeout",completionPolls);
    MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){
      if(message.message==WM_QUIT)Fail("interrupted completion",0);TranslateMessage(&message);DispatchMessageW(&message);
    }
    Sleep(1);
  }
  completion->Release();
  fprintf(journal,"{\"event\":\"deviceCompletion\",\"milliseconds\":%llu,\"polls\":%u}\n",GetTickCount64()-completionStarted,completionPolls);fflush(journal);
  Hr(device->SetTexture(0,nullptr),"unbind");texture->Release();device->Release();d3d->Release();DestroyWindow(window);
  fprintf(journal,"{\"event\":\"complete\",\"frames\":%u,\"meshesRetired\":%zu,\"materialsRetired\":%zu}\n",frame,draws.size(),draws.size());fclose(journal);return 0;
}
