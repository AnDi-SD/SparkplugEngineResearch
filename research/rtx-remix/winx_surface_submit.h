// Own bounded translation of a recognized FFP overlay into explicit Remix API
// resources. No asset IDs, game logic, texture recoloring or runtime patches.
static wchar_t surfaceAssetDirectory[MAX_PATH]{};
struct SurfaceMesh { remixapi_MeshHandle handle; unsigned frame; size_t bytes; };
static std::map<uint64_t,SurfaceMesh> surfaceMeshes;
static std::map<uint64_t,remixapi_MaterialHandle> surfaceMaterials;
static size_t surfaceMeshBytes;
struct SurfaceBuffer { std::vector<uint8_t> bytes; bool complete=false; };
struct SurfaceWrite { const void* data; UINT offset,size,total; DWORD flags; };
static std::map<void*,SurfaceBuffer> surfaceBuffers;
static std::map<void*,SurfaceWrite> surfaceWrites;
static size_t surfaceBufferBytes;
static void ForgetSurfaceBuffer(void* b) {
  auto it=surfaceBuffers.find(b);
  if(it!=surfaceBuffers.end()) {surfaceBufferBytes-=it->second.bytes.size();surfaceBuffers.erase(it);}
  surfaceWrites.erase(b);
}
static void CaptureSurfaceWrite(void* b) {
  auto it=surfaceWrites.find(b);if(it==surfaceWrites.end()) return;
  const auto w=it->second;surfaceWrites.erase(it);
  if(!w.total || w.total>8*1024*1024 || w.offset>w.total || w.size>w.total-w.offset) {ForgetSurfaceBuffer(b);return;}
  auto found=surfaceBuffers.find(b);
  if(found==surfaceBuffers.end()) {
    if(surfaceBuffers.size()>=4096 || surfaceBufferBytes+w.total>64*1024*1024) return;
    found=surfaceBuffers.emplace(b,SurfaceBuffer{std::vector<uint8_t>(w.total),false}).first;surfaceBufferBytes+=w.total;
  }
  auto& snapshot=found->second;
  if(snapshot.bytes.size()!=w.total) {ForgetSurfaceBuffer(b);return;}
  if(w.flags&D3DLOCK_DISCARD) snapshot.complete=false;
  memcpy(snapshot.bytes.data()+w.offset,w.data,w.size);
  if(w.offset==0 && w.size==w.total) snapshot.complete=true;
}

static void RetireSurfaceMeshes() {
  if(surfaceMeshes.empty()) return;
  auto api=GetRemixApi();if(!api) return;
  for(auto it=surfaceMeshes.begin();it!=surfaceMeshes.end();) {
    if(frameId-it->second.frame>300) {
      api->DestroyMesh(it->second.handle);surfaceMeshBytes-=it->second.bytes;
      it=surfaceMeshes.erase(it);
    } else ++it;
  }
}

