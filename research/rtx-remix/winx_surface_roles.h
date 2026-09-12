// Own D3D9/Remix policy. No game IDs, asset names or known hashes.
#define XXH_INLINE_ALL
#include "third-party/xxhash.h"

static bool autoSurfaceRoles, keepAutoSurfaceRolesForComparison;
static uint64_t surfaceResourceEpoch;
static std::map<std::string,unsigned> opaqueSurfaceDraws;
static std::map<IDirect3DBaseTexture9*,uint64_t> surfaceTextureHashes;
static FILE* surfaceRoleLog;
static unsigned surfaceBases, surfaceOverlays, surfaceStandalone, surfaceUnknown;

static remixapi_Interface* GetRemixApi() {
  static remixapi_Interface api{};static bool attempted=false;
  if(!attempted) {
    attempted=true;
    auto init=reinterpret_cast<PFN_remixapi_InitializeLibrary>(GetProcAddress(backend,"remixapi_InitializeLibrary"));
    remixapi_InitializeLibraryInfo info{};
    info.sType=REMIXAPI_STRUCT_TYPE_INITIALIZE_LIBRARY_INFO;
    info.version=REMIXAPI_VERSION_MAKE(REMIXAPI_VERSION_MAJOR,REMIXAPI_VERSION_MINOR,REMIXAPI_VERSION_PATCH);
    const auto result=init?init(&info,&api):REMIXAPI_ERROR_CODE_NOT_INITIALIZED;
    if(logFile) fprintf(logFile,"{\"event\":\"remix_api_init\",\"result\":%d}\n",result);
    if(surfaceRoleLog) {fprintf(surfaceRoleLog,"{\"event\":\"api_init\",\"result\":%d}\n",result);fflush(surfaceRoleLog);}
    if(result!=REMIXAPI_ERROR_CODE_SUCCESS) api={};
  }
  return api.SetConfigVariable?&api:nullptr;
}

#include "winx_surface_submit.h"

static void InitializeSurfaceRoles() {
  wchar_t option[8]{},path[MAX_PATH]{};
  autoSurfaceRoles=sizeof(void*)==4 && GetEnvironmentVariableW(L"WINX_REMIX_AUTO_SURFACE_ROLES",option,8) && wcscmp(option,L"1")==0;
  if(autoSurfaceRoles && GetEnvironmentVariableW(L"WINX_REMIX_SURFACE_AUDIT",path,MAX_PATH))
    surfaceRoleLog=_wfsopen(path,L"wb",_SH_DENYNO);
  if(autoSurfaceRoles) GetEnvironmentVariableW(L"WINX_REMIX_SURFACE_ASSETS",surfaceAssetDirectory,MAX_PATH);
}
static void ClearSurfaceBases() {opaqueSurfaceDraws.clear();++surfaceResourceEpoch;}
static void EndSurfaceRoleFrame() {
  if(surfaceRoleLog && (frameId%300==0 || triggered)) {
    fprintf(surfaceRoleLog,"{\"event\":\"frame\",\"frame\":%u,\"bases\":%u,\"overlays\":%u,\"standalone\":%u,\"unknown\":%u,\"comparisonDisabled\":%s}\n",
      frameId,surfaceBases,surfaceOverlays,surfaceStandalone,surfaceUnknown,keepAutoSurfaceRolesForComparison?"true":"false");
    fflush(surfaceRoleLog);
  }
  surfaceBases=surfaceOverlays=surfaceStandalone=surfaceUnknown=0;ClearSurfaceBases();
  RetireSurfaceMeshes();
}

