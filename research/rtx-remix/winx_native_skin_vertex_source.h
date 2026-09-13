// Own sampled API-admission audit. It reads raw source/upload data; no skin
// deformation, producer, COM call, game write or Remix submission is performed.
#pragma once
namespace native_skin_vertex_source {
static FILE* output;
static bool enabled;
static unsigned attempts,matched,rejected;
struct Summary {
  unsigned vertices=0,influences=0,negative=0,nonUnit=0;
  float minWeight=0,maxWeight=0;
  double maxSumError=0;
  uint32_t maxIndex=0;
};
static bool Inspect(const std::vector<uint8_t>& bytes,const std::vector<uint8_t>& indices,
    const D3DVERTEXELEMENT9* layout,unsigned elements,unsigned stride,
    uint32_t vertexBegin,uint32_t vertexCount,uint32_t indexBegin,uint32_t primitiveCount,
    uint32_t primitiveType,unsigned influences,unsigned paletteCount,Summary& result) {
  result={};
  if(!layout||!elements||elements>MAXD3DDECLLENGTH+1||layout[elements-1].Stream!=0xff||
     !stride||!vertexCount||vertexCount>65536||!primitiveCount||primitiveCount>32768||
     (primitiveType!=2&&primitiveType!=3)||!influences||influences>4||!paletteCount||paletteCount>256)return false;
  int weightOffset=-1,indexOffset=-1,positionOffset=-1;
  for(unsigned i=0;i<elements&&layout[i].Stream!=0xff;++i){const auto& e=layout[i];
    const unsigned size=e.Type<=D3DDECLTYPE_FLOAT4?(unsigned(e.Type)+1)*4:e.Type==D3DDECLTYPE_D3DCOLOR?4:0;
    if(e.Stream||e.Method!=D3DDECLMETHOD_DEFAULT||!size||e.Offset>stride||size>stride-e.Offset)return false;
    if(e.Usage==D3DDECLUSAGE_POSITION&&e.UsageIndex==0&&e.Type==D3DDECLTYPE_FLOAT3){if(positionOffset>=0)return false;positionOffset=e.Offset;}
    if(e.Usage==D3DDECLUSAGE_BLENDWEIGHT&&e.UsageIndex==0){if(weightOffset>=0||
       (influences==1?e.Type!=D3DDECLTYPE_FLOAT1:e.Type!=D3DDECLTYPE_FLOAT4))return false;weightOffset=e.Offset;}
    if(e.Usage==D3DDECLUSAGE_BLENDINDICES&&e.UsageIndex==0){if(indexOffset>=0||e.Type!=D3DDECLTYPE_FLOAT4)return false;indexOffset=e.Offset;}
  }
  if(positionOffset<0||weightOffset<0||indexOffset<0||unsigned(positionOffset)+12>stride||
     unsigned(weightOffset)+influences*4>stride||unsigned(indexOffset)+16>stride)return false;
  const uint64_t indexCount=primitiveType==2?uint64_t(primitiveCount)*3:uint64_t(primitiveCount)+2;
  if((uint64_t(indexBegin)+indexCount)*2>indices.size()||
     (uint64_t(vertexBegin)+vertexCount)*stride>bytes.size())return false;
  std::vector<uint8_t> visited(vertexCount,0);Summary value{};value.influences=influences;
  for(uint64_t i=0;i<indexCount;++i){uint16_t index=0;memcpy(&index,indices.data()+(uint64_t(indexBegin)+i)*2,2);
    if(index>=vertexCount)return false;if(visited[index])continue;visited[index]=1;
    const auto row=bytes.data()+(uint64_t(vertexBegin)+index)*stride;
    float position[3]{},weights[4]{},bones[4]{};memcpy(position,row+positionOffset,12);
    memcpy(weights,row+weightOffset,influences*4);memcpy(bones,row+indexOffset,16);
    for(float component:position)if(!std::isfinite(component))return false;
    bool negative=false;double sum=0;
    for(unsigned j=0;j<influences;++j){if(!std::isfinite(weights[j])||!std::isfinite(bones[j])||
       bones[j]<0||bones[j]>=paletteCount||std::floor(bones[j])!=bones[j])return false;
      negative|=weights[j]<0;sum+=weights[j];
      if(!value.vertices&&!j)value.minWeight=value.maxWeight=weights[j];
      value.minWeight=(std::min)(value.minWeight,weights[j]);
      value.maxWeight=(std::max)(value.maxWeight,weights[j]);value.maxIndex=(std::max)(value.maxIndex,uint32_t(bones[j]));}
    const double error=std::fabs(sum-1.0);value.maxSumError=(std::max)(value.maxSumError,error);
    value.negative+=negative?1u:0u;value.nonUnit+=error>1e-5?1u:0u;++value.vertices;
  }
  if(!value.vertices)return false;result=value;return true;
}
static void Capture(const native_skin_source::Observation& skin) {
#if defined(_M_IX86)
  if(!enabled||!output||_ftelli64(output)>=16*1024*1024||!(frameId%300==0||triggered))return;
  ++attempts;bool accepted=false;int rejection=0;Summary summary{};uint64_t generation=0;
  try {
    std::unique_lock<std::recursive_mutex> borrow(guard);
    namespace source=native_mesh_source;namespace abi=sparkplug::evidence::pc;
    const auto scope=native_skin_source::active;
    abi::spDXMeshObservedLayout mesh{};source::ResourcePair pair{};uint32_t declaration[7]{},device=0;
    if(!scope||scope->sequence!=skin.modelCall||scope->skin!=skin.skin||
       !source::Read(skin.mesh,mesh)||scene_geometry::Word(skin.mesh)!=abi::spDXMeshVTable||
       mesh.base.base.secondaryVTable!=abi::spDXMeshInterfaceVTable||
       (mesh.indexType!=2&&mesh.indexType!=3)||!mesh.componentWeightCount||mesh.componentWeightCount>4||
       !source::ReadResourceHeaders(mesh,pair)||!source::Read(skin.renderer+0xc9e8,device)||
       !source::Read(mesh.vertexDeclaration,declaration)||declaration[0]!=0x6f2e58||declaration[5]!=mesh.fvfCode)throw 1;
    auto vb=reinterpret_cast<void*>(pair.vb.direct3DVertexBuffer),ib=reinterpret_cast<void*>(pair.ib.direct3DIndexBuffer);
    source::TransportWitness transport{};
    if(!native_transport_source::Witness(borrow,reinterpret_cast<IDirect3DDevice9*>(device),vb,ib,
       reinterpret_cast<void*>(declaration[6]),transport)||transport.indexFormat!=D3DFMT_INDEX16||
       transport.vertexBytes!=pair.vb.byteSize||transport.indexBytes!=pair.ib.byteSize||
       !source::ResolveResourceRanges(mesh,mesh.vertexStride,vb,ib,pair)||
       surfaceWrites.count(vb)||surfaceWrites.count(ib))throw 2;
    const auto v=surfaceBuffers.find(vb),i=surfaceBuffers.find(ib);
    if(v==surfaceBuffers.end()||i==surfaceBuffers.end()||!v->second.complete||!i->second.complete||
       !source::EqualsUpload(*pair.vertices,v->second.bytes)||!source::EqualsUpload(*pair.indices,i->second.bytes))throw 3;
    const auto layout=source::Layout(mesh.base.base.vertexComponentFlags);
    if(!layout||layout->size()!=transport.elementCount||memcmp(layout->data(),transport.elements,layout->size()*sizeof(D3DVERTEXELEMENT9)))throw 4;
    if(!Inspect(pair.vertices->data,pair.indices->data,transport.elements,transport.elementCount,
       mesh.vertexStride,mesh.vertexBegin,mesh.base.base.vertexCount,mesh.indexBegin,mesh.base.base.primitiveCount,
       mesh.indexType,mesh.componentWeightCount,skin.boneCount,summary))throw 5;
    abi::spDXMeshObservedLayout final{};source::ResourcePair finalPair{};uint32_t finalDeclaration[7]{},finalDevice=0;
    if(!source::Read(skin.mesh,final)||memcmp(&mesh,&final,sizeof(mesh))||
       !source::ReadResourceHeaders(mesh,finalPair)||memcmp(&pair.vb,&finalPair.vb,sizeof(pair.vb))||
       memcmp(&pair.ib,&finalPair.ib,sizeof(pair.ib))||
       !source::Read(mesh.vertexDeclaration,finalDeclaration)||memcmp(declaration,finalDeclaration,sizeof(declaration))||
       !source::Read(skin.renderer+0xc9e8,finalDevice)||device!=finalDevice||
       !native_transport_source::Current(borrow,transport)||native_skin_source::active!=scope||
       scope->sequence!=skin.modelCall||scope->frame!=frameId||scope->mutation!=scene_geometry::MutationSerial())throw 6;
    generation=pair.vertices->generation;accepted=true;
  }catch(int reason){rejection=reason;}catch(...){rejection=7;}
  if(accepted)++matched;else ++rejected;
  fprintf(output,"{\"event\":\"sample\",\"frame\":%u,\"skin\":%u,\"mesh\":%u,\"scene\":%u,\"modelCall\":%llu,\"submission\":%llu,\"accepted\":%s,\"rejection\":%d,\"generation\":%llu,\"vertices\":%u,\"influences\":%u,\"boneCount\":%u,\"maxIndex\":%u,\"negativeWeightVertices\":%u,\"nonUnitSumVertices\":%u,\"maxSumError\":%.17g,\"minWeight\":%.9g,\"maxWeight\":%.9g}\n",
    frameId,skin.skin,skin.mesh,skin.scene,skin.modelCall,skin.submission,accepted?"true":"false",rejection,generation,
    summary.vertices,summary.influences,skin.boneCount,summary.maxIndex,summary.negative,summary.nonUnit,
    summary.maxSumError,summary.minWeight,summary.maxWeight);fflush(output);
#else
  (void)skin;
#endif
}
static void Initialize(){wchar_t path[MAX_PATH]{};const auto length=GetEnvironmentVariableW(L"WINX_REMIX_NATIVE_SKIN_VERTICES",path,MAX_PATH);
  if(!length||length>=MAX_PATH||!native_skin_source::enabled)return;output=_wfsopen(path,L"wb",_SH_DENYNO);enabled=output!=nullptr;
  if(output){fprintf(output,"{\"event\":\"init\",\"schema\":1,\"pid\":%lu,\"enabled\":true,\"maxLogBytes\":16777216,\"scope\":\"sampled qualified Skin calls; exact CPU-source/upload and shared-layout comparison; no weighted API submission; unit-sum tolerance1e-5\"}\n",GetCurrentProcessId());fflush(output);}}
static void EndFrame(){if(output&&_ftelli64(output)<16*1024*1024&&(attempts||frameId%300==0)){
  fprintf(output,"{\"event\":\"frame\",\"frame\":%u,\"attempts\":%u,\"matched\":%u,\"rejected\":%u}\n",frameId,attempts,matched,rejected);fflush(output);}
  attempts=matched=rejected=0;}
}
