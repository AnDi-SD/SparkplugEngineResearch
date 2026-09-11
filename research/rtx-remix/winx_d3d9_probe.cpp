// Our diagnostic/platform adapter. No reconstructed game logic.
// x86 only. Reads state at sampled draws and forwards calls to the selected D3D9.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <map>
#include <set>
#include <mutex>
#include <share.h>
#include <string>
#include <sstream>
#define REMIX_ALLOW_X86
#include <remix/remix_c.h>

static HMODULE selfModule, backend;
static FILE* logFile;
static std::once_flag initializeOnce;
static std::recursive_mutex guard;
static std::map<void**, std::map<unsigned, void*>> originals;
static unsigned frameId, drawId, records;
static bool triggered;
static unsigned traceUntilFrame;
static unsigned bufferRecords, dynamicBufferRecords;
static unsigned textureRecords;
static bool explicitMipLevels;
static bool textureReadback;
static std::set<IDirect3DBaseTexture9*> readTextures;
static bool resubmitTextures;
static std::set<IDirect3DBaseTexture9*> submittedTextures;
static bool orthographicUi;
static bool menuBackground;
static bool skyLayers;
static bool fitWindow;
static bool viewportScale;
struct Presentation { HWND window=nullptr; UINT width=0,height=0; BOOL windowed=FALSE; UINT initialWidth=0,initialHeight=0; };
static std::map<IDirect3DDevice9*,Presentation> presentations;
static std::set<IDirect3DDevice9*> uiStarted;
// Non-owning identities, refreshed on device creation/reset.
static std::map<IDirect3DDevice9*,IDirect3DSurface9*> primaryTargets;
struct BufferLock { void* data; UINT size; DWORD flags; };
static std::map<void*, BufferLock> bufferLocks;
using FvfFromDeclaration = HRESULT(WINAPI*)(const D3DVERTEXELEMENT9*, DWORD*);
static FvfFromDeclaration fvfFromDeclaration;
static wchar_t liveConfigPath[MAX_PATH]{};

static void Initialize() {
  wchar_t path[MAX_PATH]{}, mode[32]{}, output[MAX_PATH]{};
  GetModuleFileNameW(selfModule, path, MAX_PATH);
  wchar_t* slash = wcsrchr(path, L'\\');
  if (!slash) return;
  *(slash + 1) = 0;
  GetEnvironmentVariableW(L"WINX_REMIX_BACKEND", mode, 32);
  if (wcscmp(mode, L"system") == 0) {
    GetSystemDirectoryW(path, MAX_PATH);
    wcscat_s(path, L"\\d3d9.dll");
  } else {
    wcscat_s(path, L"d3d9.remix-original.dll");
  }
  backend = LoadLibraryW(path);
  if(wcscmp(mode,L"system")!=0) GetEnvironmentVariableW(L"WINX_REMIX_LIVE_CONFIG",liveConfigPath,MAX_PATH);
  wchar_t mipOption[8]{};
  explicitMipLevels=GetEnvironmentVariableW(L"WINX_REMIX_EXPLICIT_MIPS",mipOption,8) && wcscmp(mipOption,L"1")==0;
  wchar_t readbackOption[8]{};
  textureReadback=GetEnvironmentVariableW(L"WINX_REMIX_TEXTURE_READBACK",readbackOption,8) && wcscmp(readbackOption,L"1")==0;
  wchar_t resubmitOption[8]{};
  resubmitTextures=GetEnvironmentVariableW(L"WINX_REMIX_RESUBMIT_TEXTURES",resubmitOption,8) && wcscmp(resubmitOption,L"1")==0;
  wchar_t uiOption[8]{};
  orthographicUi=GetEnvironmentVariableW(L"WINX_REMIX_ORTHOGRAPHIC_UI",uiOption,8) && wcscmp(uiOption,L"1")==0;
  wchar_t menuOption[8]{};
  menuBackground=GetEnvironmentVariableW(L"WINX_REMIX_MENU_BACKGROUND",menuOption,8) && wcscmp(menuOption,L"1")==0;
  wchar_t skyOption[8]{};
  skyLayers=GetEnvironmentVariableW(L"WINX_REMIX_SKY_LAYERS",skyOption,8) && wcscmp(skyOption,L"1")==0;
  wchar_t windowOption[8]{};
  fitWindow=GetEnvironmentVariableW(L"WINX_REMIX_FIT_WINDOW",windowOption,8) && wcscmp(windowOption,L"1")==0;
  wchar_t scaleOption[8]{};
  viewportScale=GetEnvironmentVariableW(L"WINX_REMIX_VIEWPORT_SCALE",scaleOption,8) && wcscmp(scaleOption,L"1")==0;
  wchar_t normalize[8]{};
  if (GetEnvironmentVariableW(L"WINX_REMIX_NORMALIZE_FVF", normalize, 8) && wcscmp(normalize,L"1")==0) {
    const HMODULE d3dx=LoadLibraryExW(L"d3dx9_43.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(d3dx) fvfFromDeclaration=reinterpret_cast<FvfFromDeclaration>(GetProcAddress(d3dx,"D3DXFVFFromDeclarator"));
  }
  if (GetEnvironmentVariableW(L"WINX_REMIX_TRACE", output, MAX_PATH)) {
    if(sizeof(void*)==8) wcscat_s(output,L".x64");
    logFile = _wfsopen(output, L"wb", _SH_DENYNO);
  }
  if (logFile) {
    fprintf(logFile, "{\"event\":\"init\",\"backend\":\"%s\",\"loaded\":%s,\"pid\":%lu}\n",
      wcscmp(mode, L"system") == 0 ? "system" : "remix", backend ? "true" : "false", GetCurrentProcessId());
    fflush(logFile);
  }
}

template<class F> static F Proc(const char* name) {
  std::call_once(initializeOnce, Initialize);
  return backend ? reinterpret_cast<F>(GetProcAddress(backend, name)) : nullptr;
}

template<class F> static F Original(void* object, unsigned slot) {
  std::lock_guard<std::recursive_mutex> lock(guard);
  return reinterpret_cast<F>(originals.at(*reinterpret_cast<void***>(object)).at(slot));
}

static void Patch(void* object, unsigned slot, void* function) {
  std::lock_guard<std::recursive_mutex> lock(guard);
  void** table = *reinterpret_cast<void***>(object);
  if (originals[table].count(slot)) return;
  DWORD previous;
  if (!VirtualProtect(table + slot, sizeof(void*), PAGE_READWRITE, &previous)) {
    if (logFile) fprintf(logFile, "{\"event\":\"hook_failed\",\"slot\":%u,\"error\":%lu}\n", slot, GetLastError());
    return;
  }
  originals[table][slot] = table[slot];
  InterlockedExchangePointer(table + slot, function);
  DWORD ignored;
  VirtualProtect(table + slot, sizeof(void*), previous, &ignored);
}

static void RememberPrimaryTarget(IDirect3DDevice9* device) {
  IDirect3DSurface9* target=nullptr;
  if(SUCCEEDED(device->GetRenderTarget(0,&target)) && target) {
    primaryTargets[device]=target;
    target->Release();
  } else primaryTargets.erase(device);
}

static void LogPresentation(const char* event, const D3DPRESENT_PARAMETERS* p, HWND fallback=nullptr) {
  if(!logFile || !p) return;
  HWND window=p->hDeviceWindow?p->hDeviceWindow:fallback;
  RECT client{}, rect{};
  GetClientRect(window,&client); GetWindowRect(window,&rect);
  fprintf(logFile,"{\"event\":\"%s\",\"frame\":%u,\"size\":[%u,%u],\"windowed\":%d,\"hwnd\":%llu,\"client\":[%ld,%ld],\"window\":[%ld,%ld,%ld,%ld]}\n",
    event,frameId,p->BackBufferWidth,p->BackBufferHeight,p->Windowed,
    static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(window)),client.right,client.bottom,rect.left,rect.top,rect.right,rect.bottom);
  fflush(logFile);
}