// Same first-use XXH3 mip identity as stock Remix, only for the verified
// tightly packed managed 32-bit path. This identifies the current texture;
// the hash is never used to decide its role. Unsupported formats stay ordinary.
static uint64_t BoundSurfaceTextureHash(IDirect3DDevice9* d) {
  IDirect3DBaseTexture9* base=nullptr;
  if(FAILED(d->GetTexture(0,&base)) || !base) return 0;
  const auto cached=surfaceTextureHashes.find(base);
  if(cached!=surfaceTextureHashes.end()) {const auto hash=cached->second;base->Release();return hash;}
  uint64_t hash=0;
  if(base->GetType()==D3DRTYPE_TEXTURE && surfaceTextureHashes.size()<4096) {
    auto texture=static_cast<IDirect3DTexture9*>(base);D3DSURFACE_DESC desc{};
    if(SUCCEEDED(texture->GetLevelDesc(0,&desc)) && desc.Pool==D3DPOOL_MANAGED &&
       (desc.Format==D3DFMT_A8R8G8B8 || desc.Format==D3DFMT_X8R8G8B8) &&
       desc.Width && desc.Height && uint64_t(desc.Width)*desc.Height<=4194304) {
      D3DLOCKED_RECT rect{};
      if(SUCCEEDED(texture->LockRect(0,&rect,nullptr,D3DLOCK_READONLY))) {
        if(rect.pBits && rect.Pitch==static_cast<INT>(desc.Width*4))
          hash=XXH3_64bits(rect.pBits,size_t(desc.Width)*desc.Height*4);
        texture->UnlockRect(0);
      }
    }
    if(hash) surfaceTextureHashes.emplace(base,hash);
  }
  base->Release();return hash;
}

struct ScopedSurfaceRole {
  IDirect3DDevice9* device;
  remixapi_Interface* api=nullptr;
  std::string key;
  bool opaque=false, scopedDecal=false, blended=false;
  unsigned baseDraw=0;
  uint64_t textureHash=0;
  template<class T> void append(const T& value) {key.append(reinterpret_cast<const char*>(&value),sizeof(value));}
  bool rs(D3DRENDERSTATETYPE state,DWORD& value) {return SUCCEEDED(device->GetRenderState(state,&value));}

