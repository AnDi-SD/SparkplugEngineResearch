// Own bounded graphics experiment. Runs against an isolated, unmodified RTX
// Remix runtime or system D3D9; no game classes, game assets or scene capture.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <vector>
#include <cmath>
#define REMIX_ALLOW_X86
#include <remix/remix_c.h>
#include "winx_surface_material.h"
static FILE* journal;
static IDirect3DDevice9* device;
static HWND window;
static remixapi_Interface api{};
static unsigned frame;
static void Fail(const char* what,unsigned value) {
  fprintf(journal,"{\"event\":\"error\",\"what\":\"%s\",\"value\":%u,\"frame\":%u}\n",what,value,frame);
  fflush(journal);std::exit(1);
}
static void Hr(HRESULT value,const char* what) {if(FAILED(value))Fail(what,unsigned(value));}
static void Api(remixapi_ErrorCode value,const char* what) {if(value)Fail(what,unsigned(value));}
static void Config(const char* key,const char* value) {
  Api(api.SetConfigVariable(key,value),key);
  fprintf(journal,"{\"event\":\"config\",\"key\":\"%s\",\"value\":\"%s\",\"frame\":%u}\n",key,value,frame);fflush(journal);
}
static LRESULT CALLBACK WindowProc(HWND h,UINT m,WPARAM w,LPARAM l) {
  if(m==WM_CLOSE){PostQuitMessage(0);return 0;}return DefWindowProcW(h,m,w,l);
}
static void Capture(const char* name) {
  if(GetForegroundWindow()!=window)Fail("fixture lost foreground",0);
  RECT r{};if(!GetClientRect(window,&r))Fail("client rectangle",GetLastError());
  const int width=r.right,height=r.bottom;
  if(width!=960 || height!=540)Fail("unexpected client dimensions",0);
  // DWM's per-window redirection bitmap can remain stale with Vulkan presents.
  // Copy only this foreground window's client rectangle from the desktop DC.
  POINT origin{};if(!ClientToScreen(window,&origin))Fail("client origin",GetLastError());
  HDC source=GetDC(nullptr),memory=CreateCompatibleDC(source);
  BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
  void* pixels=nullptr;HBITMAP bitmap=CreateDIBSection(source,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
  if(!source || !memory || !bitmap || !pixels)Fail("capture allocation",GetLastError());
  auto previous=SelectObject(memory,bitmap);
  if(!BitBlt(memory,0,0,width,height,source,origin.x,origin.y,SRCCOPY))Fail("capture BitBlt",GetLastError());
  GdiFlush();
  BITMAPFILEHEADER header{};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);
  header.bfSize=header.bfOffBits+DWORD(width*height*4);
  FILE* output=nullptr;if(fopen_s(&output,name,"wb") || !output)Fail("capture file",0);
  if(fwrite(&header,sizeof(header),1,output)!=1 || fwrite(&info.bmiHeader,sizeof(BITMAPINFOHEADER),1,output)!=1 ||
     fwrite(pixels,size_t(width)*height*4,1,output)!=1)Fail("capture write",0);
  fclose(output);SelectObject(memory,previous);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(nullptr,source);
  fprintf(journal,"{\"event\":\"capture\",\"file\":\"%s\",\"frame\":%u}\n",name,frame);fflush(journal);
}
struct Vertex {float x,y,z,nx,ny,nz;DWORD color;float u,v;};
static constexpr DWORD fvf=D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_DIFFUSE|D3DFVF_TEX1;
static Vertex vertices[8][4];
static const WORD indices[]={0,1,2,2,1,3};
static void RenderPanel(unsigned panel,DWORD color,bool lighting,bool additive,bool writeDepth=true) {
  for(auto& v:vertices[panel])v.color=color;
  Hr(device->SetRenderState(D3DRS_LIGHTING,lighting),"lighting");
  Hr(device->SetRenderState(D3DRS_ALPHABLENDENABLE,additive),"blend enable");
  Hr(device->SetRenderState(D3DRS_ZWRITEENABLE,writeDepth),"depth write");
  Hr(device->SetRenderState(D3DRS_ZFUNC,writeDepth?D3DCMP_LESSEQUAL:D3DCMP_EQUAL),"depth compare");
  Hr(device->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST,0,4,2,indices,D3DFMT_INDEX16,vertices[panel],sizeof(Vertex)),"panel draw");
}
int main(int argc,char** argv) {
  if(fopen_s(&journal,"fixture.jsonl","wb") || !journal)return 1;
  const bool system=argc==2 && strcmp(argv[1],"--system")==0;
  if(argc>2 || (argc==2 && !system))Fail("arguments",0);
  SetProcessDPIAware();
  WNDCLASSW wc{};wc.lpfnWndProc=WindowProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"WinxRemixMaterialFixture";
  if(!RegisterClassW(&wc))Fail("window class",GetLastError());
  RECT size{0,0,960,540};AdjustWindowRect(&size,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,FALSE);
  window=CreateWindowW(wc.lpszClassName,system?L"Winx material fixture - system D3D9":L"Winx material fixture - stock RTX Remix",
    WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,40,40,size.right-size.left,size.bottom-size.top,nullptr,nullptr,wc.hInstance,nullptr);
  if(!window)Fail("window creation",GetLastError());ShowWindow(window,SW_SHOW);SetForegroundWindow(window);
  auto runtime=system?LoadLibraryExW(L"d3d9.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32):LoadLibraryW(L".\\d3d9.dll");
  if(!runtime)Fail("runtime load",GetLastError());
  auto factory=reinterpret_cast<IDirect3D9*(WINAPI*)(UINT)>(GetProcAddress(runtime,"Direct3DCreate9"));
  if(!factory)Fail("D3D9 entry",0);auto d3d=factory(D3D_SDK_VERSION);if(!d3d)Fail("D3D9 factory",0);
  D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;
  pp.BackBufferWidth=960;pp.BackBufferHeight=540;pp.BackBufferFormat=D3DFMT_X8R8G8B8;
  pp.EnableAutoDepthStencil=TRUE;pp.AutoDepthStencilFormat=D3DFMT_D24S8;pp.PresentationInterval=D3DPRESENT_INTERVAL_ONE;
  Hr(d3d->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,&device),"device");
  if(!system) {
    auto initialize=reinterpret_cast<PFN_remixapi_InitializeLibrary>(GetProcAddress(runtime,"remixapi_InitializeLibrary"));
    if(!initialize)Fail("Remix entry",0);
    remixapi_InitializeLibraryInfo init{};init.sType=REMIXAPI_STRUCT_TYPE_INITIALIZE_LIBRARY_INFO;
    init.version=REMIXAPI_VERSION_MAKE(REMIXAPI_VERSION_MAJOR,REMIXAPI_VERSION_MINOR,REMIXAPI_VERSION_PATCH);
    Api(initialize(&init,&api),"initialize Remix API");
    if(!api.SetConfigVariable || !api.CreateMaterial || !api.CreateMesh || !api.DrawInstance || !api.CreateLight || !api.DrawLightInstance)Fail("API availability",0);
  }
  D3DMATRIX identity{};identity._11=identity._22=identity._33=identity._44=1;
  D3DMATRIX projection{};projection._11=1.0f;projection._22=960.0f/540;projection._33=100.0f/99.9f;projection._34=1;projection._43=-.1f*100/99.9f;
  Hr(device->SetTransform(D3DTS_WORLD,&identity),"world");Hr(device->SetTransform(D3DTS_VIEW,&identity),"view");Hr(device->SetTransform(D3DTS_PROJECTION,&projection),"projection");
  Hr(device->SetFVF(fvf),"FVF");
  for(auto state:{D3DRS_SPECULARENABLE,D3DRS_ALPHATESTENABLE,D3DRS_FOGENABLE,D3DRS_SRGBWRITEENABLE})Hr(device->SetRenderState(state,0),"disable state");
  Hr(device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE),"cull");Hr(device->SetRenderState(D3DRS_ZENABLE,TRUE),"depth");
  Hr(device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_ONE),"src blend");Hr(device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_ONE),"dst blend");
  Hr(device->SetRenderState(D3DRS_AMBIENT,0),"ambient");Hr(device->SetRenderState(D3DRS_COLORVERTEX,FALSE),"material source");
  D3DMATERIAL9 material{};material.Diffuse={.6f,.4f,.2f,1};material.Emissive={.8f,.3f,.1f,0};
  Hr(device->SetMaterial(&material),"material");
  IDirect3DTexture9* texture=nullptr;Hr(device->CreateTexture(4,4,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture,nullptr),"texture");
  D3DLOCKED_RECT lock{};Hr(texture->LockRect(0,&lock,nullptr,0),"texture lock");
  for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x)static_cast<DWORD*>(static_cast<void*>(static_cast<char*>(lock.pBits)+y*lock.Pitch))[x]=0xff80c040;
  Hr(texture->UnlockRect(0),"texture unlock");Hr(device->SetTexture(0,texture),"texture bind");
  Hr(device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE),"color op");
  Hr(device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE),"color texture");Hr(device->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE),"color diffuse");
  Hr(device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1),"alpha op");Hr(device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE),"alpha texture");
  Hr(device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE),"next stage");
  for(unsigned p=0;p<8;++p) {
    const float x=-3.0f+float(p%4)*2.0f,y=p<4?1.15f:-1.15f;
    vertices[p][0]={x-.7f,y+.7f,5,0,0,-1,0xff996633,0,0};vertices[p][1]={x+.7f,y+.7f,5,0,0,-1,0xff996633,1,0};
    vertices[p][2]={x-.7f,y-.7f,5,0,0,-1,0xff996633,0,1};vertices[p][3]={x+.7f,y-.7f,5,0,0,-1,0xff996633,1,1};
  }
  remixapi_MaterialHandle apiMaterial=nullptr;remixapi_MeshHandle apiMesh=nullptr;remixapi_LightHandle light=nullptr;
  // Ensure the stock renderer has injected and initialized a scene before API
  // resource creation. Successful transport alone is not a GPU readiness signal.
  for(unsigned warmup=0;warmup<60;++warmup) {
    MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {if(message.message==WM_QUIT)Fail("closed during warmup",0);TranslateMessage(&message);DispatchMessageW(&message);}
    Hr(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff080808,1,0),"warmup clear");Hr(device->BeginScene(),"warmup begin");
    RenderPanel(0,0xff996633,false,false);Hr(device->EndScene(),"warmup end");Hr(device->Present(nullptr,nullptr,nullptr,nullptr),"warmup present");++frame;
  }
  if(!system) {
    remixapi_MaterialInfoOpaqueEXT opaque{};opaque.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_OPAQUE_EXT;opaque.albedoConstant={.3f,.5f,.7f};opaque.opacityConstant=1;opaque.roughnessConstant=1;opaque.useDrawCallAlphaState=1;opaque.alphaTestType=7;
    remixapi_MaterialInfo info{};info.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO;info.pNext=&opaque;info.hash=0x57584d46544d0001ull;
    info.emissiveColorConstant={.8f,.3f,.1f};info.emissiveIntensity=1;Api(api.CreateMaterial(&info,&apiMaterial),"API material");
    remixapi_HardcodedVertex meshVertices[4]{};uint32_t meshIndices[]={0,1,2,2,1,3};
    for(unsigned i=0;i<4;++i){memcpy(meshVertices[i].position,&vertices[7][i].x,12);meshVertices[i].normal[2]=-1;meshVertices[i].color=0xff996633;}
    remixapi_MeshInfoSurfaceTriangles surface{};surface.vertices_values=meshVertices;surface.vertices_count=4;surface.indices_values=meshIndices;surface.indices_count=6;surface.material=apiMaterial;
    remixapi_MeshInfo mesh{};mesh.sType=REMIXAPI_STRUCT_TYPE_MESH_INFO;mesh.hash=0x57584d46544d0002ull;mesh.surfaces_values=&surface;mesh.surfaces_count=1;
    Api(api.CreateMesh(&mesh,&apiMesh),"API mesh");
    if(!apiMaterial || !apiMesh)Fail("API returned null handle",0);
    fprintf(journal,"{\"event\":\"api_resources\",\"material\":%u,\"mesh\":%u}\n",unsigned(reinterpret_cast<uintptr_t>(apiMaterial)),unsigned(reinterpret_cast<uintptr_t>(apiMesh)));fflush(journal);
    remixapi_LightInfoDistantEXT distant{};distant.sType=REMIXAPI_STRUCT_TYPE_LIGHT_INFO_DISTANT_EXT;distant.direction={0,0,1};distant.angularDiameterDegrees=.53f;distant.volumetricRadianceScale=1;
    remixapi_LightInfo li{};li.sType=REMIXAPI_STRUCT_TYPE_LIGHT_INFO;li.pNext=&distant;li.hash=0x57584d46544c0001ull;li.radiance={2,2,2};Api(api.CreateLight(&li,&light),"API light");
  }
  fprintf(journal,"{\"event\":\"layout\",\"backend\":\"%s\",\"width\":960,\"height\":540,\"panels\":[\"unlit vertex color\",\"unlit white\",\"FFP emissive no lights\",\"additive only\",\"coincident base plus additive\",\"base only\",\"coincident reverse order\",\"API albedo plus emission\"]}\n",system?"system":"stock-remix");fflush(journal);
  struct Case {const char* name;const char* debug;bool alphaEnabled=false;DWORD alphaFunction=D3DCMP_NEVER;bool translate=true;};
  const Case cases[]={{"final","0"},{"albedo","23"},{"vertex-color","18"},{"emissive","30"},{"direct","100"},{"indirect","106"},{"emissive-translation-off","30"},{"final-translation-off","0"},
    {"raw-disabled-never","23",false,D3DCMP_NEVER,false},{"translated-disabled-never","23",false,D3DCMP_NEVER,true},
    {"translated-enabled-never","23",true,D3DCMP_NEVER,true},{"translated-enabled-always","23",true,D3DCMP_ALWAYS,true},
    {"translated-enabled-less","23",true,D3DCMP_LESS,true},{"translated-disabled-less","23",false,D3DCMP_LESS,true},
    {"translated-enabled-greater","23",true,D3DCMP_GREATER,true},{"translated-disabled-less-return","23",false,D3DCMP_LESS,true}};
  const auto started=GetTickCount64();bool stopped=false;
  for(unsigned c=0;c<unsigned(sizeof(cases)/sizeof(*cases))&&!stopped;++c) {
    fprintf(journal,"{\"event\":\"case\",\"index\":%u,\"name\":\"%s\",\"alphaEnabled\":%s,\"alphaFunctionD3D9\":%lu,\"translated\":%s}\n",c,cases[c].name,cases[c].alphaEnabled?"true":"false",cases[c].alphaFunction,cases[c].translate?"true":"false");fflush(journal);
    if(!system){Config("rtx.debugView.debugViewIdx",cases[c].debug);if(c==6)Config("rtx.enableEmissiveBlendEmissiveOverride","False");}
    const auto begin=GetTickCount64();unsigned caseFrames=0;
    do {
      MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){if(message.message==WM_QUIT)stopped=true;TranslateMessage(&message);DispatchMessageW(&message);}
      if(stopped)break;if(GetTickCount64()-started>120000)Fail("bounded run exceeded 120 seconds",0);
      Hr(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff080808,1,0),"clear");Hr(device->BeginScene(),"begin");
      RenderPanel(0,0xff996633,false,false);RenderPanel(1,0xffffffff,false,false);RenderPanel(2,0xffffffff,true,false);
      RenderPanel(3,0xff996633,false,true);RenderPanel(4,0xff996633,false,false);RenderPanel(4,0xffcc4d1a,false,true,false);
      RenderPanel(5,0xff996633,false,false);RenderPanel(6,0xffcc4d1a,false,true);RenderPanel(6,0xff996633,false,false,false);
      if(!system) {
        remixapi_InstanceInfoBlendEXT blend{};blend.sType=REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_BLEND_EXT;blend.writeMask=15;
        blend.alphaTestEnabled=cases[c].alphaEnabled;blend.alphaTestCompareOp=cases[c].alphaFunction-1;blend.alphaTestReferenceValue=128;
        if(cases[c].translate && !surface_material::AlphaTest(cases[c].alphaEnabled,128,cases[c].alphaFunction,blend))Fail("alpha contract",0);
        blend.textureColorOperation=3;blend.textureColorArg1Source=1;blend.textureColorArg2Source=2;
        blend.textureAlphaOperation=1;blend.textureAlphaArg1Source=1;blend.tFactor=0xffffffff;
        remixapi_InstanceInfo instance{};instance.sType=REMIXAPI_STRUCT_TYPE_INSTANCE_INFO;instance.pNext=&blend;instance.mesh=apiMesh;
        instance.transform.matrix[0][0]=instance.transform.matrix[1][1]=instance.transform.matrix[2][2]=1;instance.doubleSided=1;
        Api(api.DrawInstance(&instance),"API instance");Api(api.DrawLightInstance(light),"API light draw");
      } else {
        Hr(device->SetRenderState(D3DRS_ALPHATESTENABLE,cases[c].alphaEnabled),"native alpha enable");
        Hr(device->SetRenderState(D3DRS_ALPHAFUNC,cases[c].alphaFunction),"native alpha comparison");
        Hr(device->SetRenderState(D3DRS_ALPHAREF,128),"native alpha reference");
        RenderPanel(7,0xff996633,false,false);
        Hr(device->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE),"native alpha restore");
      }
      Hr(device->EndScene(),"end");Hr(device->Present(nullptr,nullptr,nullptr,nullptr),"present");++frame;++caseFrames;
    } while(caseFrames<(system?6u:90u) || GetTickCount64()-begin<(system?250ull:3000ull));
    if(!stopped){char name[96]{};sprintf_s(name,"%02u-%s.bmp",c,cases[c].name);Capture(name);}
  }
  if(!system){Api(api.DestroyLight(light),"destroy light");Api(api.DestroyMesh(apiMesh),"destroy mesh");Api(api.DestroyMaterial(apiMaterial),"destroy material");}
  Hr(device->SetTexture(0,nullptr),"unbind texture");texture->Release();device->Release();d3d->Release();DestroyWindow(window);
  fprintf(journal,"{\"event\":\"complete\",\"frames\":%u,\"interrupted\":%s}\n",frame,stopped?"true":"false");fclose(journal);return stopped?2:0;
}
