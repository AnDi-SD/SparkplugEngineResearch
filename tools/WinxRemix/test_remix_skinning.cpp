// Own GPU contract experiment, using only authored triangles and rigid bones.
// The existing graphics fixture supplies window capture and API error helpers.
// CPU reference below is fixture arithmetic, not recovered Sparkplug skin logic.
#define main material_fixture_unused_main
#include "test_remix_material.cpp"
#undef main
#include <array>
#include "winx_skin_packet_remix.h"
#include "winx_skin_packet_gpu.h"

static remixapi_Transform Bone(float angle,float x,float y) {
  remixapi_Transform t{};const float c=std::cos(angle),s=std::sin(angle);
  t.matrix[0][0]=c;t.matrix[0][1]=-s;t.matrix[0][3]=x;
  t.matrix[1][0]=s;t.matrix[1][1]=c;t.matrix[1][3]=y;t.matrix[2][2]=1;return t;
}
using SkinVertices=std::vector<remixapi_HardcodedVertex>;
static SkinVertices Triangle(unsigned subdivisions,std::vector<uint32_t>& indices,
    std::vector<std::array<float,3>>& barycentric) {
  SkinVertices result;
  const float p[3][3]={{-.65f,.55f,5},{.65f,.55f,5},{0,-.65f,5}};
  auto append=[&](const std::array<float,3>& weights){remixapi_HardcodedVertex vertex{};
    for(unsigned c=0;c<3;++c)for(unsigned i=0;i<3;++i)vertex.position[c]+=weights[i]*p[i][c];
    // Exact z keeps the two simultaneous instances separated by288 pixels.
    vertex.position[2]=5;vertex.normal[2]=-1;vertex.color=0xffffffff;
    result.push_back(vertex);barycentric.push_back(weights);};
  if(subdivisions==1){for(unsigned v=0;v<3;++v){std::array<float,3> weights{};weights[v]=1;append(weights);}indices={0,1,2};}
  else{
    auto index=[&](unsigned i,unsigned j){return i*(subdivisions+1)-i*(i-1)/2+j;};
    for(unsigned i=0;i<=subdivisions;++i)for(unsigned j=0;j<=subdivisions-i;++j){
      const float b=float(i)/subdivisions,c=float(j)/subdivisions;append({1-b-c,b,c});}
    for(unsigned i=0;i<subdivisions;++i)for(unsigned j=0;j<subdivisions-i;++j){
      indices.insert(indices.end(),{index(i,j),index(i+1,j),index(i,j+1)});
      if(i+j+1<subdivisions)indices.insert(indices.end(),{index(i+1,j),index(i+1,j+1),index(i,j+1)});}
  }
  return result;
}
static void Weights(unsigned count,const std::vector<std::array<float,3>>& barycentric,
    std::vector<float>& weights,std::vector<uint32_t>& bones) {
  const float values[3][4]={{.15f,.25f,.35f,.25f},{.65f,.20f,.10f,.05f},{.30f,.10f,.20f,.40f}};
  for(unsigned v=0;v<barycentric.size();++v){float sum=0;for(unsigned b=0;b<count;++b){
    float authored=0;for(unsigned corner=0;corner<3;++corner)authored+=barycentric[v][corner]*values[corner][b];
    const float w=b+1==count?1-sum:authored;sum+=w;weights.push_back(w);
    // The small fixture preserves its established varying-index control. The
    // large continuous mesh uses one palette-index tuple across its vertices.
    bones.push_back(barycentric.size()==3?(v+b)%4:b);}}
}
static void SignedWeights(unsigned count,const std::vector<std::array<float,3>>& barycentric,
    SkinVertices& input,std::vector<float>& weights,std::vector<uint32_t>& bones) {
  const float tuples[4][4]={{-1,0,0,0},{1.25f,-.25f,0,0},{-.5f,.25f,1,0},{-.5f,.25f,1.25f,-.125f}};
  for(unsigned v=0;v<input.size();++v){float sum=0;
    for(unsigned b=0;b<count;++b){const float weight=tuples[count-1][b]*(.75f+.5f*barycentric[v][0]);
      weights.push_back(weight);bones.push_back(b);sum+=weight;}
    // Keep deformed z=5 so simultaneous rigid translations still have the
    // established 288-pixel separation. This is authored fixture geometry.
    input[v].position[2]=5.f/sum;
  }
}
static winx_remix::skin_packet::Packet SourcePacket(const SkinVertices& input,const std::vector<uint32_t>& topology,unsigned count,
    const std::vector<float>& weights,const std::vector<uint32_t>& bones,const std::array<remixapi_Transform,4>& palette) {
  winx_remix::skin_packet::Packet packet;packet.influences=count;packet.attributes=3;packet.indices=topology;packet.palette.resize(palette.size());packet.vertices.resize(input.size());
  for(size_t b=0;b<palette.size();++b)for(unsigned row=0;row<3;++row)for(unsigned column=0;column<4;++column)packet.palette[b][row][column]=palette[b].matrix[row][column];
  for(size_t v=0;v<input.size();++v){auto& out=packet.vertices[v];out.sourceIndex=uint32_t(v);out.color=input[v].color;
    memcpy(out.skin.position.data(),input[v].position,12);memcpy(out.skin.normal.data(),input[v].normal,12);memcpy(out.uv.data(),input[v].texcoord,8);
    out.skin.position[3]=out.skin.normal[3]=1;
    for(unsigned b=0;b<count;++b){out.skin.weights[b]=weights[v*count+b];out.skin.indices[b]=bones[v*count+b];}}
  return packet;
}
static remixapi_MeshHandle Mesh(const SkinVertices& verts,const std::vector<uint32_t>& indices,
    remixapi_MaterialHandle material,uint64_t hash,unsigned count,
    const std::vector<float>& weights,const std::vector<uint32_t>& bones) {
  remixapi_MeshInfoSurfaceTriangles surface{};
  surface.vertices_values=verts.data();surface.vertices_count=verts.size();surface.indices_values=indices.data();surface.indices_count=indices.size();surface.material=material;
  if(count){surface.skinning_hasvalue=1;surface.skinning_value.bonesPerVertex=count;
    surface.skinning_value.blendWeights_values=weights.data();surface.skinning_value.blendWeights_count=uint32_t(weights.size());
    surface.skinning_value.blendIndices_values=bones.data();surface.skinning_value.blendIndices_count=uint32_t(bones.size());}
  remixapi_MeshInfo mesh{};mesh.sType=REMIXAPI_STRUCT_TYPE_MESH_INFO;mesh.hash=hash;mesh.surfaces_values=&surface;mesh.surfaces_count=1;
  remixapi_MeshHandle handle=nullptr;Api(api.CreateMesh(&mesh,&handle),"create fixture mesh");if(!handle)Fail("null mesh handle",0);return handle;
}
static SkinVertices Baked(const SkinVertices& input,unsigned count,
    const std::vector<float>& weights,const std::vector<uint32_t>& bones,const std::array<remixapi_Transform,4>& palette) {
  auto result=input;
  for(unsigned v=0;v<input.size();++v)for(unsigned row=0;row<3;++row){float value=0;
    for(unsigned b=0;b<count;++b){const auto& m=palette[bones[v*count+b]].matrix[row];
      value+=weights[v*count+b]*(m[0]*input[v].position[0]+m[1]*input[v].position[1]+m[2]*input[v].position[2]+m[3]);}
    result[v].position[row]=value;}
  return result;
}
static void Pump(){MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){
  if(message.message==WM_QUIT)Fail("fixture interrupted",0);TranslateMessage(&message);DispatchMessageW(&message);}}
