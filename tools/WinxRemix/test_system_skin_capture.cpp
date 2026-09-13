// Own real system-D3D9 transport test; no original game instructions execute.
#define WINX_REMIX_TEST
#include "winx_d3d9_probe.cpp"
#include <stdexcept>
static void Require(bool condition,const char* why){if(!condition)throw std::runtime_error(why);}
using ReleaseFunction=ULONG(STDMETHODCALLTYPE*)(IDirect3DDevice9*);
static ReleaseFunction beforeWrapper;static unsigned wrapperCalls;
static ULONG STDMETHODCALLTYPE WrappedRelease(IDirect3DDevice9* device){++wrapperCalls;return beforeWrapper(device);}
using ResetFunction=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,D3DPRESENT_PARAMETERS*);
using QueryFunction=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,REFIID,void**);
static ResetFunction beforeReset;static QueryFunction beforeQuery;static unsigned resetCalls,queryCalls;
static HRESULT STDMETHODCALLTYPE WrappedReset(IDirect3DDevice9* device,D3DPRESENT_PARAMETERS* pp){++resetCalls;return beforeReset(device,pp);}
static HRESULT STDMETHODCALLTYPE WrappedQuery(IDirect3DDevice9* device,REFIID iid,void** out){++queryCalls;return beforeQuery(device,iid,out);}
static void State(IDirect3DDevice9* device,const char* phase){const auto t=*reinterpret_cast<void***>(device);
  printf("{\"phase\":\"%s\",\"table\":%llu,\"createVB\":%s,\"createIB\":%s,\"createDeclaration\":%s,\"draw\":%s,\"records\":%zu,\"createdVB\":%u}\n",phase,
    static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(t)),t[26]==reinterpret_cast<void*>(CreateVB)?"true":"false",t[27]==reinterpret_cast<void*>(CreateIB)?"true":"false",
    t[86]==reinterpret_cast<void*>(CreateDeclaration)?"true":"false",t[82]==reinterpret_cast<void*>(DrawIndexed)?"true":"false",native_transport_source::records.size(),native_transport_source::created[0]);fflush(stdout);
}
int main(){HWND window=nullptr;IDirect3D9* factory=nullptr;IDirect3DDevice9* device=nullptr;IDirect3DVertexBuffer9* vertices=nullptr;
  try {
    SetEnvironmentVariableW(L"WINX_REMIX_BACKEND",L"system");factory=ProxyCreate9(D3D_SDK_VERSION);Require(factory!=nullptr,"system factory");
    native_mesh_source::enabled=true;native_transport_source::enabled=true;
    window=CreateWindowW(L"STATIC",L"System skin transport fixture",WS_OVERLAPPED,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);Require(window!=nullptr,"owned hidden window");
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;
    pp.BackBufferWidth=64;pp.BackBufferHeight=64;pp.BackBufferFormat=D3DFMT_X8R8G8B8;pp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    Require(SUCCEEDED(factory->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_MIXED_VERTEXPROCESSING,&pp,&device)),"real mixed VP device");
    State(device,"created");
    const auto table=*reinterpret_cast<void***>(device);
    Require(table[2]!=reinterpret_cast<void*>(DeviceRelease)&&table[16]!=reinterpret_cast<void*>(Reset)&&table[0]!=reinterpret_cast<void*>(TextureDeviceQuery),"system lifetime/alias installation is deferred");
    beforeWrapper=reinterpret_cast<ReleaseFunction>(table[2]);DWORD protection=0,ignored=0;
    beforeReset=reinterpret_cast<ResetFunction>(table[16]);beforeQuery=reinterpret_cast<QueryFunction>(table[0]);
    Require(VirtualProtect(table,17*sizeof(void*),PAGE_READWRITE,&protection)!=FALSE,"owned fixture slots writable");
    InterlockedExchangePointer(table,reinterpret_cast<void*>(WrappedQuery));InterlockedExchangePointer(table+16,reinterpret_cast<void*>(WrappedReset));
    InterlockedExchangePointer(table+2,reinterpret_cast<void*>(WrappedRelease));VirtualProtect(table,17*sizeof(void*),protection,&ignored);
    Require(SUCCEEDED(device->CreateVertexBuffer(256,0,0,D3DPOOL_MANAGED,&vertices,nullptr))&&vertices,"real first VB");State(device,"first VB");
    const bool first=native_transport_source::records.count(vertices)!=0;vertices->Release();vertices=nullptr;
    Require(TransportDeviceHooked(device)&&Original<ReleaseFunction>(device,2)==WrappedRelease,"resource boundary preserves completed outer Release chain");
    Require(Original<ResetFunction>(device,16)==WrappedReset&&Original<QueryFunction>(device,0)==WrappedQuery,"completed Reset/Query chains retained");
    IDirect3DDevice9* queried=nullptr;const auto queries=queryCalls;
    Require(SUCCEEDED(device->QueryInterface(__uuidof(IDirect3DDevice9),reinterpret_cast<void**>(&queried)))&&queried==device&&queryCalls==queries+1,"owned alias traverses wrapper exactly once");queried->Release();
    device->AddRef();const auto releases=wrapperCalls;Require(device->Release()>0&&wrapperCalls==releases+1,"one nonfinal Release traverses preserved wrapper exactly once");
    Require(SUCCEEDED(device->SetSoftwareVertexProcessing(TRUE)),"software VP switch");State(device,"software VP");
    Require(SUCCEEDED(device->CreateVertexBuffer(256,0,0,D3DPOOL_MANAGED,&vertices,nullptr))&&vertices,"real software VB");State(device,"software VB");
    const bool second=native_transport_source::records.count(vertices)!=0;vertices->Release();vertices=nullptr;
    Require(SUCCEEDED(device->SetSoftwareVertexProcessing(FALSE)),"hardware VP switch");State(device,"hardware VP");
    Require(SUCCEEDED(device->CreateVertexBuffer(256,0,0,D3DPOOL_MANAGED,&vertices,nullptr))&&vertices,"real hardware VB");State(device,"hardware VB");
    const bool third=native_transport_source::records.count(vertices)!=0;
    const auto resets=resetCalls;Require(SUCCEEDED(device->Reset(&pp))&&resetCalls==resets+1,"real Reset traverses wrapper exactly once");
    Require(native_transport_source::records.empty()&&native_transport_source::resetting.empty(),"real Reset retires tracked generations");vertices->Release();vertices=nullptr;
    const auto finalReleases=wrapperCalls;Require(device->Release()==0&&wrapperCalls==finalReleases+1,"final Release traverses wrapper exactly once");device=nullptr;factory->Release();factory=nullptr;DestroyWindow(window);window=nullptr;
    Require(first&&second&&third,"all actual VB creations observed across VP switches");
    Require(native_transport_source::records.empty(),"all actual resources retired");
    puts("{\"status\":\"PASS\",\"originalGame\":false,\"systemD3D9\":true}");return 0;
  }catch(const std::exception& e){if(vertices)vertices->Release();if(device)device->Release();if(factory)factory->Release();if(window)DestroyWindow(window);fprintf(stderr,"FAIL %s\n",e.what());return 1;}
}