// Own window policy: keep the entire backbuffer visible, including after Reset.
// Never request a display mode change. Fit to this monitor's work area.
static void RememberPresentation(IDirect3DDevice9* d,const D3DPRESENT_PARAMETERS& p,HWND fallback) {
  auto& state=presentations[d];
  if(!state.initialWidth) {state.initialWidth=p.BackBufferWidth;state.initialHeight=p.BackBufferHeight;}
  state.window=p.hDeviceWindow?p.hDeviceWindow:fallback;
  state.width=p.BackBufferWidth;state.height=p.BackBufferHeight;state.windowed=p.Windowed;
  uiStarted.erase(d);
  if(!fitWindow || !state.windowed || !state.width || !state.height || !IsWindow(state.window)) return;
  RECT client{},rect{}; MONITORINFO monitor{sizeof(monitor)};
  if(!GetClientRect(state.window,&client) || !GetWindowRect(state.window,&rect) ||
     !GetMonitorInfoW(MonitorFromWindow(state.window,MONITOR_DEFAULTTONEAREST),&monitor)) return;
  const LONG borderWidth=(rect.right-rect.left)-(client.right-client.left);
  const LONG borderHeight=(rect.bottom-rect.top)-(client.bottom-client.top);
  const LONG availableWidth=monitor.rcWork.right-monitor.rcWork.left-borderWidth;
  const LONG availableHeight=monitor.rcWork.bottom-monitor.rcWork.top-borderHeight;
  if(availableWidth<=0 || availableHeight<=0) return;
  double scale=1.0;
  if(state.width>static_cast<UINT>(availableWidth)) scale=double(availableWidth)/state.width;
  if(state.height*scale>availableHeight) scale=double(availableHeight)/state.height;
  const LONG width=static_cast<LONG>(state.width*scale)+borderWidth;
  const LONG height=static_cast<LONG>(state.height*scale)+borderHeight;
  const LONG x=max(monitor.rcWork.left,min(rect.left,monitor.rcWork.right-width));
  const LONG y=max(monitor.rcWork.top,min(rect.top,monitor.rcWork.bottom-height));
  SetWindowPos(state.window,nullptr,x,y,width,height,SWP_NOZORDER|SWP_NOACTIVATE);
  LogPresentation("fit_window",&p,state.window);
}

static void Matrix(const char* name, const D3DMATRIX& matrix, HRESULT hr) {
  fprintf(logFile, ",\"%s_hr\":%ld,\"%s\":[", name, hr, name);
  for (unsigned i=0; i<16; ++i) {
    const float value = reinterpret_cast<const float*>(&matrix)[i];
    if (i) fputc(',', logFile);
    if (std::isfinite(value)) fprintf(logFile, "%.9g", value);
    else fputs("null", logFile);
  }
  fputc(']', logFile);
}