static remixapi_MaterialHandle SurfaceMaterial(IDirect3DDevice9* d,uint64_t textureHash) {
  DWORD u=0,v=0,mag=0;
  if(FAILED(d->GetSamplerState(0,D3DSAMP_ADDRESSU,&u)) || FAILED(d->GetSamplerState(0,D3DSAMP_ADDRESSV,&v)) ||
     FAILED(d->GetSamplerState(0,D3DSAMP_MAGFILTER,&mag)) || u<1 || u>3 || v<1 || v>3 || mag<1 || mag>3) return nullptr;
  const uint64_t descriptor[]={textureHash,u,v,mag};
  const auto hash=XXH3_64bits(descriptor,sizeof(descriptor));
  auto found=surfaceMaterials.find(hash);if(found!=surfaceMaterials.end()) return found->second;
  if(surfaceMaterials.size()>=256 || !surfaceAssetDirectory[0]) return nullptr;
  using SaveTexture=HRESULT(WINAPI*)(LPCWSTR,int,IDirect3DBaseTexture9*,const PALETTEENTRY*);
  static auto save=[](){auto dll=LoadLibraryExW(L"d3dx9_43.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    return dll?reinterpret_cast<SaveTexture>(GetProcAddress(dll,"D3DXSaveTextureToFileW")):nullptr;}();
  if(!save) return nullptr;
  wchar_t path[MAX_PATH]{};
  swprintf_s(path,L"%s\\%016llX.dds",surfaceAssetDirectory,static_cast<unsigned long long>(textureHash));
  if(GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES) {
    IDirect3DBaseTexture9* texture=nullptr;if(FAILED(d->GetTexture(0,&texture)) || !texture) return nullptr;
    const auto hr=save(path,4,texture,nullptr);texture->Release();if(FAILED(hr)) return nullptr;
  }
  remixapi_MaterialInfoOpaqueEXT opaque{};opaque.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_OPAQUE_EXT;
  opaque.albedoConstant={1,1,1};opaque.opacityConstant=1;opaque.roughnessConstant=0.5f;opaque.useDrawCallAlphaState=1;
  remixapi_MaterialInfo info{};info.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO;info.pNext=&opaque;
  info.hash=hash;info.albedoTexture=path;
  // API uses Vulkan sampler enums: repeat=0, mirrored repeat=1, clamp=2.
  info.wrapModeU=static_cast<uint8_t>(u-1);info.wrapModeV=static_cast<uint8_t>(v-1);
  info.filterMode=mag==D3DTEXF_POINT?0:1;
  remixapi_MaterialHandle material=nullptr;
  if(GetRemixApi()->CreateMaterial(&info,&material)!=REMIXAPI_ERROR_CODE_SUCCESS || !material) return nullptr;
  surfaceMaterials.emplace(hash,material);return material;
}

static bool SurfaceSubmitFailure(unsigned line) {
  static std::set<unsigned> reported;
  if(surfaceRoleLog && reported.insert(line).second) {
    fprintf(surfaceRoleLog,"{\"event\":\"submit_unsupported\",\"frame\":%u,\"line\":%u}\n",frameId,line);fflush(surfaceRoleLog);
  }
  return false;
}

static bool SubmitSurfaceOverlay(IDirect3DDevice9* d,D3DPRIMITIVETYPE type,INT base,UINT minVertex,UINT vertices,
                                 UINT start,UINT count,uint64_t textureHash) {
  auto api=GetRemixApi();
  if(!api || !api->CreateMesh || !api->DrawInstance || !api->CreateMaterial || count>32768 || vertices>65536) return SurfaceSubmitFailure(__LINE__);
  DWORD colorOp=0,color1=0,color2=0,alphaOp=0,alpha1=0,alpha2=0,texcoord=0,transform=0,nextOp=0,lighting=0,cull=0;
  if(FAILED(d->GetTextureStageState(0,D3DTSS_COLOROP,&colorOp)) || FAILED(d->GetTextureStageState(0,D3DTSS_COLORARG1,&color1)) ||
     FAILED(d->GetTextureStageState(0,D3DTSS_COLORARG2,&color2)) || FAILED(d->GetTextureStageState(0,D3DTSS_ALPHAOP,&alphaOp)) ||
     FAILED(d->GetTextureStageState(0,D3DTSS_ALPHAARG1,&alpha1)) || FAILED(d->GetTextureStageState(0,D3DTSS_ALPHAARG2,&alpha2)) ||
     FAILED(d->GetTextureStageState(0,D3DTSS_TEXCOORDINDEX,&texcoord)) || FAILED(d->GetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,&transform)) ||
     FAILED(d->GetTextureStageState(1,D3DTSS_COLOROP,&nextOp)) || FAILED(d->GetRenderState(D3DRS_LIGHTING,&lighting)) ||
     FAILED(d->GetRenderState(D3DRS_CULLMODE,&cull))) return SurfaceSubmitFailure(__LINE__);
  // Exact supported equation: texture * vertex RGBA, one UV set, no texgen.
  // On texture stage zero CURRENT starts with the diffuse vertex color.
  if(colorOp!=D3DTOP_MODULATE || color1!=D3DTA_TEXTURE || (color2!=D3DTA_DIFFUSE && color2!=D3DTA_CURRENT) ||
     alphaOp!=D3DTOP_MODULATE || alpha1!=D3DTA_TEXTURE || (alpha2!=D3DTA_DIFFUSE && alpha2!=D3DTA_CURRENT) ||
     texcoord>7 || transform!=D3DTTFF_DISABLE || nextOp!=D3DTOP_DISABLE || lighting) return SurfaceSubmitFailure(__LINE__);
  IDirect3DVertexBuffer9* vb=nullptr;IDirect3DIndexBuffer9* ib=nullptr;IDirect3DVertexDeclaration9* decl=nullptr;
  struct Release { IDirect3DVertexBuffer9*& v;IDirect3DIndexBuffer9*& i;IDirect3DVertexDeclaration9*& d;
    ~Release(){if(v)v->Release();if(i)i->Release();if(d)d->Release();} } release{vb,ib,decl};
  UINT offset=0,stride=0,n=MAXD3DDECLLENGTH+1;D3DVERTEXELEMENT9 layout[MAXD3DDECLLENGTH+1]{};
  if(FAILED(d->GetStreamSource(0,&vb,&offset,&stride)) || !vb || !stride || FAILED(d->GetIndices(&ib)) || !ib ||
     FAILED(d->GetVertexDeclaration(&decl)) || !decl || FAILED(decl->GetDeclaration(layout,&n))) return SurfaceSubmitFailure(__LINE__);
  int pos=-1,normal=-1,color=-1,uv=-1;
  for(UINT i=0;i<n && layout[i].Stream!=0xff;++i) {
    const auto& e=layout[i];if(e.Stream || e.Method!=D3DDECLMETHOD_DEFAULT) return SurfaceSubmitFailure(__LINE__);
    if(e.Usage==D3DDECLUSAGE_POSITION && e.UsageIndex==0 && e.Type==D3DDECLTYPE_FLOAT3) pos=e.Offset;
    if(e.Usage==D3DDECLUSAGE_NORMAL && e.UsageIndex==0 && e.Type==D3DDECLTYPE_FLOAT3) normal=e.Offset;
    if(e.Usage==D3DDECLUSAGE_COLOR && e.UsageIndex==0 && e.Type==D3DDECLTYPE_D3DCOLOR) color=e.Offset;
    if(e.Usage==D3DDECLUSAGE_TEXCOORD && e.UsageIndex==texcoord && e.Type==D3DDECLTYPE_FLOAT2) uv=e.Offset;
  }
  if(pos<0 || color<0 || uv<0 || UINT(pos+12)>stride || UINT(color+4)>stride || UINT(uv+8)>stride ||
     (normal>=0 && UINT(normal+12)>stride)) return SurfaceSubmitFailure(__LINE__);
  D3DVERTEXBUFFER_DESC vd{};D3DINDEXBUFFER_DESC id{};
  if(FAILED(vb->GetDesc(&vd)) || FAILED(ib->GetDesc(&id)) ||
     (id.Format!=D3DFMT_INDEX16 && id.Format!=D3DFMT_INDEX32)) return SurfaceSubmitFailure(__LINE__);
  const auto vbSnapshot=surfaceBuffers.find(vb),ibSnapshot=surfaceBuffers.find(ib);
  if(vbSnapshot==surfaceBuffers.end() || ibSnapshot==surfaceBuffers.end() || !vbSnapshot->second.complete ||
     !ibSnapshot->second.complete || vbSnapshot->second.bytes.size()!=vd.Size || ibSnapshot->second.bytes.size()!=id.Size) return SurfaceSubmitFailure(__LINE__);
  const UINT indexSize=id.Format==D3DFMT_INDEX16?2:4,indexCount=type==D3DPT_TRIANGLELIST?count*3:count+2;
  if(uint64_t(start+uint64_t(indexCount))*indexSize>id.Size) return SurfaceSubmitFailure(__LINE__);
  const void* indexData=ibSnapshot->second.bytes.data()+start*indexSize;
  std::vector<uint32_t> indices(indexCount);
  bool valid=true;
  for(UINT i=0;i<indexCount;++i) {
    const uint32_t original=indexSize==2?static_cast<const uint16_t*>(indexData)[i]:static_cast<const uint32_t*>(indexData)[i];
    const int64_t effective=int64_t(base)+original;
    if(original<minVertex || uint64_t(original)>=uint64_t(minVertex)+vertices || effective<0 ||
       uint64_t(offset)+(uint64_t(effective)+1)*stride>vd.Size) {valid=false;break;}
    indices[i]=static_cast<uint32_t>(effective);
  }
  if(!valid) return SurfaceSubmitFailure(__LINE__);
  const void* vertexData=vbSnapshot->second.bytes.data();
  std::vector<remixapi_HardcodedVertex> expanded;expanded.reserve(size_t(count)*3);
  for(UINT tri=0;tri<count;++tri) {
    UINT ids[3]={type==D3DPT_TRIANGLELIST?tri*3:tri,type==D3DPT_TRIANGLELIST?tri*3+1:tri+1,type==D3DPT_TRIANGLELIST?tri*3+2:tri+2};
    if(type==D3DPT_TRIANGLESTRIP && tri%2) std::swap(ids[0],ids[1]);
    if(cull==D3DCULL_CW) std::swap(ids[0],ids[1]);
    remixapi_HardcodedVertex triangle[3]{};
    for(UINT j=0;j<3;++j) {
      auto src=static_cast<const uint8_t*>(vertexData)+offset+size_t(indices[ids[j]])*stride;
      memcpy(triangle[j].position,src+pos,12);memcpy(triangle[j].texcoord,src+uv,8);memcpy(&triangle[j].color,src+color,4);
      if(normal>=0) memcpy(triangle[j].normal,src+normal,12);
      for(float f:triangle[j].position) if(!std::isfinite(f)) valid=false;
      for(float f:triangle[j].texcoord) if(!std::isfinite(f)) valid=false;
      for(float f:triangle[j].normal) if(!std::isfinite(f)) valid=false;
    }
    if(!valid) break;
    if(normal<0) {
      const auto a=triangle[0].position,b=triangle[1].position,c=triangle[2].position;
      const float x=(b[1]-a[1])*(c[2]-a[2])-(b[2]-a[2])*(c[1]-a[1]);
      const float y=(b[2]-a[2])*(c[0]-a[0])-(b[0]-a[0])*(c[2]-a[2]);
      const float z=(b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]);
      const float length=std::sqrt(x*x+y*y+z*z);if(!std::isfinite(length)){valid=false;break;}if(length<1e-12f) continue;
      for(auto& vtx:triangle) {vtx.normal[0]=x/length;vtx.normal[1]=y/length;vtx.normal[2]=z/length;}
    }
    expanded.insert(expanded.end(),triangle,triangle+3);
  }
  if(!valid || expanded.empty()) return SurfaceSubmitFailure(__LINE__);
  const auto material=SurfaceMaterial(d,textureHash);if(!material) return SurfaceSubmitFailure(__LINE__);
  const auto hash=XXH3_64bits_withSeed(expanded.data(),expanded.size()*sizeof(expanded[0]),reinterpret_cast<uintptr_t>(material));
  auto found=surfaceMeshes.find(hash);
  if(found==surfaceMeshes.end()) {
    const size_t bytes=expanded.size()*(sizeof(expanded[0])+4);
    if(surfaceMeshes.size()>=512 || surfaceMeshBytes+bytes>64*1024*1024) return SurfaceSubmitFailure(__LINE__);
    std::vector<uint32_t> sequential(expanded.size());for(size_t i=0;i<sequential.size();++i) sequential[i]=static_cast<uint32_t>(i);
    remixapi_MeshInfoSurfaceTriangles surface{};surface.vertices_values=expanded.data();surface.vertices_count=expanded.size();
    surface.indices_values=sequential.data();surface.indices_count=sequential.size();surface.material=material;
    remixapi_MeshInfo info{};info.sType=REMIXAPI_STRUCT_TYPE_MESH_INFO;info.hash=hash;info.surfaces_values=&surface;info.surfaces_count=1;
    remixapi_MeshHandle mesh=nullptr;if(api->CreateMesh(&info,&mesh)!=REMIXAPI_ERROR_CODE_SUCCESS || !mesh) return SurfaceSubmitFailure(__LINE__);
    found=surfaceMeshes.emplace(hash,SurfaceMesh{mesh,frameId,bytes}).first;surfaceMeshBytes+=bytes;
  }
  found->second.frame=frameId;
  D3DMATRIX world{};DWORD test=0,func=0,ref=0,mask=0;
  if(FAILED(d->GetTransform(D3DTS_WORLD,&world)) || FAILED(d->GetRenderState(D3DRS_ALPHATESTENABLE,&test)) ||
     FAILED(d->GetRenderState(D3DRS_ALPHAFUNC,&func)) || FAILED(d->GetRenderState(D3DRS_ALPHAREF,&ref)) ||
     FAILED(d->GetRenderState(D3DRS_COLORWRITEENABLE,&mask)) || func<1 || func>8) return SurfaceSubmitFailure(__LINE__);
  remixapi_InstanceInfoBlendEXT blend{};blend.sType=REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_BLEND_EXT;
  blend.alphaTestEnabled=test!=0;blend.alphaTestReferenceValue=static_cast<uint8_t>(ref);blend.alphaTestCompareOp=func-1;
  blend.alphaBlendEnabled=1;blend.srcColorBlendFactor=6;blend.dstColorBlendFactor=7; // Vulkan SRC_ALPHA, ONE_MINUS_SRC_ALPHA
  blend.srcAlphaBlendFactor=6;blend.dstAlphaBlendFactor=7;blend.writeMask=mask;
  blend.textureColorOperation=3;blend.textureAlphaOperation=3; // Modulate
  blend.textureColorArg1Source=blend.textureAlphaArg1Source=1; // Texture
  blend.textureColorArg2Source=blend.textureAlphaArg2Source=2; // VertexColor0
  remixapi_InstanceInfo instance{};instance.sType=REMIXAPI_STRUCT_TYPE_INSTANCE_INFO;instance.pNext=&blend;
  instance.categoryFlags=REMIXAPI_INSTANCE_CATEGORY_BIT_DECAL_STATIC;instance.mesh=found->second.handle;instance.doubleSided=cull==D3DCULL_NONE;
  for(unsigned r=0;r<3;++r) for(unsigned c=0;c<4;++c) {
    const float value=world.m[c][r];if(!std::isfinite(value)) return SurfaceSubmitFailure(__LINE__);instance.transform.matrix[r][c]=value;
  }
  return api->DrawInstance(&instance)==REMIXAPI_ERROR_CODE_SUCCESS;
}