static void Begin(){Pump();Hr(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff080808,1,0),"clear");
  Hr(device->BeginScene(),"begin");RenderPanel(0,0xffffffff,false,false);}
static void End(){Hr(device->EndScene(),"end");Hr(device->Present(nullptr,nullptr,nullptr,nullptr),"present");++frame;}

#include "test_skin_motion_history.h"
#include "test_skin_motion_rotation.h"
#include "test_skin_motion_islands.h"
#include "test_skin_motion_camera.h"

int main(int argc,char** argv) {
  if(!OpenJournal())return 1;
  unsigned maximum=2,subdivisions=1,halfResolution=0,signedWeights=0,motionHistory=0,motionKind=0;
  for(int i=1;i<argc;i+=2){if(i+1>=argc)Fail("arguments",0);
    if(strcmp(argv[i],"--max-bones")==0)maximum=unsigned(std::atoi(argv[i+1]));
    else if(strcmp(argv[i],"--subdivisions")==0)subdivisions=unsigned(std::atoi(argv[i+1]));
    else if(strcmp(argv[i],"--half-resolution")==0)halfResolution=unsigned(std::atoi(argv[i+1]));
    else if(strcmp(argv[i],"--motion-history")==0)motionHistory=unsigned(std::atoi(argv[i+1]));
    else if(strcmp(argv[i],"--motion-kind")==0)motionKind=unsigned(std::atoi(argv[i+1]));
    else if(strcmp(argv[i],"--signed-weights")==0)signedWeights=unsigned(std::atoi(argv[i+1]));else Fail("arguments",0);}
  if(maximum<1||maximum>4)Fail("bones range",maximum);
  if(subdivisions!=1&&subdivisions!=24)Fail("subdivision range",subdivisions);
  if(halfResolution>1)Fail("resolution profile",halfResolution);
  if(signedWeights>1)Fail("weight profile",signedWeights);
  if(motionKind>3||(motionKind&&!motionHistory))Fail("motion kind",motionKind);
  if(motionHistory>1||(motionHistory&&(subdivisions!=24||maximum!=2||halfResolution||signedWeights)))Fail("motion profile",motionHistory);
  SetProcessDPIAware();WNDCLASSW wc{};wc.lpfnWndProc=WindowProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"WinxRemixSkinningFixture";
  if(!RegisterClassW(&wc))Fail("register window",GetLastError());RECT size{0,0,960,540};AdjustWindowRect(&size,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,FALSE);
  window=CreateWindowW(wc.lpszClassName,L"Remix skinning contract fixture",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,40,40,size.right-size.left,size.bottom-size.top,nullptr,nullptr,wc.hInstance,nullptr);
  if(!window)Fail("window",GetLastError());
  // The hidden console launcher supplies STARTUPINFO.wShowWindow. A second
  // ShowWindow explicitly exposes only our owned graphics/capture window.
  ShowWindow(window,SW_SHOW);ShowWindow(window,SW_SHOW);Focus();
  auto runtime=LoadLibraryW(L".\\d3d9.dll");if(!runtime)Fail("runtime",GetLastError());
  auto factory=reinterpret_cast<IDirect3D9*(WINAPI*)(UINT)>(GetProcAddress(runtime,"Direct3DCreate9"));if(!factory)Fail("factory",0);
  auto d3d=factory(D3D_SDK_VERSION);if(!d3d)Fail("d3d",0);
  D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;pp.BackBufferWidth=960;pp.BackBufferHeight=540;
  pp.BackBufferFormat=D3DFMT_X8R8G8B8;pp.EnableAutoDepthStencil=TRUE;pp.AutoDepthStencilFormat=D3DFMT_D24S8;pp.PresentationInterval=D3DPRESENT_INTERVAL_ONE;
  Hr(d3d->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,&device),"device");
  auto initialize=reinterpret_cast<PFN_remixapi_InitializeLibrary>(GetProcAddress(runtime,"remixapi_InitializeLibrary"));if(!initialize)Fail("Remix API",0);
  remixapi_InitializeLibraryInfo init{};init.sType=REMIXAPI_STRUCT_TYPE_INITIALIZE_LIBRARY_INFO;init.version=REMIXAPI_VERSION_MAKE(REMIXAPI_VERSION_MAJOR,REMIXAPI_VERSION_MINOR,REMIXAPI_VERSION_PATCH);
  Api(initialize(&init,&api),"initialize");if(!api.CreateMesh||!api.DrawInstance||!api.SetupCamera)Fail("API functions",0);
  D3DMATRIX identity{};identity._11=identity._22=identity._33=identity._44=1;
  D3DMATRIX projection{};projection._11=1;projection._22=960.0f/540;projection._33=100.0f/99.9f;projection._34=1;projection._43=-.1f*100/99.9f;
  Hr(device->SetTransform(D3DTS_WORLD,&identity),"world");Hr(device->SetTransform(D3DTS_VIEW,&identity),"view");Hr(device->SetTransform(D3DTS_PROJECTION,&projection),"projection");
  Hr(device->SetFVF(fvf),"fvf");for(auto state:{D3DRS_LIGHTING,D3DRS_ALPHABLENDENABLE,D3DRS_ALPHATESTENABLE,D3DRS_FOGENABLE,D3DRS_SRGBWRITEENABLE})Hr(device->SetRenderState(state,0),"disable state");
  Hr(device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE),"cull");Hr(device->SetRenderState(D3DRS_ZENABLE,TRUE),"depth");
  Hr(device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1),"rgb op");Hr(device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE),"rgb");
  Hr(device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1),"alpha op");Hr(device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE),"alpha");Hr(device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE),"stage1");
  vertices[0][0]={-3.3f,.5f,5,0,0,-1,0xffffffff,0,0};vertices[0][1]={-2.7f,.5f,5,0,0,-1,0xffffffff,1,0};
  vertices[0][2]={-3.3f,-.5f,5,0,0,-1,0xffffffff,0,1};vertices[0][3]={-2.7f,-.5f,5,0,0,-1,0xffffffff,1,1};
  for(unsigned i=0;i<60;++i){Begin();End();}
  Config("rtx.debugView.debugViewIdx","23");Config("rtx.vertexColorIsBakedLighting","False");
  Config("rtx.upscalerType",halfResolution?"2":"0");Config("rtx.resolutionScale",halfResolution?"0.5":"1");
  Config("rtx.forceCameraJitter","False");Config("rtx.enableRayReconstruction","False");
  remixapi_MaterialInfoOpaqueEXT opaque{};opaque.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_OPAQUE_EXT;opaque.albedoConstant={1,1,1};opaque.opacityConstant=1;opaque.roughnessConstant=1;opaque.useDrawCallAlphaState=1;
  remixapi_MaterialInfo material{};material.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO;material.pNext=&opaque;material.hash=0x5758534b494e0001ull;
  remixapi_MaterialHandle materialHandle=nullptr;Api(api.CreateMaterial(&material,&materialHandle),"material");if(!materialHandle)Fail("null material",0);
  remixapi_CameraInfo camera{};camera.sType=REMIXAPI_STRUCT_TYPE_CAMERA_INFO;camera.type=REMIXAPI_CAMERA_TYPE_WORLD;memcpy(camera.view,&identity,64);memcpy(camera.projection,&projection,64);
  std::vector<uint32_t> triangleIndices;std::vector<std::array<float,3>> barycentric;
  const auto baseInput=Triangle(subdivisions,triangleIndices,barycentric);
  if(motionHistory) {
    const auto captures=motionKind==1?MotionRotation(baseInput,triangleIndices,materialHandle,camera):
      motionKind==2?MotionIslands(baseInput,triangleIndices,materialHandle,camera):
      motionKind==3?MotionCamera(baseInput,triangleIndices,materialHandle,camera):
      MotionHistory(baseInput,triangleIndices,materialHandle,camera);
    Api(api.DestroyMaterial(materialHandle),"retire motion material");for(unsigned i=0;i<3;++i){Begin();End();}
    IDirect3DQuery9* completion=nullptr;Hr(device->CreateQuery(D3DQUERYTYPE_EVENT,&completion),"motion completion query");
    Hr(completion->Issue(D3DISSUE_END),"motion completion issue");const auto completionStart=GetTickCount64();unsigned polls=0;
    for(;;){BOOL done=FALSE;const auto result=completion->GetData(&done,sizeof(done),D3DGETDATA_FLUSH);++polls;
      Hr(result,"motion completion read");if(result==S_OK&&done)break;
      if(GetTickCount64()-completionStart>10000)Fail("motion completion timeout",polls);Pump();Sleep(1);}
    completion->Release();fprintf(journal,"{\"event\":\"deviceCompletion\",\"milliseconds\":%llu,\"polls\":%u}\n",GetTickCount64()-completionStart,polls);fflush(journal);
    device->Release();d3d->Release();DestroyWindow(window);
    fprintf(journal,"{\"event\":\"complete\",\"frames\":%u,\"captures\":%u,\"motionHistoryExperiment\":true}\n",frame,captures);fclose(journal);return 0;
  }
  fprintf(journal,"{\"event\":\"contract\",\"maximumBonesPerVertex\":%u,\"vertices\":%zu,\"subdivisions\":%u,\"paletteCount\":4,\"signedWeights\":%s,\"paired\":true,\"referenceWorldTranslation\":[-1.5,-0.2,0],\"worldTranslation\":[1.5,-0.2,0],\"pixelSeparation\":288,\"view\":\"identity\",\"projection\":[1,1.77777778,1.001001,1,-0.1001001],\"debugView\":23}\n",maximum,baseInput.size(),subdivisions,signedWeights?"true":"false");fflush(journal);
  fprintf(journal,"{\"event\":\"mesh_topology\",\"indices\":[");for(size_t i=0;i<triangleIndices.size();++i)fprintf(journal,"%s%u",i?",":"",triangleIndices[i]);fprintf(journal,"]}\n");fflush(journal);
  const auto start=GetTickCount64();unsigned captures=0;
  for(unsigned count=1;count<=maximum;++count){auto input=baseInput;std::vector<float> weights;std::vector<uint32_t> bones;
    if(signedWeights)SignedWeights(count,barycentric,input,weights,bones);else Weights(count,barycentric,weights,bones);
    winx_remix::skin_packet::SignedSkin encoded;
    if(signedWeights){const std::array<remixapi_Transform,4> identityPalette={Bone(0,0,0),Bone(0,0,0),Bone(0,0,0),Bone(0,0,0)};
      if(!winx_remix::skin_packet::EncodeSignedSkin(SourcePacket(input,triangleIndices,count,weights,bones,identityPalette),encoded))Fail("signed encoding",count);}
    auto skin=Mesh(input,triangleIndices,materialHandle,0x5758534b4d000000ull+count,signedWeights?encoded.influences:count,signedWeights?encoded.weights:weights,signedWeights?encoded.indices:bones);
    fprintf(journal,"{\"event\":\"mesh_input\",\"bonesPerVertex\":%u,\"weights\":[",count);
    for(size_t i=0;i<weights.size();++i)fprintf(journal,"%s%.9g",i?",":"",weights[i]);fprintf(journal,"],\"indices\":[");for(size_t i=0;i<bones.size();++i)fprintf(journal,"%s%u",i?",":"",bones[i]);fprintf(journal,"]}\n");fflush(journal);
    for(unsigned pose=0;pose<2;++pose){const float sign=pose?-1.0f:1.0f;
      const std::array<remixapi_Transform,4> palette={Bone(.3f*sign,.4f*sign,.1f),Bone(-.25f*sign,-.45f*sign,.25f),Bone(.1f*sign,.15f,-.35f*sign),Bone(-.4f*sign,-.2f,.1f*sign)};
      auto baked=Baked(input,count,weights,bones,palette);std::vector<remixapi_Transform> submittedPalette(palette.begin(),palette.end());
      if(signedWeights){const auto source=SourcePacket(input,triangleIndices,count,weights,bones,palette);winx_remix::skin_packet::BakedMesh original;winx_remix::skin_packet::SignedSkin posed;
        if(!winx_remix::skin_packet::Bake(source,original)||!winx_remix::skin_packet::EncodeSignedSkin(source,posed))Fail("shared original reference",count);
        if(posed.weights!=encoded.weights||posed.indices!=encoded.indices)Fail("pose changed immutable weights",count);
        for(size_t v=0;v<baked.size();++v){memcpy(baked[v].position,original.vertices[v].position.data(),12);memcpy(baked[v].normal,original.vertices[v].normal.data(),12);}
        submittedPalette.resize(posed.palette.size());for(size_t b=0;b<posed.palette.size();++b)for(unsigned row=0;row<3;++row)for(unsigned column=0;column<4;++column)submittedPalette[b].matrix[row][column]=posed.palette[b][row][column];
      }
      auto reference=Mesh(baked,triangleIndices,materialHandle,0x5758534b52000000ull+count*16+pose,0,weights,bones);
      fprintf(journal,"{\"event\":\"pose_input\",\"bonesPerVertex\":%u,\"pose\":%u,\"expectedPositions\":[",count,pose);
      for(unsigned v=0;v<baked.size();++v)fprintf(journal,"%s[%.9g,%.9g,%.9g]",v?",":"",baked[v].position[0],baked[v].position[1],baked[v].position[2]);fprintf(journal,"]}\n");fflush(journal);
      for(unsigned mode=0;mode<3;++mode){Focus();const auto begin=GetTickCount64();unsigned frames=0;
        do {if(GetTickCount64()-start>120000)Fail("120 second bound",0);Begin();Api(api.SetupCamera(&camera),"camera");
          remixapi_InstanceInfoBlendEXT blend{};blend.sType=REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_BLEND_EXT;blend.writeMask=15;blend.alphaTestCompareOp=7;
          blend.textureColorOperation=1;blend.textureColorArg1Source=2;blend.textureColorArg2Source=1;blend.textureAlphaOperation=1;blend.textureAlphaArg1Source=2;blend.tFactor=0xffffffff;
          remixapi_InstanceInfoBoneTransformsEXT ext{};ext.sType=REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_BONE_TRANSFORMS_EXT;ext.pNext=&blend;ext.boneTransforms_values=submittedPalette.data();ext.boneTransforms_count=uint32_t(submittedPalette.size());
          remixapi_InstanceInfo instance{};instance.sType=REMIXAPI_STRUCT_TYPE_INSTANCE_INFO;instance.pNext=mode==1?static_cast<void*>(&ext):static_cast<void*>(&blend);instance.mesh=mode==1?skin:reference;instance.doubleSided=1;
          instance.categoryFlags=REMIXAPI_INSTANCE_CATEGORY_BIT_IGNORE_ANTI_CULLING|REMIXAPI_INSTANCE_CATEGORY_BIT_IGNORE_MOTION_BLUR;
          instance.transform=Bone(0,1.5f,-.2f);Api(api.DrawInstance(&instance),"instance");
          // Simultaneous reference shares the exact camera jitter/sample. The
          // rigid world translations differ by288 pixels at every vertex(z5).
          instance.pNext=&blend;instance.mesh=reference;instance.transform=Bone(0,-1.5f,-.2f);
          Api(api.DrawInstance(&instance),"simultaneous reference");End();++frames;
        }while(frames<90||GetTickCount64()-begin<1500);
        char filename[80]{};sprintf_s(filename,"b%u-p%u-%s.bmp",count,pose,mode==1?"skin":mode==0?"reference":"reference-return");Capture(filename);++captures;
      }
      Api(api.DestroyMesh(reference),"retire reference");
    }
    Api(api.DestroyMesh(skin),"retire skin");
  }
  Api(api.DestroyMaterial(materialHandle),"retire material");for(unsigned i=0;i<3;++i){Begin();End();}
  device->Release();d3d->Release();DestroyWindow(window);
  fprintf(journal,"{\"event\":\"complete\",\"frames\":%u,\"captures\":%u,\"maximumBonesPerVertex\":%u}\n",frame,captures,maximum);fclose(journal);return 0;
}