static void Observe(IDirect3DDevice9* device, const char* call, D3DPRIMITIVETYPE type, UINT count) {
  std::lock_guard<std::recursive_mutex> lock(guard);
  ++drawId;
  if (!logFile || records >= 65536 || !(frameId < 2 || frameId % 300 == 0 || triggered || frameId<traceUntilFrame)) return;
  ++records;
  D3DMATRIX world{}, view{}, projection{};
  const HRESULT whr=device->GetTransform(D3DTS_WORLD,&world);
  const HRESULT vhr=device->GetTransform(D3DTS_VIEW,&view);
  const HRESULT phr=device->GetTransform(D3DTS_PROJECTION,&projection);
  D3DVIEWPORT9 viewport{};
  const HRESULT vpHr=device->GetViewport(&viewport);
  DWORD fvf=0, z=0, zw=0, alpha=0, alphaTest=0;
  const HRESULT fvfHr=device->GetFVF(&fvf);
  const HRESULT zHr=device->GetRenderState(D3DRS_ZENABLE,&z);
  const HRESULT zwHr=device->GetRenderState(D3DRS_ZWRITEENABLE,&zw);
  device->GetRenderState(D3DRS_ALPHABLENDENABLE,&alpha);
  device->GetRenderState(D3DRS_ALPHATESTENABLE,&alphaTest);
  IDirect3DVertexShader9* vs=nullptr;
  IDirect3DPixelShader9* ps=nullptr;
  const HRESULT vsHr=device->GetVertexShader(&vs), psHr=device->GetPixelShader(&ps);
  fprintf(logFile, "{\"event\":\"draw\",\"frame\":%u,\"draw\":%u,\"call\":\"%s\",\"type\":%u,\"count\":%u,\"fvf\":%lu,\"fvf_hr\":%ld,\"vs\":%s,\"vs_hr\":%ld,\"ps\":%s,\"ps_hr\":%ld,\"z\":%lu,\"z_hr\":%ld,\"zw\":%lu,\"zw_hr\":%ld,\"alpha\":%lu,\"alphaTest\":%lu",
    frameId,drawId,call,type,count,fvf,fvfHr,vs?"true":"false",vsHr,ps?"true":"false",psHr,z,zHr,zw,zwHr,alpha,alphaTest);
  Matrix("world",world,whr); Matrix("view",view,vhr); Matrix("projection",projection,phr);
  const D3DRENDERSTATETYPE states[]={D3DRS_LIGHTING,D3DRS_AMBIENT,D3DRS_FOGENABLE,D3DRS_FOGCOLOR,
    D3DRS_FOGTABLEMODE,D3DRS_FOGVERTEXMODE,D3DRS_FOGSTART,D3DRS_FOGEND,D3DRS_FOGDENSITY,D3DRS_RANGEFOGENABLE,
    D3DRS_ZFUNC,D3DRS_CULLMODE,D3DRS_COLORWRITEENABLE,D3DRS_SRCBLEND,D3DRS_DESTBLEND,D3DRS_BLENDOP,
    D3DRS_ALPHAFUNC,D3DRS_ALPHAREF,D3DRS_CLIPPING,D3DRS_CLIPPLANEENABLE,D3DRS_DIFFUSEMATERIALSOURCE,D3DRS_TEXTUREFACTOR};
  fputs(",\"states\":[",logFile);
  for(unsigned i=0;i<sizeof(states)/sizeof(states[0]);++i) {
    DWORD value=0; const HRESULT hr=device->GetRenderState(states[i],&value);
    fprintf(logFile,"%s[%u,%lu,%ld]",i?",":"",states[i],value,hr);
  }
  fputs("],\"textureStages\":[",logFile);
  const D3DTEXTURESTAGESTATETYPE textureStates[]={D3DTSS_COLOROP,D3DTSS_COLORARG1,D3DTSS_COLORARG2,D3DTSS_ALPHAOP,D3DTSS_ALPHAARG1,D3DTSS_ALPHAARG2,D3DTSS_TEXCOORDINDEX,D3DTSS_TEXTURETRANSFORMFLAGS};
  for(unsigned i=0;i<8;++i) {
    DWORD value=0; const HRESULT hr=device->GetTextureStageState(0,textureStates[i],&value);
    fprintf(logFile,"%s[%u,%lu,%ld]",i?",":"",textureStates[i],value,hr);
  }
  fputc(']',logFile);
  D3DMATERIAL9 material{};
  if(SUCCEEDED(device->GetMaterial(&material))) {
    fprintf(logFile,",\"material\":[%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g]",
      material.Diffuse.r,material.Diffuse.g,material.Diffuse.b,material.Ambient.r,material.Ambient.g,material.Ambient.b,
      material.Emissive.r,material.Emissive.g,material.Emissive.b,material.Power);
  }
  IDirect3DVertexBuffer9* stream=nullptr; UINT offset=0,stride=0;
  if(SUCCEEDED(device->GetStreamSource(0,&stream,&offset,&stride)) && stream) {
    D3DVERTEXBUFFER_DESC desc{}; stream->GetDesc(&desc);
    fprintf(logFile,",\"stream0\":[%llu,%u,%u,%u,%lu,%u]",static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(stream)),offset,stride,desc.Size,desc.Usage,desc.Pool);
    stream->Release();
  }
  fprintf(logFile, ",\"viewport_hr\":%ld,\"viewport\":[%lu,%lu,%lu,%lu,%.9g,%.9g]",vpHr,viewport.X,viewport.Y,viewport.Width,viewport.Height,viewport.MinZ,viewport.MaxZ);
  IDirect3DSurface9* target=nullptr;
  if (SUCCEEDED(device->GetRenderTarget(0,&target)) && target) {
    D3DSURFACE_DESC desc{};
    if (SUCCEEDED(target->GetDesc(&desc))) fprintf(logFile,",\"target\":[%u,%u,%u]",desc.Width,desc.Height,desc.Format);
    target->Release();
  }
  IDirect3DVertexDeclaration9* declaration=nullptr;
  if (SUCCEEDED(device->GetVertexDeclaration(&declaration)) && declaration) {
    D3DVERTEXELEMENT9 elements[MAXD3DDECLLENGTH+1]; UINT size=MAXD3DDECLLENGTH+1;
    if (SUCCEEDED(declaration->GetDeclaration(elements,&size)) && size<=MAXD3DDECLLENGTH+1) {
      fputs(",\"declaration\":[",logFile);
      for(UINT i=0;i<size;++i) fprintf(logFile,"%s[%u,%u,%u,%u,%u,%u]",i?",":"",elements[i].Stream,elements[i].Offset,elements[i].Type,elements[i].Method,elements[i].Usage,elements[i].UsageIndex);
      fputc(']',logFile);
    }
    declaration->Release();
  }
  IDirect3DBaseTexture9* texture=nullptr;
  if(SUCCEEDED(device->GetTexture(0,&texture)) && texture) {
    fprintf(logFile,",\"texture0_type\":%u",texture->GetType());
    if(texture->GetType()==D3DRTYPE_TEXTURE) {
      D3DSURFACE_DESC desc{};
      if(SUCCEEDED(static_cast<IDirect3DTexture9*>(texture)->GetLevelDesc(0,&desc)))
        fprintf(logFile,",\"texture0\":[%u,%u,%u]",desc.Width,desc.Height,desc.Format);
      if(textureReadback && readTextures.size()<64 && desc.Format==D3DFMT_A8R8G8B8 &&
         desc.Pool==D3DPOOL_MANAGED && desc.Width<=1024 && desc.Height<=1024 && readTextures.insert(texture).second) {
        D3DLOCKED_RECT rect{}; auto t=static_cast<IDirect3DTexture9*>(texture);
        const HRESULT lockHr=t->LockRect(0,&rect,nullptr,D3DLOCK_READONLY);
        fprintf(logFile,",\"textureReadbackHr\":%ld",lockHr);
        if(SUCCEEDED(lockHr)) {
          uint32_t hash=2166136261u; unsigned long long sum[4]{};
          for(UINT y=0;y<desc.Height;++y) {
            const auto row=static_cast<const unsigned char*>(rect.pBits)+y*rect.Pitch;
            for(UINT x=0;x<desc.Width*4;++x) {hash=(hash^row[x])*16777619u;sum[x%4]+=row[x];}
          }
          fprintf(logFile,",\"textureHash\":%u,\"textureBgraSums\":[%llu,%llu,%llu,%llu]",hash,sum[0],sum[1],sum[2],sum[3]);
          t->UnlockRect(0);
        }
      }
    }
    texture->Release();
  }
  if(vs) vs->Release(); if(ps) ps->Release();
  fputs("}\n",logFile);
}