  ScopedSurfaceRole(IDirect3DDevice9* d,D3DPRIMITIVETYPE type,INT base,UINT minVertex,UINT vertices,UINT start,UINT count):device(d) {
    if(!autoSurfaceRoles || keepAutoSurfaceRolesForComparison || uiStarted.count(d)) return;
    if((type!=D3DPT_TRIANGLELIST && type!=D3DPT_TRIANGLESTRIP) || !count || !vertices) return;
    DWORD z=0,zw=0,mask=0,clip=0,vblend=0,indexedBlend=0;
    if(!rs(D3DRS_ZENABLE,z) || z!=D3DZB_TRUE || !rs(D3DRS_ZWRITEENABLE,zw) || !zw ||
       !rs(D3DRS_COLORWRITEENABLE,mask) || (mask&7)!=7 ||
       !rs(D3DRS_CLIPPLANEENABLE,clip) || clip || !rs(D3DRS_VERTEXBLEND,vblend) || vblend ||
       !rs(D3DRS_INDEXEDVERTEXBLENDENABLE,indexedBlend) || indexedBlend) return;
    IDirect3DSurface9* target=nullptr;d->GetRenderTarget(0,&target);
    const auto known=primaryTargets.find(d);
    const bool primary=target && known!=primaryTargets.end() && known->second==target;
    if(target) target->Release();if(!primary) return;
    D3DMATRIX world{},view{},projection{};D3DVIEWPORT9 viewport{};
    if(FAILED(d->GetTransform(D3DTS_PROJECTION,&projection)) || projection._34!=1.f || projection._44!=0.f ||
       FAILED(d->GetTransform(D3DTS_WORLD,&world)) || FAILED(d->GetTransform(D3DTS_VIEW,&view)) || FAILED(d->GetViewport(&viewport))) return;
    IDirect3DVertexShader9* vs=nullptr;IDirect3DPixelShader9* ps=nullptr;
    const bool gotShaders=SUCCEEDED(d->GetVertexShader(&vs)) && SUCCEEDED(d->GetPixelShader(&ps));
    const bool fixed=gotShaders && !vs && !ps;
    if(vs) vs->Release();if(ps) ps->Release();if(!fixed) return;
    DWORD test=0,func=0,ref=0,blend=0,src=0,dst=0,op=0,cull=0,zfunc=0,clipping=0;
    if(!rs(D3DRS_ALPHATESTENABLE,test) || !rs(D3DRS_ALPHAFUNC,func) || !rs(D3DRS_ALPHAREF,ref) ||
       !rs(D3DRS_ALPHABLENDENABLE,blend) || !rs(D3DRS_SRCBLEND,src) || !rs(D3DRS_DESTBLEND,dst) ||
       !rs(D3DRS_BLENDOP,op) || !rs(D3DRS_CULLMODE,cull) || !rs(D3DRS_ZFUNC,zfunc) || !rs(D3DRS_CLIPPING,clipping)) return;
    const bool noAlphaTest=!test || func==D3DCMP_ALWAYS || (func==D3DCMP_GREATEREQUAL && ref==0);
    opaque=noAlphaTest && (!blend || (src==D3DBLEND_ONE && dst==D3DBLEND_ZERO && op==D3DBLENDOP_ADD));
    blended=blend && src==D3DBLEND_SRCALPHA && dst==D3DBLEND_INVSRCALPHA && op==D3DBLENDOP_ADD;
    if(!opaque && !blended) return;
    if(zfunc!=D3DCMP_LESSEQUAL && zfunc!=D3DCMP_EQUAL) return;
    IDirect3DVertexBuffer9* vb=nullptr;IDirect3DIndexBuffer9* ib=nullptr;IDirect3DVertexDeclaration9* decl=nullptr;
    UINT offset=0,stride=0,elements=MAXD3DDECLLENGTH+1;
    D3DVERTEXELEMENT9 layout[MAXD3DDECLLENGTH+1]{};
    bool valid=SUCCEEDED(d->GetStreamSource(0,&vb,&offset,&stride)) && vb && stride &&
      SUCCEEDED(d->GetIndices(&ib)) && ib && SUCCEEDED(d->GetVertexDeclaration(&decl)) && decl &&
      SUCCEEDED(decl->GetDeclaration(layout,&elements)) && elements<=MAXD3DDECLLENGTH+1;
    bool position=false;
    if(valid) for(UINT i=0;i<elements && layout[i].Stream!=0xff;++i) {
      if(layout[i].Stream!=0) valid=false;
      if(layout[i].Usage==D3DDECLUSAGE_POSITION && layout[i].UsageIndex==0 && layout[i].Type==D3DDECLTYPE_FLOAT3) position=true;
      if(layout[i].Usage==D3DDECLUSAGE_POSITIONT) valid=false;
    }
    if(valid && position) {
      append(d);append(target);append(vb);append(ib);append(surfaceResourceEpoch);
      append(offset);append(stride);append(type);append(base);append(minVertex);append(vertices);append(start);append(count);
      append(world);append(view);append(projection);append(viewport);append(cull);append(zfunc);append(clipping);
      key.append(reinterpret_cast<const char*>(layout),elements*sizeof(*layout));
    }
    if(vb) vb->Release();if(ib) ib->Release();if(decl) decl->Release();
    if(key.empty()) {opaque=false;blended=false;return;}
    if(opaque) return;
    const auto found=opaqueSurfaceDraws.find(key);
    if(found==opaqueSurfaceDraws.end()) {++surfaceStandalone;return;}
    baseDraw=found->second;api=GetRemixApi();textureHash=BoundSurfaceTextureHash(d);
    if(!api || !textureHash) {++surfaceUnknown;return;}
    scopedDecal=SubmitSurfaceOverlay(d,type,base,minVertex,vertices,start,count,textureHash);
    if(scopedDecal) ++surfaceOverlays;else ++surfaceUnknown;
  }
  HRESULT complete(HRESULT hr) {
    if(SUCCEEDED(hr) && opaque && !key.empty() && opaqueSurfaceDraws.size()<4096) {
      opaqueSurfaceDraws.emplace(key,drawId);++surfaceBases;
    }
    if(surfaceRoleLog && !key.empty() && (frameId%300==0 || frameId<traceUntilFrame)) {
      fprintf(surfaceRoleLog,"{\"event\":\"draw_role\",\"frame\":%u,\"draw\":%u,\"role\":\"%s\",\"baseDraw\":%u,\"textureHash\":\"%016llX\",\"epoch\":%llu,\"hr\":%ld}\n",
        frameId,drawId,opaque?"opaque":scopedDecal?"overlay":baseDraw?"unsupported_overlay":"standalone",baseDraw,
        static_cast<unsigned long long>(textureHash),static_cast<unsigned long long>(surfaceResourceEpoch),hr);
    }
    return hr;
  }
};
