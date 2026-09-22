// Own recording-API integration test, including owned COM doubles. No original
// game code, real D3D device, GPU or live renderer.
#define WINX_REMIX_TEST
#include "winx_d3d9_probe.cpp"
#include <fstream>
#include <stdexcept>
#ifdef WINX_SKIN_PACKET_WIRE_TEST
#include "util_remixapi.h"
#endif
namespace submit_test {
namespace packet=winx_remix::skin_packet;
namespace submit=winx_remix::skin_packet_submit;
static unsigned checks,creates,draws,destroys,materialCreates,materialDestroys;
static uintptr_t next=1;
static std::map<remixapi_MeshHandle,remixapi_MaterialHandle> meshes;
static std::set<remixapi_MaterialHandle> materials;
static std::vector<remixapi_HardcodedVertex> observedVertices;
static std::vector<uint32_t> observedIndices;
static std::vector<float> observedWeights;
static std::vector<uint32_t> observedBones;
static std::vector<remixapi_Transform> observedTransforms;
static unsigned observedInfluences;
static bool gpuMode,retireCreate;
static bool failCreate,failDraw,throwDraw,retireDraw,reenterDraw;
static submit::Prepared* active;
static winx_remix::skin_gpu_submit::Prepared* activeGpu;
static remixapi_MeshHandle activeMesh;
static remixapi_MaterialHandle activeMaterial;
static SurfaceInstanceState drawState{D3DCULL_NONE,1,D3DCMP_GREATEREQUAL,0,15,1,D3DBLEND_ONE,D3DBLEND_ZERO,D3DBLENDOP_ADD,true};
static surface_material::Contract contract{{3,1,2},{3,1,2}};
static void Check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
#ifdef WINX_SKIN_PACKET_WIRE_TEST
namespace wire=remixapi::util;
static std::fstream wireStream;
static bool wireReading;
static size_t wireRecords,wireBytes;
template<class Serializer,class Input> static std::vector<uint8_t> Wire(uint32_t kind,const Input& input){
 Serializer source(input);const auto size=source.size();Check(size>0&&size<=8*1024*1024,"bounded real serializer size");
 std::vector<uint8_t> expected(size);source.serialize(expected.data());
 uint32_t header[2]={kind,size};
 if(wireReading){wireStream.read(reinterpret_cast<char*>(header),8);Check(bool(wireStream)&&header[0]==kind&&header[1]==size,"cross-architecture record identity/size");
  std::vector<uint8_t> observed(size);wireStream.read(reinterpret_cast<char*>(observed.data()),size);Check(bool(wireStream)&&observed==expected,"cross-architecture wire is bit-identical");expected=std::move(observed);
 }else{wireStream.write(reinterpret_cast<const char*>(header),8);wireStream.write(reinterpret_cast<const char*>(expected.data()),size);Check(bool(wireStream),"complete wire record write");}
 ++wireRecords;wireBytes+=size+8;return expected;
}
static void MeshWire(const remixapi_MeshInfo& input){
 uint32_t size=0;Check(wire::isSupportedMeshInfo(&input,&size),"real bridge preflight accepts compact mesh");
 auto bytes=Wire<wire::serialize::MeshInfo>(1,input);Check(size==bytes.size()&&wire::validateMeshInfoPayload(bytes.data(),size),"real server mesh payload validation");
 wire::serialize::MeshInfo output(bytes.data());output.deserialize();
 Check(output.sType==input.sType&&output.hash==input.hash&&!output.pNext&&output.surfaces_count==1,"owning mesh decoder header");
 const auto& a=output.surfaces_values[0];const auto& b=input.surfaces_values[0];
 Check(a.vertices_count==b.vertices_count&&a.indices_count==b.indices_count&&a.material==b.material&&a.skinning_hasvalue==b.skinning_hasvalue,"decoded topology/material and explicit skinning presence");
 if(b.skinning_hasvalue){const auto& x=a.skinning_value;const auto& y=b.skinning_value;
  Check(x.bonesPerVertex==y.bonesPerVertex&&x.blendWeights_count==y.blendWeights_count&&x.blendIndices_count==y.blendIndices_count,"decoded signed influence layout");
  Check(!memcmp(x.blendWeights_values,y.blendWeights_values,size_t(y.blendWeights_count)*4)&&
        !memcmp(x.blendIndices_values,y.blendIndices_values,size_t(y.blendIndices_count)*4),"all signed weights and indices survive owning decoder");
 }
 for(size_t i=0;i<a.vertices_count;++i){const auto& x=a.vertices_values[i];const auto& y=b.vertices_values[i];
  Check(!memcmp(x.position,y.position,12)&&!memcmp(x.normal,y.normal,12)&&!memcmp(x.texcoord,y.texcoord,8)&&x.color==y.color,"all meaningful vertex bits survive real owning decoder");}
 Check(!memcmp(a.indices_values,b.indices_values,size_t(a.indices_count)*4),"all compact index bits survive real owning decoder");
}
static void InstanceWire(const remixapi_InstanceInfo& input,const remixapi_InstanceInfoBlendEXT& blend){
 auto bytes=Wire<wire::serialize::InstanceInfo>(2,input);wire::serialize::InstanceInfo decoded(bytes.data());decoded.deserialize();
 Check(decoded.sType==input.sType&&decoded.mesh==input.mesh&&decoded.categoryFlags==input.categoryFlags&&decoded.doubleSided==input.doubleSided&&
  !memcmp(decoded.transform.matrix,input.transform.matrix,48),"instance identity transform/category/culling survive wire");
 auto blendBytes=Wire<wire::serialize::InstanceInfoBlend>(3,blend);wire::serialize::InstanceInfoBlend decodedBlend(blendBytes.data());decodedBlend.deserialize();
 Check(decodedBlend.sType==blend.sType&&decodedBlend.alphaTestEnabled==blend.alphaTestEnabled&&decodedBlend.alphaTestReferenceValue==blend.alphaTestReferenceValue&&
  decodedBlend.alphaTestCompareOp==blend.alphaTestCompareOp&&decodedBlend.alphaBlendEnabled==blend.alphaBlendEnabled&&decodedBlend.srcColorBlendFactor==blend.srcColorBlendFactor&&
  decodedBlend.dstColorBlendFactor==blend.dstColorBlendFactor&&decodedBlend.colorBlendOp==blend.colorBlendOp&&decodedBlend.textureColorOperation==blend.textureColorOperation&&
  decodedBlend.textureColorArg1Source==blend.textureColorArg1Source&&decodedBlend.textureColorArg2Source==blend.textureColorArg2Source&&decodedBlend.textureAlphaOperation==blend.textureAlphaOperation&&
  decodedBlend.textureAlphaArg1Source==blend.textureAlphaArg1Source&&decodedBlend.textureAlphaArg2Source==blend.textureAlphaArg2Source&&decodedBlend.tFactor==blend.tFactor&&
  decodedBlend.isTextureFactorBlend==blend.isTextureFactorBlend&&decodedBlend.srcAlphaBlendFactor==blend.srcAlphaBlendFactor&&decodedBlend.dstAlphaBlendFactor==blend.dstAlphaBlendFactor&&
  decodedBlend.alphaBlendOp==blend.alphaBlendOp&&decodedBlend.writeMask==blend.writeMask&&decodedBlend.isVertexColorBakedLighting==blend.isVertexColorBakedLighting,"every blend field survives wire");
}
static void BonesWire(const remixapi_InstanceInfoBoneTransformsEXT& input) {
 Check(wire::isSupportedBoneTransforms(&input),"real bridge accepts signed bone palette");
 auto bytes=Wire<wire::serialize::InstanceInfoTransforms>(4,input);
 Check(wire::validateBoneTransformsPayload(bytes.data(),uint32_t(bytes.size())),"real server validates signed palette extent");
 wire::serialize::InstanceInfoTransforms output(bytes.data());output.deserialize();
 Check(output.sType==input.sType&&output.boneTransforms_count==input.boneTransforms_count&&
       !memcmp(output.boneTransforms_values,input.boneTransforms_values,size_t(input.boneTransforms_count)*48),"all pose matrix bits survive owning decoder");
 Check(!wire::validateBoneTransformsPayload(bytes.data(),uint32_t(bytes.size()-1)),"truncated palette payload is rejected");
}
#endif
struct Watchdog {
 HANDLE event=nullptr,thread=nullptr;
 static DWORD WINAPI Wait(void* event){if(WaitForSingleObject(event,30000)==WAIT_TIMEOUT)TerminateProcess(GetCurrentProcess(),0xe0521a30u);return 0;}
 Watchdog(){event=CreateEventW(nullptr,TRUE,FALSE,nullptr);Check(event!=nullptr,"watchdog event");thread=CreateThread(nullptr,0,Wait,event,0,nullptr);Check(thread!=nullptr,"watchdog thread");}
 ~Watchdog(){SetEvent(event);WaitForSingleObject(thread,1000);CloseHandle(thread);CloseHandle(event);}
};
static remixapi_ErrorCode REMIXAPI_CALL CreateMaterial(const remixapi_MaterialInfo*,remixapi_MaterialHandle* out){
 *out=reinterpret_cast<remixapi_MaterialHandle>(next++);Check(materials.insert(*out).second,"unique material handle");++materialCreates;return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL DestroyMaterial(remixapi_MaterialHandle material){
 for(const auto& mesh:meshes)Check(mesh.second!=material,"material outlives every mesh");
 Check(materials.erase(material)==1,"material destroyed exactly once");++materialDestroys;return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL CreateMesh(const remixapi_MeshInfo* info,remixapi_MeshHandle* out){
 Check(info&&info->sType==REMIXAPI_STRUCT_TYPE_MESH_INFO&&!info->pNext&&info->surfaces_count==1,"one explicit API surface");
 const auto& surface=info->surfaces_values[0];
 Check(surface.vertices_count&&surface.vertices_count<=65536&&surface.indices_count&&surface.indices_count<=32768*3&&surface.indices_count%3==0,"bounded compact topology");
 if(gpuMode){const auto& skin=surface.skinning_value;
  Check(surface.skinning_hasvalue&&skin.bonesPerVertex>=2&&skin.bonesPerVertex<=5&&skin.blendWeights_count==surface.vertices_count*skin.bonesPerVertex&&skin.blendIndices_count==skin.blendWeights_count,"signed API mesh has complete B2-B5 influence arrays");
  observedInfluences=skin.bonesPerVertex;observedWeights.assign(skin.blendWeights_values,skin.blendWeights_values+skin.blendWeights_count);
  observedBones.assign(skin.blendIndices_values,skin.blendIndices_values+skin.blendIndices_count);
 }else Check(!surface.skinning_hasvalue&&!surface.skinning_value.bonesPerVertex&&!surface.skinning_value.blendWeights_values&&!surface.skinning_value.blendIndices_values,"baked API mesh disables every skin input");
 Check(materials.count(surface.material)==1,"known live material");
#ifdef WINX_SKIN_PACKET_WIRE_TEST
 if(wireStream.is_open())MeshWire(*info);
#endif
 observedVertices.assign(surface.vertices_values,surface.vertices_values+surface.vertices_count);
 observedIndices.assign(surface.indices_values,surface.indices_values+surface.indices_count);
 for(const auto& v:observedVertices){Check(!v._pad0&&!v._pad1&&!v._pad2&&!v._pad3&&!v._pad4&&!v._pad5&&!v._pad6,"explicit API padding is zero");
  const float length=v.normal[0]*v.normal[0]+v.normal[1]*v.normal[1]+v.normal[2]*v.normal[2];Check(std::isfinite(length)&&(gpuMode||std::fabs(length-1)<1e-5),"finite authored normal or unit baked world normal");}
 for(auto i:observedIndices)Check(i<observedVertices.size(),"index addresses compact vertex");
 *out=reinterpret_cast<remixapi_MeshHandle>(next++);Check(meshes.emplace(*out,surface.material).second,"unique mesh owner");++creates;
 if(retireCreate)RetireSurfaceResources(true);
 return failCreate?REMIXAPI_ERROR_CODE_GENERAL_FAILURE:REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL DestroyMesh(remixapi_MeshHandle mesh){Check(meshes.erase(mesh)==1,"mesh destroyed exactly once");++destroys;return REMIXAPI_ERROR_CODE_SUCCESS;}
static remixapi_ErrorCode REMIXAPI_CALL DrawInstance(const remixapi_InstanceInfo* info){
 ++draws;Check(info&&meshes.count(info->mesh)==1&&info->sType==REMIXAPI_STRUCT_TYPE_INSTANCE_INFO,"draw uses live mesh");
 Check(info->categoryFlags==0&&info->doubleSided==(drawState.cull==D3DCULL_NONE),"explicit culling with no guessed category");
 for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)Check(info->transform.matrix[r][c]==float(r==c),"identity instance avoids second world transform");
 const void* extensions=info->pNext;
 if(gpuMode){const auto* bones=static_cast<const remixapi_InstanceInfoBoneTransformsEXT*>(extensions);
  Check(bones&&bones->sType==REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_BONE_TRANSFORMS_EXT&&bones->boneTransforms_count&&bones->boneTransforms_values,"signed draw supplies explicit palette extension");
  observedTransforms.assign(bones->boneTransforms_values,bones->boneTransforms_values+bones->boneTransforms_count);extensions=bones->pNext;
 }
 const auto* blend=static_cast<const remixapi_InstanceInfoBlendEXT*>(extensions);
 Check(blend&&blend->sType==REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_BLEND_EXT&&!blend->pNext,"blend terminates the explicit extension chain");
 Check(blend->alphaTestCompareOp==(drawState.test?drawState.function-1:7)&&
  blend->alphaTestReferenceValue==(drawState.test?drawState.reference:0)&&
  blend->srcColorBlendFactor==1&&blend->dstColorBlendFactor==0&&blend->textureColorOperation==3,"explicit opaque/alpha-test equation maps through common instance adapter");
#ifdef WINX_SKIN_PACKET_WIRE_TEST
 if(wireStream.is_open()){InstanceWire(*info,*blend);if(gpuMode)BonesWire(*static_cast<const remixapi_InstanceInfoBoneTransformsEXT*>(info->pNext));}
#endif
 if(reenterDraw){const auto before=draws;const auto nested=gpuMode?
   winx_remix::skin_gpu_submit::Draw(activeMesh,activeMaterial,*activeGpu,drawState,contract,[]{return true;}):
   submit::Draw(activeMesh,activeMaterial,*active,drawState,contract);Check(!nested.apiCalled&&draws==before,"reentrant draw never calls API twice");}
 if(retireDraw)RetireSurfaceResources(true);
 if(throwDraw)throw std::bad_alloc();
 return failDraw?REMIXAPI_ERROR_CODE_GENERAL_FAILURE:REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_MaterialHandle Material(uint64_t key){SurfaceResourceOperation operation;remixapi_MaterialInfo info{};info.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO;info.hash=key;return CreateSurfaceMaterialOwned(info,operation);}
static void Clean(){RetireSurfaceResources(true);Check(meshes.empty()&&materials.empty()&&surfaceMeshes.empty()&&surfaceMaterials.empty()&&!surfaceMeshBytes,"all resources and byte reservations retired");}
static packet::Packet Quad(unsigned influences){
 packet::Packet p;p.influences=influences;p.attributes=3;p.palette.resize(4);p.vertices.resize(4);p.indices={0,1,2,0,2,3};
 for(unsigned b=0;b<4;++b){auto& m=p.palette[b];m[0]={1,0,0,float(b+1)};m[1]={0,1,0,float(b)};m[2]={0,0,1,-float(b)};}
 const float weights[4]={.5f,-.25f,.125f,.75f};
 for(unsigned i=0;i<4;++i){auto& v=p.vertices[i];v.skin.position={float(i&1),float(i>>1),0,1};v.skin.normal={0,0,1,1};v.sourceIndex=i;v.uv={float(i&1),float(i>>1)};v.color=0xff000000;
  for(unsigned b=0;b<influences;++b){v.skin.weights[b]=weights[b];v.skin.indices[b]=b;}}
 return p;
}
static submit::Prepared Prepare(const packet::Packet& p,DWORD cull=D3DCULL_NONE){submit::Prepared out;
 Check(submit::Prepare(p,std::vector<uint32_t>(p.vertices.size(),0xffabcdef),cull,out),"generic B1-B4 preparation with explicit material color");return out;}
static void Payload(const submit::Prepared& prepared){
 Check(observedVertices.size()==prepared.vertices.size()&&observedIndices==prepared.indices,"API retains compact index topology");
 Check(!memcmp(observedVertices.data(),prepared.vertices.data(),observedVertices.size()*sizeof(observedVertices[0])),"API receives all explicit 64-byte vertices");
}
static void DrawOkay(submit::Prepared& prepared,remixapi_MeshHandle mesh,remixapi_MaterialHandle material){
 active=&prepared;activeMesh=mesh;activeMaterial=material;drawState.cull=prepared.cull;
 const auto result=submit::Draw(mesh,material,prepared,drawState,contract);
 Check(result.apiCalled&&result.apiSucceeded&&result.stateStable,"one successful stable API call");
}
static void Tests(){
 static_assert(sizeof(remixapi_HardcodedVertex)==64&&offsetof(remixapi_HardcodedVertex,normal)==12&&offsetof(remixapi_HardcodedVertex,color)==32);
 for(unsigned b=1;b<=4;++b){Clean();++frameId;auto source=Quad(b);auto prepared=Prepare(source);const auto material=Material(100+b);
  Check(prepared.vertices.size()==4&&prepared.indices.size()==6,"quad stays four vertices instead of six");
  const auto mesh=submit::Resource(prepared,material);Check(mesh!=nullptr,"compact resource created");Payload(prepared);
  Check(surfaceMeshBytes==4*64+6*4,"actual compact byte accounting");
  const auto calls=creates;Check(submit::Resource(prepared,material)==mesh&&creates==calls,"same geometry/material reuses immutable API mesh");
  source.vertices[0].skin.position[0]=99;Check(prepared.vertices[0].position[0]!=99,"prepared copy owns input");DrawOkay(prepared,mesh,material);
  auto pose=Prepare(source);Check(submit::Resource(pose,material)!=mesh,"changed pose creates new immutable mesh");
  auto topology=prepared;std::swap(topology.indices[0],topology.indices[1]);Check(submit::Resource(topology,material)!=mesh,"topology participates in cache key");
  auto color=prepared;color.vertices[0].color^=1;Check(submit::Resource(color,material)!=mesh,"explicit material vertex color participates in key");
  auto otherMaterial=Material(200+b);Check(submit::Resource(prepared,otherMaterial)!=mesh,"material identity participates in key");
  const auto count=creates;auto malformed=prepared;malformed.indices[0]=4;Check(!submit::Resource(malformed,material)&&creates==count,"invalid index rejects before API");
  Check(!submit::Resource(prepared,reinterpret_cast<remixapi_MaterialHandle>(99999))&&creates==count,"foreign material handle rejected");
  auto clockwise=Prepare(source,D3DCULL_CW);Check(clockwise.indices==std::vector<uint32_t>({1,0,2,2,0,3}),"clockwise cull reverses each triangle once");
  auto clockwiseMesh=submit::Resource(clockwise,material);DrawOkay(clockwise,clockwiseMesh,material);
  const auto before=prepared.vertices;Check(!submit::Prepare(source,{},D3DCULL_NONE,prepared)&&!memcmp(before.data(),prepared.vertices.data(),before.size()*sizeof(before[0])),"missing material color refuses without changing previous output");
 }
 Clean();++frameId;auto prepared=Prepare(Quad(4));auto material=Material(300);auto mesh=submit::Resource(prepared,material);DrawOkay(prepared,mesh,material);
 reenterDraw=true;DrawOkay(prepared,mesh,material);reenterDraw=false;
 failDraw=true;auto result=submit::Draw(mesh,material,prepared,drawState,contract);failDraw=false;
 Check(result.apiCalled&&!result.apiSucceeded&&result.stateStable,"API failure records irreversible call boundary");
 throwDraw=true;result=submit::Draw(mesh,material,prepared,drawState,contract);throwDraw=false;
 Check(result.apiCalled&&!result.apiSucceeded,"allocation exception after call cannot authorize fallback");
 const auto before=draws;drawState.operation=D3DBLENDOP_SUBTRACT;result=submit::Draw(mesh,material,prepared,drawState,contract);drawState.operation=D3DBLENDOP_ADD;
 Check(!result.apiCalled&&draws==before,"unsupported blend rejects before submission");
 retireDraw=true;result=submit::Draw(mesh,material,prepared,drawState,contract);retireDraw=false;
 Check(result.apiCalled&&result.apiSucceeded&&!result.stateStable&&meshes.empty()&&materials.empty(),"retirement during draw drains mesh before material after call returns");
 Check(!submit::Draw(mesh,material,prepared,drawState,contract).apiCalled,"stale handle after reset retirement rejects");
 ++frameId;material=Material(400);failCreate=true;const auto destroyed=destroys;
 Check(!submit::Resource(prepared,material)&&destroys==destroyed+1&&meshes.empty()&&!surfaceMeshBytes,"failing Create with nonnull handle retains then releases ownership");failCreate=false;Clean();
 auto input=Quad(4);submit::Color4Prepared projected;
 Check(submit::PrepareColor4(input,{1,1,1,1},D3DCULL_NONE,projected),"ColorMode4 black additive RGB qualifies separately from albedo");
 Check(material_channels::Unit(projected.material.albedo)&&material_channels::Zero(projected.material.emission)&&projected.geometry.vertices[0].color==0xffffffff,"black illumination never blackens reflectance");
 for(auto& vertex:input.vertices)vertex.color=0x007f3f1f;
 Check(submit::PrepareColor4(input,{.25f,.5f,.75f,1},D3DCULL_NONE,projected)&&projected.geometry.vertices[0].color==0xffffffff,"uniform additive color folds into emission, original vertex alpha ignored in mode4");
 Check(projected.material.albedo.v[0]==.25f&&projected.material.emission.v[0]==127.f/255&&projected.material.emission.v[1]==63.f/255,"separate reflectance and additive emission coefficients");
 const auto previous=projected.material;input.vertices[1].color=0xff112233;
 Check(!submit::PrepareColor4(input,{1,1,1,1},D3DCULL_NONE,projected)&&!memcmp(&previous,&projected.material,sizeof(previous)),"variable additive RGB with constant albedo refuses without flattening or changing output");
 Check(submit::PrepareColor4(input,{0,0,0,1},D3DCULL_NONE,projected)&&projected.material.vertexRGB&&material_channels::Unit(projected.material.emission),"zero reflectance admits variable emission using the common vertex multiplier");
 Check(projected.geometry.vertices[1].color==0xff112233,"variable emissive RGB reaches API while alpha is explicitly opaque");
 Check(!submit::PrepareColor4(input,{1,1,1,.5f},D3DCULL_NONE,projected),"nonopaque material alpha awaits a separate policy");
 input.attributes=1;for(auto& vertex:input.vertices)vertex.color=0xffffffff;
 Check(!submit::PrepareColor4(input,{1,1,1,1},D3DCULL_NONE,projected),"missing authored COLOR0 cannot borrow an unrelated default");
}
static packet::Packet Read(const std::string& path){std::ifstream stream(path,std::ios::binary|std::ios::ate);Check(bool(stream),"packet file open");const auto size=stream.tellg();Check(size>0&&uint64_t(size)<=packet::MaximumWireBytes,"packet file bound");
 std::vector<uint8_t> bytes(size_t(size),0);stream.seekg(0);Check(bool(stream.read(reinterpret_cast<char*>(bytes.data()),size)),"packet complete read");packet::Packet p;Check(packet::Decode(bytes,p),"SKP1 decode");return p;}
static void Replay(const char* manifest){std::ifstream stream(manifest);Check(bool(stream),"manifest open");std::string path;size_t packets=0,vertices=0,indices=0,bytes=0;const auto material=Material(1000);
 while(std::getline(stream,path)){if(!path.empty()&&path.back()=='\r')path.pop_back();Check(!path.empty()&&path.size()<1024&&++packets<=128,"bounded manifest");
  auto packet=Read(path);
  if(gpuMode){namespace gpu=winx_remix::skin_gpu_submit;gpu::Prepared input;
   Check(gpu::Prepare(packet,std::vector<uint32_t>(packet.vertices.size(),0xffabcdef),D3DCULL_NONE,input),"captured signed packet prepares");
   const auto mesh=gpu::Resource(input,material,packets);Check(mesh!=nullptr,"captured signed resource owned");
   for(unsigned pose=0;pose<2;++pose){++frameId;if(pose)input.transforms[0].matrix[0][3]+=1;
    Check(gpu::Resource(input,material,packets)==mesh,"second wire pose keeps the same immutable mesh");
    const auto result=gpu::Draw(mesh,material,input,drawState,contract,[]{return true;});
    Check(result.apiCalled&&result.apiSucceeded&&result.stateStable,"captured signed instance emitted once");
   }
   vertices+=input.vertices.size();indices+=input.indices.size();
   bytes+=input.vertices.size()*64+input.indices.size()*4+input.skin.weights.size()*8;
  }else {auto prepared=Prepare(packet);const auto mesh=submit::Resource(prepared,material);Check(mesh!=nullptr,"live packet resource");Payload(prepared);DrawOkay(prepared,mesh,material);
   vertices+=prepared.vertices.size();indices+=prepared.indices.size();bytes+=prepared.vertices.size()*64+prepared.indices.size()*4;
  }
 }
 Check(stream.eof()&&packets>0,"complete nonempty manifest");Clean();
 printf("{\"status\":\"PASS\",\"packets\":%zu,\"vertices\":%zu,\"indices\":%zu,\"compactBytes\":%zu,\"expandedBytes\":%zu,\"checks\":%u,\"gpu\":false,\"materialQualified\":false}\n",packets,vertices,indices,bytes,indices*68,checks);
}
template<class K,size_t N> static void Set(const K (&keys)[N],std::array<DWORD,N>& values,K key,DWORD value){
 for(size_t i=0;i<N;++i)if(keys[i]==key){values[i]=value;return;}throw std::runtime_error("unknown fixture state");
}
static winx_remix::skin_draw_state::Snapshot InitialState(){
 using namespace winx_remix::skin_draw_state;Snapshot s;s.vertexShader=1;s.textures[0]=2;
 const auto r=[&](D3DRENDERSTATETYPE k,DWORD v){Set(StateKeys,s.states,k,v);};
 const auto t=[&](D3DTEXTURESTAGESTATETYPE k,DWORD v){Set(StageKeys,s.stages[0],k,v);};
 const auto a=[&](D3DSAMPLERSTATETYPE k,DWORD v){Set(SamplerKeys,s.samplers[0],k,v);};
 r(D3DRS_ZENABLE,D3DZB_TRUE);r(D3DRS_ZWRITEENABLE,1);r(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
 r(D3DRS_ALPHABLENDENABLE,1);r(D3DRS_SRCBLEND,D3DBLEND_ONE);r(D3DRS_DESTBLEND,D3DBLEND_ZERO);r(D3DRS_BLENDOP,D3DBLENDOP_ADD);
 r(D3DRS_ALPHATESTENABLE,1);r(D3DRS_ALPHAFUNC,D3DCMP_GREATER);r(D3DRS_ALPHAREF,0);r(D3DRS_CULLMODE,D3DCULL_NONE);
 r(D3DRS_COLORWRITEENABLE,15);r(D3DRS_LIGHTING,1);r(D3DRS_TEXTUREFACTOR,0xffffffff);
 t(D3DTSS_COLOROP,D3DTOP_MODULATE);t(D3DTSS_COLORARG1,D3DTA_TEXTURE);t(D3DTSS_COLORARG2,D3DTA_CURRENT);
 t(D3DTSS_ALPHAOP,D3DTOP_MODULATE);t(D3DTSS_ALPHAARG1,D3DTA_TEXTURE);t(D3DTSS_ALPHAARG2,D3DTA_CURRENT);t(D3DTSS_RESULTARG,D3DTA_CURRENT);
 for(unsigned n=1;n<8;++n)Set(StageKeys,s.stages[n],D3DTSS_COLOROP,DWORD(D3DTOP_DISABLE));
 a(D3DSAMP_ADDRESSU,D3DTADDRESS_WRAP);a(D3DSAMP_ADDRESSV,D3DTADDRESS_WRAP);a(D3DSAMP_ADDRESSW,D3DTADDRESS_WRAP);
 a(D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);a(D3DSAMP_MINFILTER,D3DTEXF_LINEAR);a(D3DSAMP_MIPFILTER,D3DTEXF_LINEAR);a(D3DSAMP_MAXANISOTROPY,1);
 return s;
}
static void StateTests(){
 namespace plan=winx_remix::skin_draw;using namespace winx_remix::skin_draw_state;
 auto state=InitialState();plan::Color4 prepared;plan::Error error{};
 const auto oldDraw=drawState;const auto oldContract=contract;
 for(unsigned b=1;b<=4;++b)for(DWORD cull=D3DCULL_NONE;cull<=D3DCULL_CCW;++cull){
  Set(StateKeys,state.states,D3DRS_CULLMODE,cull);
  Check(plan::PrepareColor4(Quad(b),{1,1,1,1},state,prepared,&error)&&error==plan::Error::None,"known state and B1-B4 prepare without model identity");
  drawState=prepared.state.instance;contract=prepared.state.texture;
  const auto material=Material(600+b);const auto mesh=submit::Resource(prepared.packet.geometry,material);
  Payload(prepared.packet.geometry);DrawOkay(prepared.packet.geometry,mesh,material);Clean();
 }
 for(DWORD enabled=0;enabled<=1;++enabled)for(DWORD function=1;function<=8;++function){
  Set(StateKeys,state.states,D3DRS_ALPHATESTENABLE,enabled);Set(StateKeys,state.states,D3DRS_ALPHAFUNC,function);Set(StateKeys,state.states,D3DRS_ALPHAREF,DWORD(127));
  Check(plan::PrepareColor4(Quad(4),{1,1,1,1},state,prepared),"all common alpha compare modes prepare");
  drawState=prepared.state.instance;contract=prepared.state.texture;
  const auto material=Material(700);const auto mesh=submit::Resource(prepared.packet.geometry,material);
  DrawOkay(prepared.packet.geometry,mesh,material);Clean();
 }
 state=InitialState();Check(plan::PrepareColor4(Quad(4),{1,1,1,1},state,prepared),"baseline before refusal tests");
 const auto original=prepared.packet.geometry.vertices;const auto originalIndices=prepared.packet.geometry.indices;
 const auto refuse=[&](const Snapshot& bad,plan::Error expected){
  Check(!plan::PrepareColor4(Quad(4),{1,1,1,1},bad,prepared,&error)&&error==expected,"unsupported state has explicit reason");
  Check(prepared.packet.geometry.indices==originalIndices&&!memcmp(original.data(),prepared.packet.geometry.vertices.data(),original.size()*sizeof(original[0]))&&prepared.state.instance.function==D3DCMP_GREATER,"refusal preserves previous prepared output");
 };
 auto bad=state;bad.vertexShader=0;refuse(bad,plan::Error::Shader);bad=state;bad.pixelShader=1;refuse(bad,plan::Error::Shader);
 bad=state;bad.textures[0]=0;refuse(bad,plan::Error::Texture);
 for(unsigned n=1;n<8;++n){bad=state;bad.textures[n]=3;refuse(bad,plan::Error::Texture);bad=state;
  Set(StageKeys,bad.stages[n],D3DTSS_COLOROP,DWORD(D3DTOP_MODULATE));refuse(bad,plan::Error::Stage);}
 const struct {D3DRENDERSTATETYPE key;DWORD value;plan::Error reason;} cases[]={
  {D3DRS_ZENABLE,0,plan::Error::Depth},{D3DRS_ZWRITEENABLE,0,plan::Error::Depth},{D3DRS_ZFUNC,D3DCMP_ALWAYS,plan::Error::Depth},
  {D3DRS_ALPHABLENDENABLE,0,plan::Error::Blend},{D3DRS_SRCBLEND,D3DBLEND_SRCALPHA,plan::Error::Blend},
  {D3DRS_DESTBLEND,D3DBLEND_ONE,plan::Error::Blend},{D3DRS_BLENDOP,D3DBLENDOP_SUBTRACT,plan::Error::Blend},{D3DRS_SEPARATEALPHABLENDENABLE,1,plan::Error::Blend},
  {D3DRS_ALPHATESTENABLE,2,plan::Error::Alpha},{D3DRS_ALPHAFUNC,0,plan::Error::Alpha},{D3DRS_ALPHAFUNC,9,plan::Error::Alpha},{D3DRS_ALPHAREF,256,plan::Error::Alpha},
  {D3DRS_CULLMODE,0,plan::Error::Cull},{D3DRS_CULLMODE,4,plan::Error::Cull},{D3DRS_COLORWRITEENABLE,7,plan::Error::Write},
  {D3DRS_STENCILENABLE,1,plan::Error::Effects},{D3DRS_SCISSORTESTENABLE,1,plan::Error::Effects},{D3DRS_SRGBWRITEENABLE,1,plan::Error::Effects},
  {D3DRS_FOGENABLE,1,plan::Error::Effects},{D3DRS_SPECULARENABLE,1,plan::Error::Effects}};
 for(const auto& c:cases){bad=state;Set(StateKeys,bad.states,c.key,c.value);refuse(bad,c.reason);}
 for(size_t i=0;i<std::size(StageKeys);++i){if(StageKeys[i]==D3DTSS_COLORARG0||StageKeys[i]==D3DTSS_ALPHAARG0)continue;
  bad=state;bad.stages[0][i]^=0x10;refuse(bad,plan::Error::Stage);}
 for(size_t i=0;i<std::size(SamplerKeys);++i){if(SamplerKeys[i]==D3DSAMP_BORDERCOLOR)continue;
  bad=state;bad.samplers[0][i]^=0x10;refuse(bad,plan::Error::Sampler);}
 for(DWORD flags:{DWORD(D3DTTFF_COUNT1),DWORD(D3DTTFF_COUNT3),DWORD(D3DTTFF_COUNT4),DWORD(D3DTTFF_COUNT2|D3DTTFF_PROJECTED),DWORD(D3DTTFF_COUNT3|D3DTTFF_PROJECTED)}) {
  bad=state;Set(StageKeys,bad.stages[0],D3DTSS_TEXTURETRANSFORMFLAGS,flags);refuse(bad,plan::Error::Stage);
 }
 bad=state;Set(StageKeys,bad.stages[0],D3DTSS_TEXTURETRANSFORMFLAGS,DWORD(D3DTTFF_COUNT2));bad.transform.fill(0xffffffff);
 Check(plan::PrepareColor4(Quad(4),{1,1,1,1},bad,prepared)&&prepared.state.texture.transformFlags==D3DTTFF_DISABLE,
   "observed COUNT2 with proven shader cannot apply the inactive FFP matrix again");
 // Inactive inputs are intentionally excluded from the material equation.
 bad=state;Set(StateKeys,bad.states,D3DRS_LIGHTING,DWORD(0));Set(StateKeys,bad.states,D3DRS_FOGCOLOR,DWORD(0x123456));
 Set(StateKeys,bad.states,D3DRS_SRCBLENDALPHA,DWORD(D3DBLEND_DESTALPHA));bad.transform.fill(0xffffffff);
 Set(StageKeys,bad.stages[0],D3DTSS_COLORARG0,DWORD(D3DTA_TEMP));Set(SamplerKeys,bad.samplers[0],D3DSAMP_BORDERCOLOR,DWORD(123));
 Check(plan::PrepareColor4(Quad(4),{1,1,1,1},bad,prepared),"inactive FFP inputs are not mistaken for shader illumination");
 Check(!plan::PrepareColor4(Quad(4),{1,1,1,.5f},state,prepared,&error)&&error==plan::Error::Projection,"unsupported material policy propagates its own boundary");
 Snapshot read=state;unsigned failure=0;Check(!ReadSnapshot(nullptr,read,&failure)&&failure==1&&read==state,"failed state acquisition is atomic");
 drawState=oldDraw;contract=oldContract;
}
static void Project(const char* packetPath,const char* parameters){
 const auto packet=Read(packetPath);std::ifstream stream(parameters,std::ios::binary|std::ios::ate);
 Check(bool(stream)&&stream.tellg()==368,"FLP1 exact extent");std::array<uint8_t,368> bytes{};stream.seekg(0);Check(bool(stream.read(reinterpret_cast<char*>(bytes.data()),bytes.size())),"FLP1 complete read");
 uint32_t header[4]{};memcpy(header,bytes.data(),16);Check(header[0]==0x31504c46&&header[1]==1&&header[2]==4&&header[3]<=8,"FLP1 ColorMode4 directional input");
 packet::pc::FixedSkinVector diffuse{};memcpy(diffuse.data(),bytes.data()+80,16);submit::Color4Prepared out;
 Check(submit::PrepareColor4(packet,diffuse,D3DCULL_NONE,out),"bounded initial ColorMode4 projection");
 for(const auto& vertex:out.geometry.vertices)Check(vertex.color==material_channels::Vertex(packet.vertices[&vertex-out.geometry.vertices.data()].color,out.material),"policy colors reach compact API input");
 printf("{\"status\":\"PASS\",\"vertices\":%zu,\"albedo\":[%.9g,%.9g,%.9g],\"emission\":[%.9g,%.9g,%.9g],\"vertexRGB\":%s,\"alpha\":%u,\"materialQualified\":false,\"gpu\":false}\n",out.geometry.vertices.size(),out.material.albedo.v[0],out.material.albedo.v[1],out.material.albedo.v[2],out.material.emission.v[0],out.material.emission.v[1],out.material.emission.v[2],out.material.vertexRGB?"true":"false",out.material.alpha);
}
}
#include "test_skin_gpu_submit.h"
namespace shader_snapshot_test {
#if defined(_M_IX86)
using submit_test::Check;
namespace semantic=shader_semantics;
struct Fixture;
static Fixture* active;
static uintptr_t __fastcall DummySelect(void*,void*,uint32_t,uint32_t){return 0;}
static HRESULT STDMETHODCALLTYPE GetFunction(IDirect3DVertexShader9*,void*,UINT*);
struct Fixture {
  std::array<void*,5> table{};void** shader=table.data();
  std::array<uint32_t,21> header{};std::array<uint32_t,2> nativeCode{0xfffe0101,0xffff},boundCode=nativeCode;
  uint32_t slot=uint32_t(reinterpret_cast<uintptr_t>(&semantic::Select));unsigned calls=0,mode=0;
  semantic::Selection previous=semantic::current;
  semantic::NativeSelect previousSelect=semantic::originalSelect;
  DWORD previousThread=semantic::ownerThread;uintptr_t previousSlot=semantic::selectionSlot;
  Fixture() {
    active=this;table[4]=reinterpret_cast<void*>(GetFunction);
    header[0]=0x6f2ecc;header[0x48/4]=8;header[0x4c/4]=uint32_t(reinterpret_cast<uintptr_t>(nativeCode.data()));
    header[0x50/4]=uint32_t(reinterpret_cast<uintptr_t>(Shader()));
    semantic::current={0x10000,reinterpret_cast<uintptr_t>(header.data()),0x40011,0,frameId,1};
    semantic::originalSelect=reinterpret_cast<semantic::NativeSelect>(DummySelect);semantic::ownerThread=GetCurrentThreadId();semantic::selectionSlot=reinterpret_cast<uintptr_t>(&slot);
  }
  ~Fixture(){semantic::current=previous;semantic::originalSelect=previousSelect;semantic::ownerThread=previousThread;semantic::selectionSlot=previousSlot;active=nullptr;}
  IDirect3DVertexShader9* Shader(){return reinterpret_cast<IDirect3DVertexShader9*>(&shader);}
};
static HRESULT STDMETHODCALLTYPE GetFunction(IDirect3DVertexShader9*,void* bytes,UINT* size) {
  auto& f=*active;++f.calls;
  if(f.mode==7)return D3DERR_INVALIDCALL;
  if(f.mode==8){*size=12;return D3D_OK;}
  if(f.mode==9&&f.calls==1)++semantic::current.sequence;
  if(bytes){Check(*size>=8,"bound bytecode buffer has the original requested size");memcpy(bytes,f.boundCode.data(),8);}
  *size=8;
  if(f.mode==10&&f.calls==2)f.nativeCode[1]^=1;
  if(f.mode==11&&f.calls==2)++f.header[1];
  return D3D_OK;
}
static void Run() {
  for(unsigned reason=0;reason<14;++reason){
    Fixture f;f.mode=reason;semantic::BoundSelection result;
    switch(reason){
      case 1:f.header[0]=0;break;case 2:f.header[0x50/4]=0;break;case 3:f.header[0x48/4]=16388;break;
      case 4:++semantic::current.frame;break;case 5:f.slot=0;break;case 6:f.boundCode[1]^=1;break;
      case 12:semantic::ownerThread=0;break;case 13:f.header[0x4c/4]=1;break;
    }
    const bool accepted=semantic::ReadBound(f.Shader(),result);
    if(reason){Check(!accepted&&result.bytes.empty(),"invalid or changed native/COM shader does not publish a snapshot");}
    else {
      Check(accepted&&f.calls==2&&semantic::Current(result),"current native shader and owned COM bytecode produce a stable snapshot");
      ++semantic::current.sequence;Check(!semantic::Current(result),"another selection invalidates a saved shader even if its address is unchanged");--semantic::current.sequence;
      f.nativeCode[1]^=1;Check(!semantic::Current(result),"native code changed after external work invalidates saved shader");
    }
  }
}
#else
static void Run() {}
#endif
}
int main(int argc,char** argv){try{using namespace submit_test;
 Watchdog watchdog;
#if defined(_WIN64)
 skin_packet_capture::Initialize();Check(!skin_packet_capture::Enabled(),"x64 API consumer cannot enable PC x86 game capture");
#endif
 remixapi_Interface api{};api.CreateMaterial=CreateMaterial;api.DestroyMaterial=DestroyMaterial;api.CreateMesh=CreateMesh;api.DestroyMesh=DestroyMesh;api.DrawInstance=DrawInstance;testRemixApi=&api;
#ifdef WINX_SKIN_PACKET_WIRE_TEST
 if(argc==4&&(std::string(argv[1])=="--wire-write"||std::string(argv[1])=="--wire-read"||
              std::string(argv[1])=="--wire-signed-write"||std::string(argv[1])=="--wire-signed-read")){
  wireReading=std::string(argv[1]).find("read")!=std::string::npos;
  gpuMode=std::string(argv[1]).find("signed")!=std::string::npos;
  if(!wireReading){std::ifstream existing(argv[3],std::ios::binary);Check(!existing,"fresh wire file required");}
  wireStream.open(argv[3],std::ios::binary|(wireReading?std::ios::in:std::ios::out));Check(bool(wireStream),"wire file open");
  Replay(argv[2]);if(wireReading)Check(wireStream.peek()==std::char_traits<char>::eof(),"no extra wire records");else wireStream.flush();
  Check(!wireStream.bad(),"wire stream complete");wireStream.close();
  printf("{\"status\":\"PASS\",\"wireRecords\":%zu,\"wireBytes\":%zu,\"reading\":%s,\"checks\":%u,\"gpu\":false,\"ipc\":false}\n",wireRecords,wireBytes,wireReading?"true":"false",checks);return 0;
 }
#endif
 if(argc==3&&std::string(argv[1])=="--manifest"){Replay(argv[2]);return 0;}
 if(argc==4&&std::string(argv[1])=="--project-color4"){Project(argv[2],argv[3]);return 0;}
 Check(argc==1,"usage: test_skin_packet_submit [--manifest paths.txt]");Tests();StateTests();skin_gpu_test::Run();shader_snapshot_test::Run();Clean();
 printf("{\"status\":\"PASS\",\"checks\":%u,\"creates\":%u,\"destroys\":%u,\"draws\":%u,\"materialCreates\":%u,\"materialDestroys\":%u,\"remaining\":0,\"gpu\":false}\n",checks,creates,destroys,draws,materialCreates,materialDestroys);return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