// Experimental, opt-in re-expression of an SDK-validated FVF-compatible declaration.
// The original declaration object is restored after the forwarded draw.
struct ScopedFvf {
  IDirect3DDevice9* device;
  IDirect3DVertexDeclaration9* saved=nullptr;
  bool changed=false;
  explicit ScopedFvf(IDirect3DDevice9* d):device(d) {
    if(!fvfFromDeclaration) return;
    IDirect3DVertexShader9* vs=nullptr;
    if(FAILED(d->GetVertexShader(&vs))) return;
    if(vs) { vs->Release(); return; }
    if(FAILED(d->GetVertexDeclaration(&saved)) || !saved) return;
    D3DVERTEXELEMENT9 elements[MAXD3DDECLLENGTH+1]; UINT count=MAXD3DDECLLENGTH+1; DWORD fvf=0;
    if(SUCCEEDED(saved->GetDeclaration(elements,&count)) && count<=MAXD3DDECLLENGTH+1 &&
       SUCCEEDED(fvfFromDeclaration(elements,&fvf)) && fvf!=0) {
      const HRESULT hr=d->SetFVF(fvf); changed=SUCCEEDED(hr);
      if(logFile && records<16384 && frameId%300==0) fprintf(logFile,"{\"event\":\"normalize_fvf\",\"frame\":%u,\"draw\":%u,\"fvf\":%lu,\"hr\":%ld}\n",frameId,drawId,fvf,hr);
    }
  }
  ~ScopedFvf() {
    if(changed) device->SetVertexDeclaration(saved);
    if(saved) saved->Release();
  }
};

// Remix 1.5.2 scales SetViewport by current backbuffer / creation backbuffer,
// even without a resolution override. Compensate only during each draw and
// restore the client-visible viewport immediately afterwards. This applies to
// offscreen targets too, since the upstream scaling does not distinguish them.
struct ScopedViewport {
  IDirect3DDevice9* device; D3DVIEWPORT9 saved{}; bool changed=false;
  explicit ScopedViewport(IDirect3DDevice9* d):device(d) {
    if(!viewportScale || sizeof(void*)!=4) return;
    const auto found=presentations.find(d);
    if(found==presentations.end()) return;
    const auto& p=found->second;
    if(!p.width || !p.height || !p.initialWidth || !p.initialHeight ||
       (p.width==p.initialWidth && p.height==p.initialHeight) || FAILED(d->GetViewport(&saved))) return;
    const float sx=float(p.width)/p.initialWidth, sy=float(p.height)/p.initialHeight;
    auto inverse=[](DWORD value,float scale) {return static_cast<DWORD>(std::ceil(double(value)/scale));};
    D3DVIEWPORT9 adjusted=saved;
    adjusted.X=inverse(saved.X,sx);adjusted.Y=inverse(saved.Y,sy);
    adjusted.Width=inverse(saved.Width,sx);adjusted.Height=inverse(saved.Height,sy);
    changed=SUCCEEDED(d->SetViewport(&adjusted));
    if(logFile && frameId<traceUntilFrame && drawId<3) fprintf(logFile,"{\"event\":\"viewport_scale\",\"frame\":%u,\"draw\":%u,\"logical\":[%lu,%lu],\"forwarded\":[%lu,%lu],\"scale\":[%.9g,%.9g]}\n",
      frameId,drawId,saved.Width,saved.Height,adjusted.Width,adjusted.Height,sx,sy);
  }
  ~ScopedViewport() {if(changed) device->SetViewport(&saved);}
};

// Winx sky layers are early fixed-function draws centered exactly on the camera
// with depth test/write disabled. Tag their viewport depth for stock Remix sky
// classification. Preserve the game's viewport and never alter its matrices.
struct ScopedSky {
  IDirect3DDevice9* device; D3DVIEWPORT9 saved{}; bool changed=false;
  explicit ScopedSky(IDirect3DDevice9* d):device(d) {
    if(!skyLayers || drawId>16 || uiStarted.count(d)) return;
    DWORD z=1,zw=1; D3DMATRIX w{},v{},p{};
    if(FAILED(d->GetRenderState(D3DRS_ZENABLE,&z)) || z ||
       FAILED(d->GetRenderState(D3DRS_ZWRITEENABLE,&zw)) || zw ||
       FAILED(d->GetTransform(D3DTS_PROJECTION,&p)) || p._34!=1.0f || p._44!=0.0f ||
       FAILED(d->GetTransform(D3DTS_WORLD,&w)) || FAILED(d->GetTransform(D3DTS_VIEW,&v))) return;
    IDirect3DVertexShader9* shader=nullptr; if(FAILED(d->GetVertexShader(&shader))) return;
    if(shader) {shader->Release();return;}
    const float* world=reinterpret_cast<const float*>(&w);const float* view=reinterpret_cast<const float*>(&v);
    for(unsigned j=0;j<3;++j) {
      double atCamera=view[12+j];
      for(unsigned k=0;k<3;++k) atCamera+=double(world[12+k])*view[k*4+j];
      if(!std::isfinite(atCamera) || std::fabs(atCamera)>0.05) return;
    }
    IDirect3DSurface9* target=nullptr; d->GetRenderTarget(0,&target);
    const auto known=primaryTargets.find(d);
    const bool primary=target && known!=primaryTargets.end() && target==known->second;
    if(target) target->Release(); if(!primary || FAILED(d->GetViewport(&saved))) return;
    auto sky=saved;sky.MinZ=sky.MaxZ=1.0f;
    changed=SUCCEEDED(d->SetViewport(&sky));
    if(logFile && (frameId<traceUntilFrame || frameId%300==0)) fprintf(logFile,"{\"event\":\"sky_layer\",\"frame\":%u,\"draw\":%u,\"tagged\":%s}\n",frameId,drawId,changed?"true":"false");
  }
  ~ScopedSky() {if(changed) device->SetViewport(&saved);}
};

