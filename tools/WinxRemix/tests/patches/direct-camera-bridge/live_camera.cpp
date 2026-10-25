// Own bounded D3D bootstrap / direct camera integration experiment. No game data.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <vector>
#define REMIX_ALLOW_X86
#include <remix/remix_c.h>

static FILE* logFile;
static unsigned frame;
static LARGE_INTEGER frequency;
static ULONGLONG started;
static remixapi_Interface api{};
static void fail(const char* where,unsigned result) {
  fprintf(logFile,"{\"event\":\"error\",\"where\":\"%s\",\"result\":%u,\"frame\":%u}\n",where,result,frame);fflush(logFile);std::exit(1);
}
static void hr(HRESULT value,const char* where) {if(FAILED(value))fail(where,unsigned(value));}
static void code(remixapi_ErrorCode value,const char* where) {if(value)fail(where,unsigned(value));}
static LRESULT CALLBACK windowProc(HWND h,UINT message,WPARAM w,LPARAM l) {return DefWindowProcW(h,message,w,l);}
static remixapi_CameraInfo camera(float x) {
  remixapi_CameraInfo c{};c.sType=REMIXAPI_STRUCT_TYPE_CAMERA_INFO;c.type=REMIXAPI_CAMERA_TYPE_WORLD;
  c.view[0][0]=c.view[1][1]=c.view[2][2]=c.view[3][3]=1;c.view[3][0]=-x;
  c.projection[0][0]=c.projection[1][1]=1.7320508f;c.projection[2][2]=100.0f/99.9f;
  c.projection[2][3]=1;c.projection[3][2]=-.1f*100/99.9f;return c;
}
static double setup(const remixapi_CameraInfo* c,const char* label,remixapi_ErrorCode expected) {
  LARGE_INTEGER a{},b{};QueryPerformanceCounter(&a);const auto result=api.SetupCamera(c);QueryPerformanceCounter(&b);
  const double ms=1000.0*double(b.QuadPart-a.QuadPart)/double(frequency.QuadPart);
  fprintf(logFile,"{\"event\":\"camera\",\"label\":\"%s\",\"frame\":%u,\"result\":%u,\"expected\":%u,\"latencyMs\":%.6f,\"view\":[",label,frame,unsigned(result),unsigned(expected),ms);
  for(unsigned i=0;i<16;++i)fprintf(logFile,"%s%.9g",i?",":"",c?c->view[i/4][i%4]:0);
  fprintf(logFile,"]}\n");fflush(logFile);
  if(result!=expected)fail(label,unsigned(result));return ms;
}

