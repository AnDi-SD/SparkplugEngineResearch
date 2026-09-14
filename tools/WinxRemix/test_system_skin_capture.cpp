// Own real system-D3D9 transport test; no original game instructions execute.
#define WINX_REMIX_TEST
#include "winx_d3d9_probe.cpp"
#include <stdexcept>
static void Require(bool condition,const char* why){if(!condition)throw std::runtime_error(why);}
using ReleaseFunction=ULONG(STDMETHODCALLTYPE*)(IDirect3DDevice9*);
static ReleaseFunction beforeWrapper;static unsigned wrapperCalls;
static bool inspectRelease,mutateRelease,releaseReadRefused;
static ULONG STDMETHODCALLTYPE WrappedRelease(IDirect3DDevice9* device){
  ++wrapperCalls;
  if(inspectRelease){
    std::unique_lock<std::recursive_mutex> borrow(guard);d3d9_state_witness::Witness witness{};
    releaseReadRefused=!d3d9_state_witness::Read(borrow,device,witness);
    if(mutateRelease)device->SetRenderState(D3DRS_TEXTUREFACTOR,0x12345678);
  }
  return beforeWrapper(device);
}
using ResetFunction=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,D3DPRESENT_PARAMETERS*);
using QueryFunction=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,REFIID,void**);
static ResetFunction beforeReset;static QueryFunction beforeQuery;static unsigned resetCalls,queryCalls;
using GetSwapFunction=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,UINT,IDirect3DSwapChain9**);
using AdditionalSwapFunction=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,D3DPRESENT_PARAMETERS*,IDirect3DSwapChain9**);
static GetSwapFunction beforeGetSwap;static AdditionalSwapFunction beforeAdditionalSwap;static unsigned getSwapCalls,additionalSwapCalls;
static HRESULT STDMETHODCALLTYPE WrappedGetSwap(IDirect3DDevice9* d,UINT i,IDirect3DSwapChain9** out){++getSwapCalls;return beforeGetSwap(d,i,out);}
static HRESULT STDMETHODCALLTYPE WrappedAdditionalSwap(IDirect3DDevice9* d,D3DPRESENT_PARAMETERS* p,IDirect3DSwapChain9** out){++additionalSwapCalls;return beforeAdditionalSwap(d,p,out);}
static HRESULT STDMETHODCALLTYPE WrappedReset(IDirect3DDevice9* device,D3DPRESENT_PARAMETERS* pp){++resetCalls;return beforeReset(device,pp);}
static HRESULT STDMETHODCALLTYPE WrappedQuery(IDirect3DDevice9* device,REFIID iid,void** out){++queryCalls;return beforeQuery(device,iid,out);}
static void ConstantWitness(IDirect3DDevice9* device){
  std::unique_lock<std::recursive_mutex> borrow(guard);d3d9_state_witness::Witness witness{};
  const float floats[4]={0.125f,-2.0f,3.5f,1.0f};const int integers[4]={1,-2,3,4};const BOOL boolean=TRUE;
  float actualFloats[4]{};int actualIntegers[4]{};BOOL actualBoolean=FALSE;
  for(unsigned kind=0;kind<6;++kind){
    Require(d3d9_state_witness::Read(borrow,device,witness),"real device constant coverage");HRESULT set=E_FAIL,get=E_FAIL;
    switch(kind){
      case 0:set=device->SetVertexShaderConstantF(0,floats,1);get=device->GetVertexShaderConstantF(0,actualFloats,1);break;
      case 1:set=device->SetVertexShaderConstantI(0,integers,1);get=device->GetVertexShaderConstantI(0,actualIntegers,1);break;
      case 2:set=device->SetVertexShaderConstantB(0,&boolean,1);get=device->GetVertexShaderConstantB(0,&actualBoolean,1);break;
      case 3:set=device->SetPixelShaderConstantF(0,floats,1);get=device->GetPixelShaderConstantF(0,actualFloats,1);break;
      case 4:set=device->SetPixelShaderConstantI(0,integers,1);get=device->GetPixelShaderConstantI(0,actualIntegers,1);break;
      case 5:set=device->SetPixelShaderConstantB(0,&boolean,1);get=device->GetPixelShaderConstantB(0,&actualBoolean,1);break;
    }
    Require(SUCCEEDED(set)&&SUCCEEDED(get),"real shader constant setter/getter");
    Require(!d3d9_state_witness::Current(borrow,witness),"every real constant setter invalidates prior read");
    Require(kind%3==0?!memcmp(actualFloats,floats,sizeof floats):kind%3==1?!memcmp(actualIntegers,integers,sizeof integers):actualBoolean==boolean,"constant values round trip unchanged");
  }
  IDirect3DStateBlock9* block=nullptr;
  struct Release {IDirect3DStateBlock9*& block;~Release(){if(block)block->Release();}} release{block};
  Require(SUCCEEDED(device->CreateStateBlock(D3DSBT_VERTEXSTATE,&block))&&block,"real constant state block");
  const float changed[4]={7,8,9,10};Require(SUCCEEDED(device->SetVertexShaderConstantF(0,changed,1)),"change recorded constant");
  Require(d3d9_state_witness::Read(borrow,device,witness),"state block tracked");
  Require(SUCCEEDED(block->Apply())&&!d3d9_state_witness::Current(borrow,witness),"real Apply invalidates read");
  Require(SUCCEEDED(device->GetVertexShaderConstantF(0,actualFloats,1))&&!memcmp(actualFloats,floats,sizeof floats),"state block restores original constant");
  block->Release();block=nullptr;Require(d3d9_state_witness::blocks.empty(),"real state block retired");
  Require(d3d9_state_witness::Read(borrow,device,witness),"device stays qualified after constant operations");
  IDirect3DSurface9* target=nullptr;const auto beforeReleases=wrapperCalls;
  Require(SUCCEEDED(device->GetRenderTarget(0,&target))&&target,"borrow current real render target");
  target->Release();target=nullptr;
  printf("{\"phase\":\"read-only target\",\"serialBefore\":%llu,\"serialAfter\":%llu,\"deviceReleaseCalls\":%u}\n",witness.serial,d3d9_state_witness::serial,wrapperCalls-beforeReleases);
  Require(d3d9_state_witness::Current(borrow,witness),"read-only target round trip preserves device state witness");
  device->AddRef();inspectRelease=true;releaseReadRefused=false;
  const auto remaining=device->Release();inspectRelease=false;
  Require(remaining&&releaseReadRefused&&d3d9_state_witness::Current(borrow,witness),"nonfinal Release blocks reentrant read and preserves completed state");
  DWORD originalFactor=0;Require(SUCCEEDED(device->GetRenderState(D3DRS_TEXTUREFACTOR,&originalFactor)),"read state before lifetime callback");
  device->AddRef();inspectRelease=mutateRelease=true;releaseReadRefused=false;
  const auto changedRemaining=device->Release();inspectRelease=mutateRelease=false;
  Require(changedRemaining&&releaseReadRefused&&!d3d9_state_witness::Current(borrow,witness),"setter reentered from nonfinal Release invalidates state");
  Require(SUCCEEDED(device->SetRenderState(D3DRS_TEXTUREFACTOR,originalFactor)),"restore state after owned lifetime mutation");
}
static void MaterialCapture(IDirect3DDevice9* device){
  IDirect3DTexture9* texture=nullptr;
  struct Release {IDirect3DDevice9* device;IDirect3DTexture9*& texture;~Release(){device->SetTexture(0,nullptr);if(texture)texture->Release();}} release{device,texture};
  Require(SUCCEEDED(device->CreateTexture(8,4,3,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture,nullptr)),"real managed material texture");
  for(unsigned level=0;level<3;++level){D3DLOCKED_RECT rect{};Require(SUCCEEDED(texture->LockRect(level,&rect,nullptr,0)),"texture source write");
    for(unsigned y=0;y<(4u>>level);++y)for(unsigned x=0;x<(8u>>level);++x){const uint32_t pixel=0xff000000u|(level<<16)|(x<<8)|y;
      memcpy(static_cast<uint8_t*>(rect.pBits)+y*rect.Pitch+x*4,&pixel,4);}
    Require(SUCCEEDED(texture->UnlockRect(level)),"texture source unlock");}
  Require(SUCCEEDED(device->SetTexture(0,texture)),"bound material texture");
  std::unique_lock<std::recursive_mutex> borrow(guard);native_transport_source::TextureWitness witness{};
  std::vector<uint8_t> bytes;Require(skin_material_capture::CopyTexture(borrow,device,texture,bytes,witness),"owned all-mip texture snapshot");
  Require(bytes.size()==28+3*16+(8*4+4*2+2)*4,"SKT1 exact mip sizes");
  size_t offset=28;
  for(unsigned level=0;level<3;++level){uint32_t header[4]{};memcpy(header,bytes.data()+offset,16);offset+=16;
    Require(header[0]==(8u>>level)&&header[1]==(4u>>level)&&header[2]==header[0]*4&&header[3]==header[2]*header[1],"mip header and tight stride");
    for(unsigned y=0;y<header[1];++y)for(unsigned x=0;x<header[0];++x){uint32_t pixel=0;memcpy(&pixel,bytes.data()+offset+y*header[2]+x*4,4);
      Require(pixel==(0xff000000u|(level<<16)|(x<<8)|y),"all original mip pixels preserved");}offset+=header[3];}
  Require(native_transport_source::CurrentTexture(borrow,witness),"readonly capture does not change content generation");
  skin_material_capture::Snapshot before{},after{};Require(skin_material_capture::Read(device,before),"material state snapshot");
  Require(skin_material_capture::Save(L".",device,1,1,0,0,0,nullptr),"complete material sidecar and texture write");
  Require(skin_material_capture::Save(L".",device,2,2,0,0,0,nullptr)&&skin_material_capture::textures.size()==1,"unchanged texture deduplicated");
  Require(skin_material_capture::Read(device,after)&&before==after,"capture leaves all bound render/stage/sampler states unchanged");
  const auto file=CreateFileW(L"texture-0001.skt",GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
  Require(file!=INVALID_HANDLE_VALUE,"saved texture reopen");std::vector<uint8_t> disk(bytes.size()+1);DWORD read=0;
  const bool complete=ReadFile(file,disk.data(),DWORD(disk.size()),&read,nullptr)!=FALSE;CloseHandle(file);
  disk.resize(read);Require(complete&&disk==bytes,"saved SKT1 bytes round trip exactly");
  D3DLOCKED_RECT locked{};Require(SUCCEEDED(texture->LockRect(0,&locked,nullptr,0)),"new texture mutation");
  std::vector<uint8_t> unchanged=bytes;native_transport_source::TextureWitness refused{};
  Require(!skin_material_capture::CopyTexture(borrow,device,texture,unchanged,refused)&&unchanged==bytes,"open writable lock refuses without publishing bytes");
  const uint32_t changed=0x12345678;memcpy(locked.pBits,&changed,4);Require(SUCCEEDED(texture->UnlockRect(0)),"changed texture unlock");
  Require(!native_transport_source::CurrentTexture(borrow,witness),"old texture content witness retired");
  Require(skin_material_capture::Save(L".",device,3,3,0,0,0,nullptr)&&skin_material_capture::textures.size()==2,"changed content gets fresh texture file");
}
static void State(IDirect3DDevice9* device,const char* phase){const auto t=*reinterpret_cast<void***>(device);
  printf("{\"phase\":\"%s\",\"table\":%llu,\"createVB\":%s,\"createIB\":%s,\"createDeclaration\":%s,\"draw\":%s,\"records\":%zu,\"createdVB\":%u}\n",phase,
    static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(t)),t[26]==reinterpret_cast<void*>(CreateVB)?"true":"false",t[27]==reinterpret_cast<void*>(CreateIB)?"true":"false",
    t[86]==reinterpret_cast<void*>(CreateDeclaration)?"true":"false",t[82]==reinterpret_cast<void*>(DrawIndexed)?"true":"false",native_transport_source::records.size(),native_transport_source::created[0]);fflush(stdout);
}
int main(){HWND window=nullptr;IDirect3D9* factory=nullptr;IDirect3DDevice9* device=nullptr;IDirect3DVertexBuffer9* vertices=nullptr;
  try {
    SetEnvironmentVariableW(L"WINX_REMIX_BACKEND",L"system");factory=ProxyCreate9(D3D_SDK_VERSION);Require(factory!=nullptr,"system factory");
    native_mesh_source::enabled=true;native_transport_source::enabled=true;skin_draw_source::enabled=true;
    window=CreateWindowW(L"STATIC",L"System skin transport fixture",WS_OVERLAPPED,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);Require(window!=nullptr,"owned hidden window");
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;
    pp.BackBufferWidth=64;pp.BackBufferHeight=64;pp.BackBufferFormat=D3DFMT_X8R8G8B8;pp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    Require(SUCCEEDED(factory->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_MIXED_VERTEXPROCESSING,&pp,&device)),"real mixed VP device");
    State(device,"created");
    const auto table=*reinterpret_cast<void***>(device);
    Require(table[2]!=reinterpret_cast<void*>(DeviceRelease)&&table[16]!=reinterpret_cast<void*>(Reset)&&table[0]!=reinterpret_cast<void*>(TextureDeviceQuery),"system lifetime/alias installation is deferred");
    Require(table[13]!=reinterpret_cast<void*>(TextureAdditionalSwap)&&table[14]!=reinterpret_cast<void*>(GetSwapChain),"system swap-chain installation is deferred");
    beforeWrapper=reinterpret_cast<ReleaseFunction>(table[2]);DWORD protection=0,ignored=0;
    beforeReset=reinterpret_cast<ResetFunction>(table[16]);beforeQuery=reinterpret_cast<QueryFunction>(table[0]);
    beforeGetSwap=reinterpret_cast<GetSwapFunction>(table[14]);beforeAdditionalSwap=reinterpret_cast<AdditionalSwapFunction>(table[13]);
    Require(VirtualProtect(table,17*sizeof(void*),PAGE_READWRITE,&protection)!=FALSE,"owned fixture slots writable");
    InterlockedExchangePointer(table,reinterpret_cast<void*>(WrappedQuery));InterlockedExchangePointer(table+16,reinterpret_cast<void*>(WrappedReset));
    InterlockedExchangePointer(table+13,reinterpret_cast<void*>(WrappedAdditionalSwap));InterlockedExchangePointer(table+14,reinterpret_cast<void*>(WrappedGetSwap));
    InterlockedExchangePointer(table+2,reinterpret_cast<void*>(WrappedRelease));VirtualProtect(table,17*sizeof(void*),protection,&ignored);
    Require(SUCCEEDED(device->CreateVertexBuffer(256,0,0,D3DPOOL_MANAGED,&vertices,nullptr))&&vertices,"real first VB");State(device,"first VB");
    const bool first=native_transport_source::records.count(vertices)!=0;vertices->Release();vertices=nullptr;
    Require(TransportDeviceHooked(device)&&Original<ReleaseFunction>(device,2)==WrappedRelease,"resource boundary preserves completed outer Release chain");
    Require(Original<ResetFunction>(device,16)==WrappedReset&&Original<QueryFunction>(device,0)==WrappedQuery,"completed Reset/Query chains retained");
    Require(Original<GetSwapFunction>(device,14)==WrappedGetSwap&&Original<AdditionalSwapFunction>(device,13)==WrappedAdditionalSwap,"completed swap-chain wrappers retained");
    IDirect3DSwapChain9* swap=nullptr;const auto getCalls=getSwapCalls;
    Require(SUCCEEDED(device->GetSwapChain(0,&swap))&&swap&&getSwapCalls==getCalls+1,"GetSwapChain wrapper traversed once");swap->Release();swap=nullptr;
    auto additional=pp;const auto addCalls=additionalSwapCalls;
    Require(SUCCEEDED(device->CreateAdditionalSwapChain(&additional,&swap))&&swap&&additionalSwapCalls==addCalls+1,"additional swap-chain wrapper traversed once");swap->Release();swap=nullptr;
    MaterialCapture(device);
    ConstantWitness(device);
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
    Require(d3d9_state_witness::devices.empty(),"actual device state witness retired");
    puts("{\"status\":\"PASS\",\"originalGame\":false,\"systemD3D9\":true}");return 0;
  }catch(const std::exception& e){if(vertices)vertices->Release();if(device)device->Release();if(factory)factory->Release();if(window)DestroyWindow(window);fprintf(stderr,"FAIL %s\n",e.what());return 1;}
}