// Trigger Remix's UI boundary with a zero-area primitive. Real UI draws retain
// Z writes: Winx uses depth to layer loading images, backgrounds and diary pages.
static void PrepareUi(IDirect3DDevice9* d) {
    if(!orthographicUi || uiStarted.count(d)) return;
    D3DMATRIX projection{};
    if(FAILED(d->GetTransform(D3DTS_PROJECTION,&projection))) return;
    const bool orthographic=projection._44==1.0f && projection._14==0.0f && projection._24==0.0f && projection._34==0.0f;
    // Observed Winx menu backdrop camera: unrotated, eye (0,0,-28.8675117),
    // 60-degree horizontal perspective. Only recognize it at the frame start.
    D3DMATRIX view{}; bool backdrop=false;
    if(menuBackground && drawId<=3 && !orthographic && SUCCEEDED(d->GetTransform(D3DTS_VIEW,&view))) {
      const float expected[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,28.8675117f,1};
      backdrop=true;
      for(unsigned i=0;i<16;++i) if(std::fabs(reinterpret_cast<const float*>(&view)[i]-expected[i])>0.0001f) backdrop=false;
      backdrop=backdrop && std::fabs(projection._11-1.7320509f)<0.0001f && projection._34==1.0f && projection._44==0.0f;
    }
    if(!orthographic && !backdrop) return;
    IDirect3DVertexShader9* vs=nullptr;
    if(FAILED(d->GetVertexShader(&vs))) return;
    if(vs) {vs->Release();return;}
    IDirect3DSurface9* target=nullptr;
    d->GetRenderTarget(0,&target);
    const auto known=primaryTargets.find(d);
    const bool primary=target && known!=primaryTargets.end() && target==known->second;
    if(target) target->Release();
    if(!primary) return;
    DWORD saved=0; IDirect3DVertexDeclaration9* declaration=nullptr;
    IDirect3DVertexBuffer9* stream=nullptr; UINT offset=0,stride=0;
    if(FAILED(d->GetRenderState(D3DRS_ZWRITEENABLE,&saved)) ||
       FAILED(d->GetVertexDeclaration(&declaration)) || !declaration) return;
    if(FAILED(d->GetStreamSource(0,&stream,&offset,&stride))) { declaration->Release(); return; }
    const float vertices[9]{};
    HRESULT hr=D3DERR_INVALIDCALL;
    if(backdrop) { D3DMATRIX marker{};marker._11=marker._22=marker._33=marker._44=1.0f;d->SetTransform(D3DTS_PROJECTION,&marker); }
    if(SUCCEEDED(d->SetFVF(D3DFVF_XYZ)) && SUCCEEDED(d->SetRenderState(D3DRS_ZWRITEENABLE,FALSE))) {
      using F=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,const void*,UINT);
      hr=Original<F>(d,83)(d,D3DPT_TRIANGLELIST,1,vertices,3*sizeof(float));
      if(SUCCEEDED(hr)) uiStarted.insert(d);
    }
    d->SetRenderState(D3DRS_ZWRITEENABLE,saved);
    if(backdrop) d->SetTransform(D3DTS_PROJECTION,&projection);
    d->SetVertexDeclaration(declaration);
    d->SetStreamSource(0,stream,offset,stride);
    if(stream) stream->Release(); declaration->Release();
    if(logFile && (frameId<traceUntilFrame || frameId%300==0)) fprintf(logFile,"{\"event\":\"ui_boundary\",\"frame\":%u,\"draw\":%u,\"backdrop\":%s,\"hr\":%ld}\n",frameId,drawId,backdrop?"true":"false",hr);
}

static HRESULT STDMETHODCALLTYPE SetLight(IDirect3DDevice9* d,DWORD index,const D3DLIGHT9* light) {
  using F=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,DWORD,const D3DLIGHT9*);
  const HRESULT hr=Original<F>(d,51)(d,index,light);
  if(logFile && light) {
    static std::map<DWORD,D3DLIGHT9> previous;
    static unsigned lightRecords=0;
    if(lightRecords<2048 && (!previous.count(index) || memcmp(&previous[index],light,sizeof(*light))!=0)) {
      previous[index]=*light;++lightRecords;
      fprintf(logFile,"{\"event\":\"light\",\"frame\":%u,\"index\":%lu,\"type\":%u,\"diffuse\":[%.9g,%.9g,%.9g],\"ambient\":[%.9g,%.9g,%.9g],\"position\":[%.9g,%.9g,%.9g],\"direction\":[%.9g,%.9g,%.9g],\"range\":%.9g,\"attenuation\":[%.9g,%.9g,%.9g],\"hr\":%ld}\n",
        frameId,index,light->Type,light->Diffuse.r,light->Diffuse.g,light->Diffuse.b,light->Ambient.r,light->Ambient.g,light->Ambient.b,
        light->Position.x,light->Position.y,light->Position.z,light->Direction.x,light->Direction.y,light->Direction.z,
        light->Range,light->Attenuation0,light->Attenuation1,light->Attenuation2,hr);
    }
  }
  return hr;
}

// Opt-in experiment: retransmit the existing CPU contents of managed textures
// immediately before first use. No pixels are generated or edited here.
static void ResubmitTextures(IDirect3DDevice9* device) {
  if(!resubmitTextures || sizeof(void*)!=4) return;
  for(DWORD stage=0;stage<8;++stage) {
    IDirect3DBaseTexture9* base=nullptr;
    if(FAILED(device->GetTexture(stage,&base)) || !base) continue;
    if(base->GetType()==D3DRTYPE_TEXTURE && !submittedTextures.count(base)) {
      auto texture=static_cast<IDirect3DTexture9*>(base); D3DSURFACE_DESC desc{};
      if(SUCCEEDED(texture->GetLevelDesc(0,&desc)) && desc.Pool==D3DPOOL_MANAGED) {
        submittedTextures.insert(base);
        for(UINT level=0;level<texture->GetLevelCount();++level) {
          D3DLOCKED_RECT rect{}; HRESULT hr=texture->LockRect(level,&rect,nullptr,0);
          if(SUCCEEDED(hr)) hr=texture->UnlockRect(level);
          if(logFile) fprintf(logFile,"{\"event\":\"resubmit_texture\",\"frame\":%u,\"width\":%u,\"height\":%u,\"level\":%u,\"hr\":%ld}\n",frameId,desc.Width,desc.Height,level,hr);
        }
      }
    }
    base->Release();
  }
}

