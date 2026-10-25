// Own CPU/recording fixture. It qualifies adapter identities and live material
// binding; it does not execute original game code, a GPU or Toolkit replacement.
#define WINX_INDEPENDENT_SUBMIT_FIXTURE_ONLY
#include "../scene/test_independent_submit.cpp"

namespace identity_adapter_test {
using namespace direct_test;
struct MaterialRecord {uint64_t hash=0;remixapi_MaterialHandle handle=nullptr;};
struct MeshRecord {
  uint64_t hash=0;remixapi_MaterialHandle material=nullptr;remixapi_MeshHandle handle=nullptr;
  remixapi_HardcodedVertex vertices[3]{};
};
static MaterialRecord recordedMaterials[64]{};
static MeshRecord recordedMeshes[64]{};
static unsigned recordedMaterialCount,recordedMeshCount;
static remixapi_ErrorCode REMIXAPI_CALL RecordMaterial(const remixapi_MaterialInfo* info,remixapi_MaterialHandle* output) {
  Check(info&&output&&recordedMaterialCount<64,"bounded material API identity recording");
  auto& record=recordedMaterials[recordedMaterialCount++];record={};record.hash=info->hash;
  const auto result=direct_test::CreateMaterial(info,output);record.handle=*output;return result;
}
static remixapi_ErrorCode REMIXAPI_CALL RecordMesh(const remixapi_MeshInfo* info,remixapi_MeshHandle* output) {
  Check(info&&output&&info->surfaces_count==1&&recordedMeshCount<64,"bounded mesh API identity recording");
  const auto& surface=info->surfaces_values[0];
  Check(surface.vertices_count==3&&surface.indices_count==3,"identity fixture records exact triangle");
  Check(surfaceMeshes.count(info->hash)==1&&!surfaceMeshes.at(info->hash).handle&&!surfaceMeshes.at(info->hash).usable,
    "actual API mesh hash is also the pending registry key before Create");
  auto& record=recordedMeshes[recordedMeshCount++];record={};record.hash=info->hash;record.material=surface.material;
  memcpy(record.vertices,surface.vertices_values,sizeof(record.vertices));
  const auto result=direct_test::CreateMesh(info,output);record.handle=*output;return result;
}
static void Attach(Fixture& fixture) {
  recordedMaterialCount=recordedMeshCount=0;
  fixture.api.CreateMaterial=RecordMaterial;fixture.api.CreateMesh=RecordMesh;
}
static remixapi_MaterialHandle Material(uint64_t identity) {
  SurfaceResourceOperation operation;if(!operation.owned)return nullptr;
  remixapi_MaterialInfo info{};info.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO;
  info.hash=identity;info.albedoTexture=L"owned-recording-identity.dds";
  return CreateSurfaceMaterialOwned(info,operation);
}
static std::vector<remixapi_HardcodedVertex> Triangle() {
  std::vector<remixapi_HardcodedVertex> vertices(3);
  vertices[1].position[0]=1;vertices[2].position[1]=1;
  for(auto& vertex:vertices){vertex.normal[2]=1;vertex.color=0xffffffffu;}
  vertices[1].texcoord[0]=1;vertices[2].texcoord[1]=1;return vertices;
}
static uint64_t LegacyMaterial(uint64_t texture,DWORD u,DWORD v,DWORD mag,const material_channels::Plan& plan) {
  // Literal former adapter descriptor, evaluated by the actual XXH3 library.
  // This fixture is Windows x86: uint64 and binary32 have the legacy LE bytes.
  const uint64_t values[]={texture,u,v,mag};std::string bytes="winx-independent-ffp-v1";
  bytes.append(reinterpret_cast<const char*>(values),sizeof(values));
  bytes.append(reinterpret_cast<const char*>(&plan.albedo),sizeof(plan.albedo));
  bytes.append(reinterpret_cast<const char*>(&plan.emission),sizeof(plan.emission));
  return XXH3_64bits(bytes.data(),bytes.size());
}
static uint64_t ExpectedMesh(const remixapi_HardcodedVertex* vertices,size_t count,uint64_t material) {
  // Independent one-shot reference to the documented byte stream. In
  // particular, neither an SDK struct image nor an API handle is appended.
  std::string bytes="winx-geometry-content-v1";const uint64_t counts[]={count,count};
  bytes.append(reinterpret_cast<const char*>(counts),sizeof(counts));
  for(size_t i=0;i<count;++i) {
    bytes.append(reinterpret_cast<const char*>(vertices[i].position),12);
    bytes.append(reinterpret_cast<const char*>(vertices[i].normal),12);
    bytes.append(reinterpret_cast<const char*>(vertices[i].texcoord),8);
    bytes.append(reinterpret_cast<const char*>(&vertices[i].color),4);
  }
  for(uint32_t i=0;i<count;++i)bytes.append(reinterpret_cast<const char*>(&i),4);
  const uint64_t values[]={XXH3_64bits(bytes.data(),bytes.size()),material};
  bytes="winx-bound-mesh-v1";bytes.append(reinterpret_cast<const char*>(values),sizeof(values));
  return XXH3_64bits(bytes.data(),bytes.size());
}
static void MaterialCompatibility() {
  for(unsigned variant=0;variant<4;++variant) {
    material_channels::Plan plan{};plan.albedo={{.125f,.5f,1}};plan.emission={{0,.25f,.75f}};
    if(variant==1)plan.emission.v[0]=-0.0f;
    if(variant==2)plan.albedo.v[1]=.75f;
    const uint64_t texture=variant==3?0xfedcba9876543210ull:0x123456789abcdef0ull;
    const DWORD u=variant%3+1,v=(variant+1)%3+1,mag=(variant+2)%3+1;
    const auto expected=LegacyMaterial(texture,u,v,mag,plan);
    Check(expected&&SurfaceChannelMaterialIdentity(texture,u,v,mag,plan)==expected,
      "new material identity keeps literal legacy XXH3 descriptor and DDS name");
    plan.vertexRGB=!plan.vertexRGB;plan.vertexAlpha=!plan.vertexAlpha;plan.alpha=23;
    Check(SurfaceChannelMaterialIdentity(texture,u,v,mag,plan)==expected,
      "instance and expanded vertex policies remain outside the material coefficient identity");
  }
}
struct PlacementFixture {
  Fixture* value;
  explicit PlacementFixture(void* storage):value(::new(storage) Fixture){Attach(*value);}
  ~PlacementFixture(){value->~Fixture();}
};
static void DifferentProcessLocalIdentities() {
  alignas(Fixture) unsigned char storage[2][sizeof(Fixture)];
  uint64_t materialIdentity[2]{},meshIdentity[2]{};
  uint64_t textureGeneration[2]{},sceneScope[2]{};unsigned sourceFrame[2]{};
  remixapi_MaterialHandle materialHandle[2]{};remixapi_MeshHandle meshHandle[2]{};
  uintptr_t textureAddress[2]{};
  for(unsigned index=0;index<2;++index) {
    nextHandle=0x10000+index*0x3000;
    PlacementFixture placed(storage[index]);auto& fixture=*placed.value;
    scene_geometry::Scope scope(Ptr(&fixture.scene),Ptr(&fixture.camera));fixture.Inputs(scope);fixture.Submit(scope);
    Check(materialCalls==1&&meshCalls==1&&drawCalls==3&&recordedMaterialCount==1&&recordedMeshCount==1,
      "independent path creates one shared resource pair for three actual occurrences");
    materialIdentity[index]=recordedMaterials[0].hash;meshIdentity[index]=recordedMeshes[0].hash;
    materialHandle[index]=recordedMaterials[0].handle;meshHandle[index]=recordedMeshes[0].handle;
    textureAddress[index]=reinterpret_cast<uintptr_t>(fixture.Texture());
    textureGeneration[index]=source::inputs.begin()->second.textureWitness.generation;
    sceneScope[index]=scope.serial;sourceFrame[index]=frameId;
    Check(recordedMeshes[0].material==materialHandle[index]&&
      meshIdentity[index]==ExpectedMesh(recordedMeshes[0].vertices,3,materialIdentity[index]),
      "independent API hash binds content to stable material identity and the actual live material handle");
    Check(surfaceMeshes.count(meshIdentity[index])&&surfaceMeshes.at(meshIdentity[index]).handle==meshHandle[index],
      "accepted API hash remains the registry key after Create");
    fixture.Inputs(scope);fixture.Submit(scope);
    Check(materialCalls==1&&meshCalls==1&&drawCalls==6,
      "new occurrence scope reuses immutable resources without losing repeated Models");
  }
  Check(textureAddress[0]!=textureAddress[1]&&materialHandle[0]!=materialHandle[1]&&meshHandle[0]!=meshHandle[1]&&
    textureGeneration[0]!=textureGeneration[1]&&sceneScope[0]!=sceneScope[1]&&sourceFrame[0]!=sourceFrame[1],
    "literal placement lifecycle generation scope frame and opaque API handle ranges actually changed");
  Check(materialIdentity[0]==materialIdentity[1]&&meshIdentity[0]==meshIdentity[1],
    "same content preserves actual API hashes across changed addresses generations frame scopes and handles");
}
static void ChangedTextureContent() {
  uint64_t textureIdentity[2]{},materialIdentity[2]{},geometryIdentity[2]{},meshIdentity[2]{};
  for(unsigned variant=0;variant<2;++variant) {
    Fixture fixture;Attach(fixture);
    fixture.comTexture.pixel=variant?0xff20a060u:0xff805020u;
    scene_geometry::Scope scope(Ptr(&fixture.scene),Ptr(&fixture.camera));fixture.Inputs(scope);fixture.Submit(scope);
    Check(materialCalls==1&&meshCalls==1&&drawCalls==3&&recordedMaterialCount==1&&recordedMeshCount==1&&textureReadCalls==1,
      "fresh texture-content fixture reaches one sampled material and bound mesh through complete independent submission");
    textureIdentity[variant]=surfaceChannelTextureHashes.at(fixture.Texture());
    materialIdentity[variant]=recordedMaterials[0].hash;meshIdentity[variant]=recordedMeshes[0].hash;
    SurfaceIdentityHash digest;
    geometryIdentity[variant]=winx_remix::ResourceIdentitySource::GeometryExpanded(recordedMeshes[0].vertices,3,digest);
    Check(textureIdentity[variant]&&materialIdentity[variant]&&geometryIdentity[variant]&&
      meshIdentity[variant]==ExpectedMesh(recordedMeshes[0].vertices,3,materialIdentity[variant]),
      "sampled texture content reaches actual material and bound-mesh API identities");
  }
  Check(textureIdentity[0]!=textureIdentity[1]&&materialIdentity[0]!=materialIdentity[1]&&meshIdentity[0]!=meshIdentity[1]&&
    geometryIdentity[0]==geometryIdentity[1],
    "changed actual texture pixel changes texture material and bound-mesh hashes while pure geometry identity stays equal");
}
static void ContentAndMaterialBinding() {
  Fixture fixture;Attach(fixture);auto vertices=Triangle();
  const auto firstMaterial=Material(0x1234567890abcdefull),secondMaterial=Material(0x1122334455667788ull);
  Check(firstMaterial&&secondMaterial&&firstMaterial!=secondMaterial,"different stable material IDs own different opaque handles");
  const auto firstMesh=SurfaceRigidGeometryResourceStable(vertices,firstMaterial);
  const auto secondMesh=SurfaceRigidGeometryResourceStable(vertices,secondMaterial);
  Check(firstMesh&&secondMesh&&firstMesh!=secondMesh&&recordedMeshCount==2&&recordedMeshes[0].hash!=recordedMeshes[1].hash,
    "same geometry and different stable materials cannot alias one API resource");
  Check(recordedMeshes[0].material==firstMaterial&&recordedMeshes[1].material==secondMaterial&&
    liveMeshes.at(firstMesh)==firstMaterial&&liveMeshes.at(secondMesh)==secondMaterial,
    "each stable bound mesh retains its own requested live material");
  Check(recordedMeshes[0].hash==ExpectedMesh(vertices.data(),vertices.size(),recordedMaterials[0].hash)&&
    recordedMeshes[1].hash==ExpectedMesh(vertices.data(),vertices.size(),recordedMaterials[1].hash),
    "actual bound mesh API hashes match independent one-shot XXH3 byte references");
  auto padded=vertices;
  for(size_t index=0;index<padded.size();++index) {
    memset(&padded[index],0xa5,sizeof(padded[index]));
    memcpy(padded[index].position,vertices[index].position,12);memcpy(padded[index].normal,vertices[index].normal,12);
    memcpy(padded[index].texcoord,vertices[index].texcoord,8);padded[index].color=vertices[index].color;
  }
  Check(memcmp(padded.data(),vertices.data(),vertices.size()*sizeof(vertices[0]))!=0,
    "padding fixture changes the actual SDK struct image");
  Check(SurfaceRigidGeometryResourceStable(padded,firstMaterial)==firstMesh&&recordedMeshCount==2,
    "SDK padding does not change content identity or create another API mesh");
  uint64_t previous[6]={recordedMeshes[0].hash,recordedMeshes[1].hash,0,0,0,0};
  for(unsigned field=0;field<4;++field) {
    auto changed=vertices;
    if(field==0)changed[0].position[0]=.125f;
    if(field==1)changed[0].normal[1]=.25f;
    if(field==2)changed[0].texcoord[0]=.5f;
    if(field==3)changed[0].color=0xff908070u;
    const auto mesh=SurfaceRigidGeometryResourceStable(changed,firstMaterial);const auto& record=recordedMeshes[recordedMeshCount-1];
    Check(mesh&&mesh!=firstMesh&&record.hash==ExpectedMesh(changed.data(),changed.size(),recordedMaterials[0].hash),
      "position normal UV and color changes each reach distinct content-backed API identities");
    for(unsigned i=0;i<field+2;++i)Check(record.hash!=previous[i],"changed meaningful content does not alias prior tested variants");
    previous[field+2]=record.hash;
  }
}
static void RetirementAndAdmission() {
  Fixture fixture;Attach(fixture);auto vertices=Triangle();const uint64_t identity=0x5566778899aabbccull;
  auto material=Material(identity);auto mesh=SurfaceRigidGeometryResourceStable(vertices,material);
  Check(material&&mesh&&recordedMeshCount==1,"initial identity owns one mesh before retirement");
  const auto originalMaterial=material;const auto originalMesh=mesh;const auto originalHash=recordedMeshes[0].hash;
  RetireSurfaceResources(true);const auto before=meshCalls;
  Check(!SurfaceRigidGeometryResourceStable(vertices,originalMaterial)&&meshCalls==before&&liveMeshes.empty()&&liveMaterials.empty(),
    "retired material handle has no permission to recreate a stable mesh");
  nextHandle+=0x100;material=Material(identity);mesh=SurfaceRigidGeometryResourceStable(vertices,material);
  Check(material&&mesh&&material!=originalMaterial&&mesh!=originalMesh&&recordedMeshCount==2&&
    recordedMeshes[1].hash==originalHash&&recordedMeshes[1].material==material,
    "retire and recreate retain API content hash while binding new live opaque handles");
  const auto calls=meshCalls;
  Check(!SurfaceRigidGeometryResourceStable(vertices,reinterpret_cast<remixapi_MaterialHandle>(uintptr_t(0xdeadcafe)))&&meshCalls==calls,
    "unknown material cannot gain identity or invoke mesh Create");
  surfaceMaterials.at(identity).usable=false;
  Check(!SurfaceRigidGeometryResourceStable(vertices,material)&&meshCalls==calls,
    "quarantined material rejects even an already cached mesh before API");
  surfaceMaterials.at(identity).usable=true;
  auto& cached=surfaceMeshes.at(originalHash);cached.usable=false;
  Check(!SurfaceRigidGeometryResourceStable(vertices,material)&&meshCalls==calls,
    "occupied quarantined mesh key cannot be recreated or reused");
  cached.usable=true;
  const auto other=Material(identity+1);cached.material=other;
  Check(!SurfaceRigidGeometryResourceStable(vertices,material)&&meshCalls==calls,
    "cache material mismatch fails closed without another API Create");
  cached.material=material;
  Check(SurfaceRigidGeometryResourceStable(vertices,material)==mesh&&meshCalls==calls,
    "restored current material dependency resolves only the original owned mesh");
}
static void LegacySurfaceIdentity() {
  auto vertices=Triangle();
  uint64_t priorHash=0;remixapi_MaterialHandle priorMaterial=nullptr;
  for(unsigned restart=0;restart<2;++restart) {
    Fixture fixture;Attach(fixture);nextHandle=0x50000+restart*0x3000;
    const auto material=Material(0x7366726967696401ull);
    const auto legacy=SurfaceGeometryResource(vertices,material);
    const auto ordinary=SurfaceRigidGeometryResourceStable(vertices,material);
    Check(legacy&&legacy==ordinary&&recordedMeshCount==1&&
      recordedMeshes[0].hash==ExpectedMesh(vertices.data(),vertices.size(),recordedMaterials[0].hash),
      "legacy and ordinary rigid paths bind the same content ID to one current API mesh");
    if(restart)Check(material!=priorMaterial&&recordedMeshes[0].hash==priorHash,
      "legacy rigid ID survives different material handles across resource lifetimes");
    priorHash=recordedMeshes[0].hash;priorMaterial=material;
    auto padded=vertices;
    for(size_t index=0;index<padded.size();++index) {
      memset(&padded[index],0x5a,sizeof(padded[index]));
      memcpy(padded[index].position,vertices[index].position,12);
      memcpy(padded[index].normal,vertices[index].normal,12);
      memcpy(padded[index].texcoord,vertices[index].texcoord,8);
      padded[index].color=vertices[index].color;
    }
    Check(memcmp(padded.data(),vertices.data(),vertices.size()*sizeof(vertices[0]))!=0&&
      SurfaceGeometryResource(padded,material)==legacy&&recordedMeshCount==1,
      "legacy rigid cache ignores changed SDK padding");
    auto changed=vertices;changed[0].texcoord[0]=.25f;
    const auto altered=SurfaceGeometryResource(changed,material);
    Check(altered&&altered!=legacy&&recordedMeshCount==2&&recordedMeshes[1].hash!=priorHash,
      "legacy rigid UV content change creates a distinct asset");
    const auto other=Material(0x7366726967696402ull);
    Check(SurfaceGeometryResource(vertices,other)&&recordedMeshCount==3&&
      recordedMeshes[2].hash!=priorHash&&recordedMeshes[2].material==other,
      "legacy rigid material content change preserves its separate live binding");
    const auto calls=meshCalls;
    Check(!SurfaceGeometryResource(vertices,reinterpret_cast<remixapi_MaterialHandle>(uintptr_t(0xdeadcafe)))&&meshCalls==calls,
      "legacy rigid entry rejects unknown material before API Create");
    auto malformed=vertices;malformed.pop_back();
    Check(!SurfaceGeometryResource(malformed,material)&&meshCalls==calls,
      "legacy rigid entry rejects incomplete triangle payload before API Create");
    surfaceMaterials.at(recordedMaterials[0].hash).usable=false;
    Check(!SurfaceGeometryResource(vertices,material)&&meshCalls==calls,
      "legacy rigid cache cannot return geometry for a quarantined material owner");
    surfaceMaterials.at(recordedMaterials[0].hash).usable=true;
    RetireSurfaceResources(true);
    Check(!SurfaceGeometryResource(vertices,material)&&meshCalls==calls&&liveMeshes.empty()&&liveMaterials.empty(),
      "legacy rigid retired handles cannot recreate resources or escape teardown");
  }
}
static std::vector<remixapi_HardcodedVertex>* reentryVertices;
static remixapi_MaterialHandle reentryMaterial;
static void FailureAndReentry() {
  {Fixture fixture;Attach(fixture);auto vertices=Triangle();const auto material=Material(0x2001);const auto before=meshCalls;
    for(int allocation=0;allocation<2;++allocation) {
      failAllocationAfter=allocation;const auto mesh=SurfaceRigidGeometryResourceStable(vertices,material);failAllocationAfter=-1;
      Check(!mesh&&meshCalls==before&&surfaceMeshes.empty()&&!surfaceMeshBytes,
        "index and ownership-node allocation failures precede API and leave no mesh reservation");
    }
    failMesh=true;Check(!SurfaceRigidGeometryResourceStable(vertices,material)&&surfaceMeshes.empty()&&!surfaceMeshBytes,
      "ordinary mesh API failure leaves no usable identity entry or retained bytes");
    failMesh=false;const auto mesh=SurfaceRigidGeometryResourceStable(vertices,material);
    Check(mesh&&meshCalls==before+2,"new stable helper can retry only after a failed Create released its empty owner");}
  {Fixture fixture;Attach(fixture);auto vertices=Triangle();reentryVertices=&vertices;reentryMaterial=Material(0x2002);
    onApi=[](char operation,unsigned){if(operation!='m')return;const auto calls=meshCalls;
      Check(!SurfaceRigidGeometryResourceStable(*reentryVertices,reentryMaterial),"nested same-key stable helper rejects pending owner");
      auto different=*reentryVertices;different[0].position[0]=.5f;
      Check(!SurfaceRigidGeometryResourceStable(different,reentryMaterial)&&meshCalls==calls,
        "nested different-key stable helper also rejects without another API Create");};
    Check(SurfaceRigidGeometryResourceStable(vertices,reentryMaterial)&&meshCalls==1,
      "resource callback reentry preserves exactly one accepted outer mesh");}
  for(unsigned mutation=0;mutation<2;++mutation) {
    Fixture fixture;Attach(fixture);auto vertices=Triangle();const auto material=Material(0x2010+mutation);
    if(!mutation)onApi=[](char operation,unsigned){if(operation=='m')RetireSurfaceResources(true);};
    else onApi=[](char operation,unsigned){if(operation=='m')++frameId;};
    Check(!SurfaceRigidGeometryResourceStable(vertices,material)&&meshCalls==1&&liveMeshes.empty()&&surfaceMeshes.empty()&&!surfaceMeshBytes,
      "retirement or frame transition inside mesh Create cancels output and releases reservation");
    if(!mutation)Check(liveMaterials.empty(),"deferred retirement destroys mesh before its material");
  }
  {Fixture fixture;Attach(fixture);auto vertices=Triangle();const auto material=Material(0x2020);
    onApi=[](char operation,unsigned){if(operation=='m')failAllocationAfter=0;};
    const auto mesh=SurfaceRigidGeometryResourceStable(vertices,material);const auto armed=failAllocationAfter;failAllocationAfter=-1;
    Check(mesh&&armed==0&&meshCalls==1,"accepted API output needs no subsequent allocation to retain its owned identity");}
}
static void Run() {
  MaterialCompatibility();DifferentProcessLocalIdentities();ChangedTextureContent();ContentAndMaterialBinding();RetirementAndAdmission();LegacySurfaceIdentity();FailureAndReentry();
  Check(liveMeshes.empty()&&liveMaterials.empty(),"all identity fixture resources retired in dependency order");
}
} // namespace identity_adapter_test
int main() {
  SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
  try {direct_test::Watchdog watchdog;identity_adapter_test::Run();
    printf("{\"status\":\"PASS\",\"checks\":%u,\"gpu\":false,\"nativeGameCodeExecuted\":false,\"ownedComCalls\":%u}\n",
      direct_test::checks,direct_test::comCalls);return 0;
  }catch(const std::exception& error){fprintf(stderr,"FAIL %s\n",error.what());return 1;}
}
