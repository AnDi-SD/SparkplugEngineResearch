// Own adapter integration fixture: synthetic native ABI camera packets, actual
// system D3D9 state, and a recording Remix API. This is not game/native evidence.
// No fixed-address hook, bridge, game executable, or native method is executed.
#define WINX_REMIX_TEST
#include "winx_d3d9_probe.cpp"
#include <stdexcept>
#include <limits>

namespace camera_test {
namespace source=native_camera_source;
using Raw=source::abi::spCameraObservedLayout;
static unsigned checks;
static std::vector<remixapi_CameraInfo> cameras;
struct Event { bool setup;unsigned frame,draw; };
static std::vector<Event> events;
static remixapi_ErrorCode response=REMIXAPI_ERROR_CODE_SUCCESS;
static void Check(bool ok,const char* message) {
  ++checks;if(!ok)throw std::runtime_error(message);
}
static void Hr(HRESULT hr,const char* message) {Check(SUCCEEDED(hr),message);}
static remixapi_ErrorCode REMIXAPI_CALL Setup(const remixapi_CameraInfo* info) {
  Check(info&&info->sType==REMIXAPI_STRUCT_TYPE_CAMERA_INFO&&
    info->type==REMIXAPI_CAMERA_TYPE_WORLD&&!info->pNext,"WORLD camera API contract");
  cameras.push_back(*info);events.push_back({true,frameId,drawId});return response;
}
struct Device {
  HMODULE module=nullptr;HWND window=nullptr;IDirect3D9* d3d=nullptr;
  IDirect3DDevice9* device=nullptr;IDirect3DSurface9* primary=nullptr;
  ~Device() {
    if(device)primaryTargets.erase(device);
    if(primary)primary->Release();
    if(device)device->Release();
    if(d3d)d3d->Release();
    if(window)DestroyWindow(window);
    if(module)FreeLibrary(module);
  }
  void Create() {
    window=CreateWindowExW(0,L"STATIC",L"Native camera adapter fixture",WS_OVERLAPPEDWINDOW,
      0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    Check(window!=nullptr,"create owned hidden HWND");
    module=LoadLibraryExW(L"d3d9.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    Check(module!=nullptr,"load system D3D9");
    const auto create=reinterpret_cast<IDirect3D9*(WINAPI*)(UINT)>(GetProcAddress(module,"Direct3DCreate9"));
    Check(create!=nullptr,"system Direct3DCreate9 export");d3d=create(D3D_SDK_VERSION);
    Check(d3d!=nullptr,"create system D3D9 object");
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow=window;pp.BackBufferWidth=pp.BackBufferHeight=64;
    pp.BackBufferFormat=D3DFMT_UNKNOWN;pp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    Hr(d3d->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window,
      D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&device),"create real D3D9 HAL device");
    Hr(device->GetRenderTarget(0,&primary),"retain actual primary target");
    Check(primary!=nullptr,"actual primary target exists");primaryTargets[device]=primary;
  }
};
struct AdapterState {
  remixapi_Interface recording{};
  AdapterState() {
    Check(!source::enabled&&!source::output&&!testRemixApi,"fresh standalone adapter globals");
    source::Reset();source::enabled=source::submitEnabled=true;
    recording.SetupCamera=&Setup;testRemixApi=&recording;
  }
  ~AdapterState() {
    source::enabled=source::submitEnabled=false;source::Reset();source::EndFrame();
    testRemixApi=nullptr;
  }
};
static Raw Camera(float translation=-13.25f) {
  Raw raw{};raw.viewMatrix[0]=raw.viewMatrix[5]=raw.viewMatrix[10]=raw.viewMatrix[15]=1;
  raw.viewMatrix[1]=-0.0f;raw.viewMatrix[12]=translation;
  raw.viewMatrix[13]=.375f;raw.viewMatrix[14]=6.5f;
  raw.projectionMatrix[0]=1.375f;raw.projectionMatrix[5]=1.625f;
  raw.projectionMatrix[10]=1.001f;raw.projectionMatrix[11]=1;
  raw.projectionMatrix[14]=-.125f;raw.dirtyFlags=3;return raw;
}
static void Bind(IDirect3DDevice9* device,const Raw& raw) {
  D3DMATRIX view{},projection{};
  memcpy(&view,raw.viewMatrix,sizeof(view));memcpy(&projection,raw.projectionMatrix,sizeof(projection));
  Hr(device->SetTransform(D3DTS_VIEW,&view),"bind native view to actual D3D state");
  Hr(device->SetTransform(D3DTS_PROJECTION,&projection),"bind native projection to actual D3D state");
}
static void Frame(IDirect3DDevice9* device,const Raw& raw) {
  ++frameId;drawId=0;events.clear();Bind(device,raw);
}
constexpr uint32_t sceneA=0x1100,cameraA=0x2200,sceneB=0x3300,cameraB=0x4400;
static void Capture(const Raw& raw,uint32_t scene=sceneA,uint32_t camera=cameraA,uint32_t table=0x6ef1e0) {
  Check(source::Capture(scene,camera,camera,table,raw),"accept fresh qualified native apply packet");
}
static void Draw(IDirect3DDevice9* device,uint32_t scene=sceneA,uint32_t camera=cameraA) {
  // This is the production AtDraw boundary. The draw marker tests ordering; it
  // does not claim to execute a game draw or qualify renderer camera acceptance.
  source::AtDraw(device,scene,camera);events.push_back({false,frameId,drawId});++drawId;
}
static void Exact(const Raw& raw) {
  Check(!cameras.empty()&&!memcmp(cameras.back().view,raw.viewMatrix,64)&&
    !memcmp(cameras.back().projection,raw.projectionMatrix,64),"exact native matrix bits reach API without transpose or recomputation");
}
static void Run(Device& fixture,AdapterState& state) {
  auto d=fixture.device;const Raw raw=Camera();
  Frame(d,raw);Capture(raw);const auto firstSequence=source::pending.sequence;
  Draw(d);Exact(raw);
  Check(cameras.size()==1&&source::submittedFrame==frameId&&source::selected.sequence==firstSequence,
    "first matching primary draw selects and submits its camera");
  Check(events.size()==2&&events[0].setup&&!events[1].setup&&events[0].draw==0&&events[0].frame==frameId,
    "SetupCamera precedes first draw marker");
  Draw(d);Check(cameras.size()==1,"repeated draw never resubmits camera in one frame");

  Raw clean=raw;clean.dirtyFlags=0;Frame(d,clean);Capture(clean);Draw(d);Exact(clean);
  Check(cameras.size()==2&&source::pending.sequence>firstSequence,"successful clean native apply refreshes the next frame");

  Frame(d,raw);Capture(raw);const auto saved=source::pending;
  Raw ui=Camera(21);ui.twoDimensional=1;ui.projectionBranch=1;
  const auto foreignBefore=source::foreignApplies;
  Check(!source::Capture(sceneA,cameraB,cameraA,0x6dea20,ui),"foreign UI apply is not a main camera packet");
  Check(source::foreignApplies==foreignBefore+1&&source::pending.valid&&source::pending.sequence==saved.sequence&&
    source::pending.camera==saved.camera&&!memcmp(source::pending.info.view,saved.info.view,128),
    "foreign UI apply preserves pending main camera matrices and identity");
  Raw screen=raw;memset(screen.projectionMatrix,0,sizeof(screen.projectionMatrix));
  screen.projectionMatrix[0]=screen.projectionMatrix[5]=screen.projectionMatrix[10]=screen.projectionMatrix[15]=1;
  Bind(d,screen);const auto beforeUI=cameras.size();Draw(d);
  Check(cameras.size()==beforeUI&&source::attemptedFrame!=frameId,"orthographic screen draw does not consume perspective window");
  Bind(d,raw);Draw(d);Check(cameras.size()==beforeUI+1,"main perspective camera survives foreign UI apply and draw");Exact(raw);

  Frame(d,raw);Capture(raw);Raw branch=raw;branch.projectionBranch=1;
  Check(branch.projectionMatrix[11]==1&&!source::Capture(sceneA,cameraA,cameraA,0x6ef1e0,branch)&&!source::pending.valid,
    "native orthographic branch rejects stale perspective _34 equals one");
  Capture(raw);branch=raw;branch.twoDimensional=1;
  Check(!source::Capture(sceneA,cameraA,cameraA,0x6ef1e0,branch)&&!source::pending.valid,"native Is2D rejects camera independently of projection branch");
  Capture(raw);Check(!source::Capture(sceneA,cameraA,cameraA,0x1234,raw)&&!source::pending.valid,"unknown camera ABI vtable invalidates main packet");
  Capture(raw);branch=raw;branch.viewMatrix[3]=std::numeric_limits<float>::quiet_NaN();
  Check(!source::Capture(sceneA,cameraA,cameraA,0x6ef1e0,branch)&&!source::pending.valid,"nonfinite view invalidates packet");
  Capture(raw);branch=raw;branch.projectionMatrix[4]=std::numeric_limits<float>::infinity();
  Check(!source::Capture(sceneA,cameraA,cameraA,0x6ef1e0,branch)&&!source::pending.valid,"nonfinite projection invalidates packet");

  Frame(d,raw);Capture(raw);Draw(d);const auto beforeFresh=cameras.size();
  Frame(d,raw);const auto missedBefore=source::missedFirstDraws;Draw(d);
  Check(cameras.size()==beforeFresh&&source::missedFirstDraws==missedBefore+1&&source::attemptedFrame==frameId,
    "previous frame packet cannot submit without a fresh successful apply");
  Capture(raw);Draw(d);Check(cameras.size()==beforeFresh,"late matching apply cannot reopen a missed frame window");
  Frame(d,raw);Capture(raw);Draw(d);Check(cameras.size()==beforeFresh+1,"next frame fresh apply recovers after missed window");

  Frame(d,raw);Capture(raw);const auto mismatchBefore=source::stateMismatches;
  const Raw changed=Camera(17.75f);Bind(d,changed);const auto beforeMismatch=cameras.size();Draw(d);
  Check(source::stateMismatches==mismatchBefore+1&&cameras.size()==beforeMismatch,
    "early perspective draw with different D3D view closes the frame window");
  Bind(d,raw);Capture(raw);Draw(d);Check(cameras.size()==beforeMismatch,"later exact matrix match cannot submit after early mismatch");
  Frame(d,raw);Capture(raw);Raw changedProjection=raw;changedProjection.projectionMatrix[0]+=.125f;
  Bind(d,changedProjection);Draw(d);Check(source::stateMismatches==mismatchBefore+2&&cameras.size()==beforeMismatch,
    "projection mismatch independently rejects native camera");

  IDirect3DSurface9* other=nullptr;
  Hr(d->CreateRenderTarget(64,64,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&other,nullptr),"create owned offscreen target");
  Frame(d,raw);Capture(raw);Hr(d->SetRenderTarget(0,other),"bind foreign render target");other->Release();
  const auto beforeTarget=cameras.size(),missedTarget=source::missedFirstDraws;Draw(d);
  Check(cameras.size()==beforeTarget&&source::missedFirstDraws==missedTarget+1,"matching perspective on a non-primary target closes frame window");
  Hr(d->SetRenderTarget(0,fixture.primary),"restore actual primary target");Draw(d);
  Check(cameras.size()==beforeTarget,"return to primary target cannot reopen frame window");

  Frame(d,raw);Capture(raw);Draw(d,sceneB,cameraA);
  Check(cameras.size()==beforeTarget,"different scene identity rejects pending camera");
  Frame(d,raw);Capture(raw);Draw(d,sceneA,cameraB);
  Check(cameras.size()==beforeTarget,"different main camera identity rejects pending camera");
  Frame(d,changed);Capture(changed,sceneB,cameraB,0x6dea20);Draw(d,sceneB,cameraB);Exact(changed);
  Check(cameras.size()==beforeTarget+1&&source::selected.scene==sceneB&&source::selected.camera==cameraB,"fresh scene and camera switch selects exact new camera");

  Frame(d,raw);Capture(raw);const auto oldPacket=source::pending;const auto oldEpoch=source::deviceEpoch;
  source::Reset();Check(source::deviceEpoch==oldEpoch+1&&!source::pending.valid&&!source::selected.valid&&
    source::observedFrame==UINT32_MAX&&source::submittedFrame==UINT32_MAX&&source::attemptedFrame==UINT32_MAX,
    "Reset clears pending and selected packets and advances device epoch");
  const auto beforeReset=cameras.size();Draw(d);Check(cameras.size()==beforeReset,"Reset invalidates a previously captured packet");
  Frame(d,raw);source::pending=oldPacket;source::pending.frame=frameId;Draw(d);
  Check(cameras.size()==beforeReset&&source::attemptedFrame==frameId,"stale device epoch is rejected even with a current synthetic frame stamp");
  Frame(d,raw);Capture(raw);Draw(d);Check(cameras.size()==beforeReset+1,"fresh native apply recovers after device reset");

  Frame(d,raw);Capture(raw);Draw(d);const auto beforeLate=cameras.size(),lateBefore=source::lateUpdates;
  Capture(raw);Check(source::lateUpdates==lateBefore,"identical later apply does not report a changed selected camera");
  Capture(changed);Bind(d,changed);Draw(d);
  Check(source::lateUpdates==lateBefore+1&&cameras.size()==beforeLate&&
    !memcmp(source::selected.info.view,raw.viewMatrix,64),"later changed main apply records a late update without a second SetupCamera");
  Capture(raw);Check(source::lateUpdates==lateBefore+1,"return to selected matrix compares with selected camera rather than previous pending packet");
  Capture(changed,sceneB,cameraB,0x6dcbc0);
  Check(source::lateUpdates==lateBefore+2,"later scene and camera switch records a changed selected camera");

  Frame(d,raw);Capture(raw);response=REMIXAPI_ERROR_CODE_GENERAL_FAILURE;
  const auto beforeError=cameras.size(),failureBefore=source::failures;Draw(d);
  Check(cameras.size()==beforeError+1&&source::failures==failureBefore+1&&source::submittedFrame!=frameId,
    "normal API rejection reports one failure and does not claim submission");
  response=REMIXAPI_ERROR_CODE_SUCCESS;Draw(d);Capture(raw);Draw(d);
  Check(cameras.size()==beforeError+1&&source::failures==failureBefore+1,"normal API error keeps original draw path without same-frame retry");
  Frame(d,raw);Capture(raw);Draw(d);Check(cameras.size()==beforeError+2,"normal API rejection can recover on next fresh frame");

  Frame(d,raw);Capture(raw);state.recording.SetupCamera=nullptr;
  const auto beforeNull=cameras.size(),nullFailures=source::failures;Draw(d);Draw(d);
  Check(cameras.size()==beforeNull&&source::failures==nullFailures+1,"missing SetupCamera capability falls back once per frame");
  state.recording.SetupCamera=&Setup;Draw(d);Check(cameras.size()==beforeNull,"restoring capability does not retry current frame");
  Frame(d,raw);Capture(raw);testRemixApi=nullptr;Draw(d);Draw(d);
  Check(cameras.size()==beforeNull&&source::failures==nullFailures+2,"missing API interface falls back once per frame");
  testRemixApi=&state.recording;Frame(d,raw);Capture(raw);Draw(d);
  Check(cameras.size()==beforeNull+1,"fresh frame recovers when camera API becomes available");

  Frame(d,raw);Capture(raw);source::submitEnabled=false;
  const auto beforeObserve=cameras.size(),matchedBefore=source::matched;Draw(d);Draw(d);
  Check(cameras.size()==beforeObserve&&source::matched==matchedBefore+1&&source::observedFrame==frameId,
    "observation selects matching native camera once without calling API");
  Capture(changed);Check(source::lateUpdates==lateBefore+3,"observation also diagnoses changed late main apply");
  source::submitEnabled=true;Draw(d);Check(cameras.size()==beforeObserve,"enabling submission after observed draw does not reopen frame window");
  Frame(d,raw);Capture(raw);source::enabled=false;Draw(d);
  Check(cameras.size()==beforeObserve&&source::attemptedFrame!=frameId,"disabled adapter leaves draw untouched");
  source::enabled=true;Draw(d);Check(cameras.size()==beforeObserve+1,"enabled adapter accepts fresh pending camera before any attempted draw");
}
} // namespace camera_test
int main() {
  using namespace camera_test;
  try {
    Device device;device.Create();AdapterState state;Run(device,state);
    printf("{\"status\":\"PASS\",\"checks\":%u,\"cameraCalls\":%zu,\"device\":\"system-d3d9-hal\",\"source\":\"synthetic-native-ABI\",\"bridge\":false,\"gameEvidence\":false}\n",checks,cameras.size());
    return 0;
  } catch(const std::exception& error) {
    fprintf(stderr,"FAIL after %u checks: %s\n",checks,error.what());return 1;
  }
}