template<class Buffer, class Desc> static HRESULT STDMETHODCALLTYPE BufferLockCall(Buffer* b,UINT offset,UINT size,void** data,DWORD flags) {
  using F=HRESULT(STDMETHODCALLTYPE*)(Buffer*,UINT,UINT,void**,DWORD);
  const HRESULT hr=Original<F>(b,11)(b,offset,size,data,flags);
  std::lock_guard<std::recursive_mutex> lock(guard);
  Desc desc{}; b->GetDesc(&desc);
  const bool dynamic=(desc.Usage&D3DUSAGE_DYNAMIC)!=0;
  if(SUCCEEDED(hr) && data && *data && logFile && bufferRecords<1024 && (!dynamic || dynamicBufferRecords<20)) {
    if(dynamic) ++dynamicBufferRecords;
    const UINT length=offset<=desc.Size ? (size ? (size<desc.Size-offset ? size : desc.Size-offset) : desc.Size-offset) : 0;
    bufferLocks[b]={*data,length,flags};
    fprintf(logFile,"{\"event\":\"buffer_lock\",\"buffer\":%llu,\"frame\":%u,\"type\":%u,\"offset\":%u,\"size\":%u,\"length\":%u,\"usage\":%lu,\"pool\":%u,\"flags\":%lu,\"hr\":%ld}\n",static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(b)),frameId,desc.Type,offset,size,length,desc.Usage,desc.Pool,flags,hr);
    ++bufferRecords;
  }
  return hr;
}
template<class Buffer> static HRESULT STDMETHODCALLTYPE BufferUnlockCall(Buffer* b) {
  std::lock_guard<std::recursive_mutex> lock(guard);
  auto found=bufferLocks.find(b);
  if(found!=bufferLocks.end()) {
    const auto& info=found->second;
    const auto bytes=static_cast<const unsigned char*>(info.data);
    uint32_t hash=2166136261u; for(UINT i=0;i<info.size;++i) hash=(hash^bytes[i])*16777619u;
    fprintf(logFile,"{\"event\":\"buffer_unlock\",\"buffer\":%llu,\"frame\":%u,\"fnv1a32\":%u,\"head\":[",static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(b)),frameId,hash);
    for(UINT i=0;i<info.size && i<32;++i) fprintf(logFile,"%s%u",i?",":"",bytes[i]);
    fputs("]}\n",logFile); bufferLocks.erase(found);
  }
  using F=HRESULT(STDMETHODCALLTYPE*)(Buffer*);
  return Original<F>(b,12)(b);
}
static HRESULT STDMETHODCALLTYPE CreateVB(IDirect3DDevice9* d,UINT size,DWORD usage,DWORD fvf,D3DPOOL pool,IDirect3DVertexBuffer9** result,HANDLE* shared) {
  using F=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,UINT,DWORD,DWORD,D3DPOOL,IDirect3DVertexBuffer9**,HANDLE*);
  const HRESULT hr=Original<F>(d,26)(d,size,usage,fvf,pool,result,shared);
  if(SUCCEEDED(hr) && result && *result) {
    Patch(*result,11,reinterpret_cast<void*>(BufferLockCall<IDirect3DVertexBuffer9,D3DVERTEXBUFFER_DESC>));
    Patch(*result,12,reinterpret_cast<void*>(BufferUnlockCall<IDirect3DVertexBuffer9>));
  }
  return hr;
}
static HRESULT STDMETHODCALLTYPE CreateIB(IDirect3DDevice9* d,UINT size,DWORD usage,D3DFORMAT format,D3DPOOL pool,IDirect3DIndexBuffer9** result,HANDLE* shared) {
  using F=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,UINT,DWORD,D3DFORMAT,D3DPOOL,IDirect3DIndexBuffer9**,HANDLE*);
  const HRESULT hr=Original<F>(d,27)(d,size,usage,format,pool,result,shared);
  if(SUCCEEDED(hr) && result && *result) {
    Patch(*result,11,reinterpret_cast<void*>(BufferLockCall<IDirect3DIndexBuffer9,D3DINDEXBUFFER_DESC>));
    Patch(*result,12,reinterpret_cast<void*>(BufferUnlockCall<IDirect3DIndexBuffer9>));
  }
  return hr;
}