int main(int argc,char** argv) {
  bool visible=false,fault=false;
  for(int i=1;i<argc;++i) {if(!strcmp(argv[i],"--visible"))visible=true;else if(!strcmp(argv[i],"--fault"))fault=true;else return 2;}
  if(fopen_s(&logFile,"helper.jsonl","wb")||!logFile)return 1;
  started=GetTickCount64();QueryPerformanceFrequency(&frequency);SetProcessDPIAware();
  fprintf(logFile,"{\"event\":\"start\",\"pid\":%lu,\"visible\":%s,\"fault\":%s,\"framesLimit\":20,\"budgetMs\":25000}\n",GetCurrentProcessId(),visible?"true":"false",fault?"true":"false");fflush(logFile);
  WNDCLASSW wc{};wc.lpfnWndProc=windowProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"DirectCameraBridgeFixture";
  if(!RegisterClassW(&wc))fail("RegisterClass",GetLastError());
  RECT r{0,0,256,256};const DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU;AdjustWindowRect(&r,style,FALSE);
  HWND window=CreateWindowW(wc.lpszClassName,L"Remix direct camera test",style,40,40,r.right-r.left,r.bottom-r.top,nullptr,nullptr,wc.hInstance,nullptr);
  if(!window)fail("CreateWindow",GetLastError());if(visible)ShowWindow(window,SW_SHOWNOACTIVATE);
  auto runtime=LoadLibraryW(L".\\d3d9.dll");if(!runtime)fail("LoadLibrary",GetLastError());
  auto factory=reinterpret_cast<IDirect3D9*(WINAPI*)(UINT)>(GetProcAddress(runtime,"Direct3DCreate9"));
  auto initialize=reinterpret_cast<PFN_remixapi_InitializeLibrary>(GetProcAddress(runtime,"remixapi_InitializeLibrary"));
  if(!factory||!initialize)fail("exports",0);
  auto d3d=factory(D3D_SDK_VERSION);if(!d3d)fail("D3D factory",0);
  remixapi_InitializeLibraryInfo info{};info.sType=REMIXAPI_STRUCT_TYPE_INITIALIZE_LIBRARY_INFO;
  info.version=REMIXAPI_VERSION_MAKE(REMIXAPI_VERSION_MAJOR,REMIXAPI_VERSION_MINOR,REMIXAPI_VERSION_PATCH);
  code(initialize(&info,&api),"InitializeLibrary");
  fprintf(logFile,"{\"event\":\"capabilities\",\"setupCamera\":%s,\"present\":%s}\n",api.SetupCamera?"true":"false",api.Present?"true":"false");fflush(logFile);
  if(!api.SetupCamera||!api.SetConfigVariable||!api.CreateMaterial||!api.CreateMesh||!api.DrawInstance)fail("API capability",0);
  auto current=camera(0);
  // Valid payload, no registered D3D device: this error must travel from server.
  setup(&current,"before-device",REMIXAPI_ERROR_CODE_REMIX_DEVICE_WAS_NOT_REGISTERED);
  setup(nullptr,"local-null",REMIXAPI_ERROR_CODE_INVALID_ARGUMENTS);
  auto invalid=current;invalid.pNext=&invalid;setup(&invalid,"local-extension",REMIXAPI_ERROR_CODE_INVALID_ARGUMENTS);
  invalid=current;invalid.sType=REMIXAPI_STRUCT_TYPE_NONE;setup(&invalid,"local-stype",REMIXAPI_ERROR_CODE_INVALID_ARGUMENTS);
  setup(&current,"after-local-errors",REMIXAPI_ERROR_CODE_REMIX_DEVICE_WAS_NOT_REGISTERED);
  // Losing a mandatory transport reply does not require GPU/device startup.
  // Test the same submitted camera command before device registration.
  if(fault) {
    FILE* ready=nullptr;fopen_s(&ready,"fault.ready","wb");if(!ready)fail("fault marker",0);fclose(ready);
    while(GetFileAttributesW(L"fault.proceed")==INVALID_FILE_ATTRIBUTES) {
      if(GetTickCount64()-started>25000)fail("fault gate timeout",0);Sleep(5);
    }
    fprintf(logFile,"{\"event\":\"fault-camera-call\"}\n");fflush(logFile);
    api.SetupCamera(&current);
    fail("fatal transport unexpectedly returned",0);
  }
  D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;
  pp.BackBufferWidth=pp.BackBufferHeight=256;pp.BackBufferFormat=D3DFMT_X8R8G8B8;pp.EnableAutoDepthStencil=TRUE;
  pp.AutoDepthStencilFormat=D3DFMT_D24S8;pp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
  IDirect3DDevice9* device=nullptr;hr(d3d->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,&device),"CreateDevice");
  fprintf(logFile,"{\"event\":\"device-created\"}\n");fflush(logFile);
  remixapi_MaterialInfoOpaqueEXT opaque{};opaque.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_OPAQUE_EXT;
  opaque.albedoConstant={.3f,.6f,.9f};opaque.opacityConstant=1;opaque.roughnessConstant=.8f;
  remixapi_MaterialInfo materialInfo{};materialInfo.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO;materialInfo.pNext=&opaque;
  materialInfo.hash=0x43414d4552410001ull;materialInfo.emissiveColorConstant={.3f,.6f,.9f};materialInfo.emissiveIntensity=2;
  remixapi_MaterialHandle material=nullptr;code(api.CreateMaterial(&materialInfo,&material),"CreateMaterial");
  remixapi_HardcodedVertex vertices[3]{};const float positions[3][3]={{-.7f,-.6f,4},{0,.7f,4},{.7f,-.6f,4}};
  for(unsigned i=0;i<3;++i){memcpy(vertices[i].position,positions[i],12);vertices[i].normal[2]=-1;vertices[i].color=0xffffffff;}
  const uint32_t indices[]={0,1,2};remixapi_MeshInfoSurfaceTriangles surface{};
  surface.vertices_values=vertices;surface.vertices_count=3;surface.indices_values=indices;surface.indices_count=3;surface.material=material;
  remixapi_MeshInfo meshInfo{};meshInfo.sType=REMIXAPI_STRUCT_TYPE_MESH_INFO;meshInfo.hash=0x43414d4552410002ull;meshInfo.surfaces_values=&surface;meshInfo.surfaces_count=1;
  remixapi_MeshHandle mesh=nullptr;code(api.CreateMesh(&meshInfo,&mesh),"CreateMesh");
  remixapi_InstanceInfo instance{};instance.sType=REMIXAPI_STRUCT_TYPE_INSTANCE_INFO;instance.mesh=mesh;instance.doubleSided=TRUE;
  instance.transform.matrix[0][0]=instance.transform.matrix[1][1]=instance.transform.matrix[2][2]=1;
  std::vector<double> latency;
  for(frame=0;frame<20;++frame) {
    if(GetTickCount64()-started>25000)fail("frame deadline",0);
    MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
    current=camera(-.25f+.5f*float(frame)/19);
    hr(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff101820,1,0),"Clear");hr(device->BeginScene(),"BeginScene");
    // First main camera update of this frame, before any captured/API draw.
    latency.push_back(setup(&current,"world-frame",REMIXAPI_ERROR_CODE_SUCCESS));
    code(api.DrawInstance(&instance),"DrawInstance-after-camera");
    code(api.SetConfigVariable("rtx.enableNearPlaneOverride","False"),"next-config-command");
    hr(device->EndScene(),"EndScene");hr(device->Present(nullptr,nullptr,nullptr,nullptr),"Present");
    fprintf(logFile,"{\"event\":\"present\",\"frame\":%u}\n",frame);fflush(logFile);Sleep(16);
  }
  code(api.DestroyMesh(mesh),"DestroyMesh");code(api.DestroyMaterial(material),"DestroyMaterial");
  device->Release();d3d->Release();FreeLibrary(runtime);DestroyWindow(window);
  double total=0;for(double ms:latency)total+=ms;std::sort(latency.begin(),latency.end());
  fprintf(logFile,"{\"event\":\"complete\",\"frames\":%u,\"elapsedMs\":%llu,\"cameraMeanMs\":%.6f,\"cameraP95Ms\":%.6f,\"cameraMaxMs\":%.6f}\n",frame,GetTickCount64()-started,total/latency.size(),latency[18],latency.back());
  fclose(logFile);return 0;
}