// Optional diagnostic control through the stock public Remix API. The file is
// read at most twice per second; removing a key does not reset its live value.
// No executable commands, DLL loading, game memory access or source patching.
static void ApplyLiveConfig() {
  if(!liveConfigPath[0]) return;
  static ULONGLONG nextCheck=0; const auto now=GetTickCount64();
  if(now<nextCheck) return; nextCheck=now+500;
  static remixapi_Interface api{}; static bool attempted=false;
  if(!attempted) {
    attempted=true;
    auto init=reinterpret_cast<PFN_remixapi_InitializeLibrary>(GetProcAddress(backend,"remixapi_InitializeLibrary"));
    remixapi_InitializeLibraryInfo info{};
    info.sType=REMIXAPI_STRUCT_TYPE_INITIALIZE_LIBRARY_INFO;
    info.version=REMIXAPI_VERSION_MAKE(REMIXAPI_VERSION_MAJOR,REMIXAPI_VERSION_MINOR,REMIXAPI_VERSION_PATCH);
    const auto result=init?init(&info,&api):REMIXAPI_ERROR_CODE_NOT_INITIALIZED;
    if(logFile) fprintf(logFile,"{\"event\":\"live_config_init\",\"result\":%d}\n",result);
    if(result!=REMIXAPI_ERROR_CODE_SUCCESS) api={};
  }
  if(!api.SetConfigVariable) return;
  FILE* file=_wfsopen(liveConfigPath,L"rb",_SH_DENYNO); if(!file) return;
  std::string content; char chunk[1024]; size_t count=0;
  while((count=fread(chunk,1,sizeof(chunk),file))!=0 && content.size()<16384) content.append(chunk,count);
  fclose(file); if(content.size()>=16384) return;
  static std::string previous;
  if(content==previous) return; previous=content;
  std::istringstream input(content); std::string line;
  static std::map<std::string,std::string> applied;
  auto trim=[](std::string s) {
    const auto first=s.find_first_not_of(" \t\r");
    return first==std::string::npos?std::string():s.substr(first,s.find_last_not_of(" \t\r")-first+1);
  };
  while(std::getline(input,line)) {
    const auto split=line.find('='); if(split==std::string::npos) continue;
    const auto key=trim(line.substr(0,split)),value=trim(line.substr(split+1));
    if(key.rfind("rtx.",0)!=0 || key.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.")!=std::string::npos ||
       value.empty() || value.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_., +-()")!=std::string::npos) continue;
    if(applied.count(key) && applied[key]==value) continue;
    const auto result=api.SetConfigVariable(key.c_str(),value.c_str());
    if(result==REMIXAPI_ERROR_CODE_SUCCESS) applied[key]=value;
    if(logFile) fprintf(logFile,"{\"event\":\"live_config\",\"frame\":%u,\"key\":\"%s\",\"value\":\"%s\",\"result\":%d}\n",frameId,key.c_str(),value.c_str(),result);
  }
}

static HRESULT STDMETHODCALLTYPE Present(IDirect3DDevice9* d,const RECT* a,const RECT* b,HWND c,const RGNDATA* e) {
  ApplyLiveConfig();
  using F=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,const RECT*,const RECT*,HWND,const RGNDATA*);
  RECT source{},destination{};
  const auto state=presentations.find(d);
  if(fitWindow && state!=presentations.end() && state->second.windowed && !a && !b && !c &&
     GetClientRect(state->second.window,&destination) && destination.right>0 && destination.bottom>0) {
    source={0,0,static_cast<LONG>(state->second.width),static_cast<LONG>(state->second.height)};
    a=&source; b=&destination;
  }
  if(logFile && (frameId<traceUntilFrame || frameId%300==0)) {
    fprintf(logFile,"{\"event\":\"present\",\"frame\":%u,\"draws\":%u,\"source\":[%ld,%ld,%ld,%ld],\"destination\":[%ld,%ld,%ld,%ld],\"rects\":[%d,%d]}\n",
      frameId,drawId,a?a->left:0,a?a->top:0,a?a->right:0,a?a->bottom:0,b?b->left:0,b?b->top:0,b?b->right:0,b?b->bottom:0,a!=nullptr,b!=nullptr);
  }
  const HRESULT hr=Original<F>(d,17)(d,a,b,c,e);
  uiStarted.erase(d);
  std::lock_guard<std::recursive_mutex> lock(guard);
  if(logFile) fflush(logFile);
  ++frameId; drawId=0; triggered=(GetAsyncKeyState(VK_F8)&1)!=0;
  if(triggered) { records=0; traceUntilFrame=frameId+120; }
  return hr;
}
static HRESULT STDMETHODCALLTYPE CreateTexture(IDirect3DDevice9* d,UINT width,UINT height,UINT levels,DWORD usage,D3DFORMAT format,D3DPOOL pool,IDirect3DTexture9** result,HANDLE* shared) {
  using F=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,UINT,UINT,UINT,DWORD,D3DFORMAT,D3DPOOL,IDirect3DTexture9**,HANDLE*);
  UINT forwardedLevels=levels;
  // D3D9 halves each dimension with truncation down to 1. The 1.5.2 bridge
  // uses ceil(log2(maxDimension))+1 for Levels=0, which overcounts NPOT textures.
  if(explicitMipLevels && levels==0 && width && height) {
    forwardedLevels=1;
    for(UINT size=width>height?width:height;size>1;size>>=1) ++forwardedLevels;
  }
  const HRESULT hr=Original<F>(d,23)(d,width,height,forwardedLevels,usage,format,pool,result,shared);
  if(SUCCEEDED(hr) && result && *result) { submittedTextures.erase(*result); readTextures.erase(*result); }
  if(logFile && textureRecords++<2048) {
    const bool ok=SUCCEEDED(hr) && result && *result;
    fprintf(logFile,"{\"event\":\"create_texture\",\"frame\":%u,\"width\":%u,\"height\":%u,\"levels\":%u,\"forwardedLevels\":%u,\"usage\":%lu,\"format\":%u,\"pool\":%u,\"hr\":%ld,\"actualLevels\":%u,\"texture\":%llu}\n",frameId,width,height,levels,forwardedLevels,usage,format,pool,hr,ok?(*result)->GetLevelCount():0,ok?static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(*result)):0);
    fflush(logFile);
  }
  return hr;
}
static HRESULT STDMETHODCALLTYPE SwapPresent(IDirect3DSwapChain9* d,const RECT* a,const RECT* b,HWND c,const RGNDATA* e,DWORD flags) {
  using F=HRESULT(STDMETHODCALLTYPE*)(IDirect3DSwapChain9*,const RECT*,const RECT*,HWND,const RGNDATA*,DWORD);
  const HRESULT hr=Original<F>(d,3)(d,a,b,c,e,flags);
  std::lock_guard<std::recursive_mutex> lock(guard);
  if(logFile) fflush(logFile);
  ++frameId; drawId=0;
  return hr;
}
static HRESULT STDMETHODCALLTYPE GetSwapChain(IDirect3DDevice9* d,UINT index,IDirect3DSwapChain9** result) {
  using F=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,UINT,IDirect3DSwapChain9**);
  const HRESULT hr=Original<F>(d,14)(d,index,result);
  if(SUCCEEDED(hr) && result && *result) Patch(*result,3,reinterpret_cast<void*>(SwapPresent));
  return hr;
}
static HRESULT STDMETHODCALLTYPE Reset(IDirect3DDevice9* d,D3DPRESENT_PARAMETERS* p) {
  using F=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,D3DPRESENT_PARAMETERS*);
  LogPresentation("reset_request",p);
  traceUntilFrame=frameId+120; records=0;
  const HRESULT hr=Original<F>(d,16)(d,p);
  if(SUCCEEDED(hr)) {
    RememberPrimaryTarget(d);
    D3DDEVICE_CREATION_PARAMETERS creation{}; d->GetCreationParameters(&creation);
    RememberPresentation(d,*p,creation.hFocusWindow);
  }
  std::lock_guard<std::recursive_mutex> lock(guard);
  if(logFile) { fprintf(logFile,"{\"event\":\"reset\",\"hr\":%ld}\n",hr); fflush(logFile); }
  return hr;
}
static HRESULT STDMETHODCALLTYPE Draw(IDirect3DDevice9* d,D3DPRIMITIVETYPE t,UINT start,UINT count) {
  Observe(d,"DrawPrimitive",t,count);
  ResubmitTextures(d);
  ScopedFvf normalize(d);
  ScopedViewport viewport(d);
  ScopedSky sky(d);
  PrepareUi(d);
  using F=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,UINT);
  return Original<F>(d,81)(d,t,start,count);
}
static HRESULT STDMETHODCALLTYPE DrawIndexed(IDirect3DDevice9* d,D3DPRIMITIVETYPE t,INT base,UINT min,UINT num,UINT start,UINT count) {
  Observe(d,"DrawIndexedPrimitive",t,count);
  ResubmitTextures(d);
  ScopedFvf normalize(d);
  ScopedViewport viewport(d);
  ScopedSky sky(d);
  PrepareUi(d);
  using F=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,D3DPRIMITIVETYPE,INT,UINT,UINT,UINT,UINT);
  return Original<F>(d,82)(d,t,base,min,num,start,count);
}
static HRESULT STDMETHODCALLTYPE DrawUP(IDirect3DDevice9* d,D3DPRIMITIVETYPE t,UINT count,const void* data,UINT stride) {
  Observe(d,"DrawPrimitiveUP",t,count);
  ResubmitTextures(d);
  ScopedFvf normalize(d);
  ScopedViewport viewport(d);
  ScopedSky sky(d);
  PrepareUi(d);
  using F=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,const void*,UINT);
  return Original<F>(d,83)(d,t,count,data,stride);
}
static HRESULT STDMETHODCALLTYPE DrawIndexedUP(IDirect3DDevice9* d,D3DPRIMITIVETYPE t,UINT min,UINT num,UINT count,const void* indices,D3DFORMAT format,const void* data,UINT stride) {
  Observe(d,"DrawIndexedPrimitiveUP",t,count);
  ResubmitTextures(d);
  ScopedFvf normalize(d);
  ScopedViewport viewport(d);
  ScopedSky sky(d);
  PrepareUi(d);
  using F=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,UINT,UINT,const void*,D3DFORMAT,const void*,UINT);
  return Original<F>(d,84)(d,t,min,num,count,indices,format,data,stride);
}
static HRESULT STDMETHODCALLTYPE CreateDevice(IDirect3D9* d,UINT adapter,D3DDEVTYPE type,HWND window,DWORD flags,D3DPRESENT_PARAMETERS* p,IDirect3DDevice9** result) {
  using F=HRESULT(STDMETHODCALLTYPE*)(IDirect3D9*,UINT,D3DDEVTYPE,HWND,DWORD,D3DPRESENT_PARAMETERS*,IDirect3DDevice9**);
  LogPresentation("create_device_request",p,window);
  const HRESULT hr=Original<F>(d,16)(d,adapter,type,window,flags,p,result);
  if(SUCCEEDED(hr) && result && *result) {
    RememberPrimaryTarget(*result);
    RememberPresentation(*result,*p,window);
    // Server forwards game Present via its swap chain; sample that boundary on x64.
    if(sizeof(void*)==8) {
      Patch(*result,14,reinterpret_cast<void*>(GetSwapChain));
      IDirect3DSwapChain9* swap=nullptr;
      if(SUCCEEDED((*result)->GetSwapChain(0,&swap)) && swap) swap->Release();
    }
    Patch(*result,16,reinterpret_cast<void*>(Reset)); Patch(*result,17,reinterpret_cast<void*>(Present));
    Patch(*result,26,reinterpret_cast<void*>(CreateVB)); Patch(*result,27,reinterpret_cast<void*>(CreateIB));
    Patch(*result,23,reinterpret_cast<void*>(CreateTexture));
    Patch(*result,51,reinterpret_cast<void*>(SetLight));
    Patch(*result,81,reinterpret_cast<void*>(Draw)); Patch(*result,82,reinterpret_cast<void*>(DrawIndexed));
    Patch(*result,83,reinterpret_cast<void*>(DrawUP)); Patch(*result,84,reinterpret_cast<void*>(DrawIndexedUP));
  }
  if(logFile) { fprintf(logFile,"{\"event\":\"create_device\",\"hr\":%ld}\n",hr); fflush(logFile); }
  return hr;
}

extern "C" IDirect3D9* WINAPI ProxyCreate9(UINT sdk) {
  auto f=Proc<IDirect3D9*(WINAPI*)(UINT)>("Direct3DCreate9");
  IDirect3D9* result=f?f(sdk):nullptr;
  if(result) Patch(result,16,reinterpret_cast<void*>(CreateDevice));
  return result;
}
extern "C" HRESULT WINAPI ProxyCreate9Ex(UINT sdk,IDirect3D9Ex** result) {
  auto f=Proc<HRESULT(WINAPI*)(UINT,IDirect3D9Ex**)>("Direct3DCreate9Ex");
  // Ex creation is forwarded; Ex-only device creation/presentation is not sampled.
  const HRESULT hr=f?f(sdk,result):D3DERR_NOTAVAILABLE;
  if(SUCCEEDED(hr) && result && *result) Patch(*result,16,reinterpret_cast<void*>(CreateDevice));
  return hr;
}
extern "C" int WINAPI ProxyBegin(D3DCOLOR color,LPCWSTR name) { auto f=Proc<int(WINAPI*)(D3DCOLOR,LPCWSTR)>("D3DPERF_BeginEvent"); return f?f(color,name):-1; }
extern "C" int WINAPI ProxyEnd() { auto f=Proc<int(WINAPI*)()>("D3DPERF_EndEvent"); return f?f():-1; }
extern "C" void WINAPI ProxyMarker(D3DCOLOR color,LPCWSTR name) { auto f=Proc<void(WINAPI*)(D3DCOLOR,LPCWSTR)>("D3DPERF_SetMarker"); if(f) f(color,name); }
extern "C" void WINAPI ProxyRegion(D3DCOLOR color,LPCWSTR name) { auto f=Proc<void(WINAPI*)(D3DCOLOR,LPCWSTR)>("D3DPERF_SetRegion"); if(f) f(color,name); }
extern "C" void WINAPI ProxyOptions(DWORD options) { auto f=Proc<void(WINAPI*)(DWORD)>("D3DPERF_SetOptions"); if(f) f(options); }
extern "C" DWORD WINAPI ProxyStatus() { auto f=Proc<DWORD(WINAPI*)()>("D3DPERF_GetStatus"); return f?f():0; }
extern "C" BOOL WINAPI ProxyRepeat() { auto f=Proc<BOOL(WINAPI*)()>("D3DPERF_QueryRepeatFrame"); return f?f():FALSE; }
extern "C" void WINAPI ProxyMaximized(BOOL enabled) { auto f=Proc<void(WINAPI*)(BOOL)>("Direct3D9EnableMaximizedWindowedModeShim"); if(f) f(enabled); }

BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID) {
  if(reason==DLL_PROCESS_ATTACH) { selfModule=module; DisableThreadLibraryCalls(module); }
  // Loaded for the entire game process lifetime. No work/FreeLibrary under loader lock.
  return TRUE;
}
