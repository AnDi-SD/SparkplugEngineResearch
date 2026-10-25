// Own CPU adapter fixture. Reuse literal ABI/resource records and recording
// interfaces; no original game instructions, system D3D, GPU or API run.
#define WINX_INDEPENDENT_SUBMIT_FIXTURE_ONLY
#include "../scene/test_independent_submit.cpp"

namespace rigid_capture_test {
namespace source = independent_scene_source;
namespace mesh = native_mesh_source;
namespace transport = native_transport_source;
using Builder = winx_remix::RigidGeometryPacketBuilder;
using direct_test::Check;
using direct_test::Fixture;
using direct_test::Ptr;

struct Effects {
  unsigned com = direct_test::comCalls;
  unsigned materials = direct_test::materialCalls, meshes = direct_test::meshCalls, draws = direct_test::drawCalls;
  unsigned cameras = direct_test::cameraCalls, lights = direct_test::lightCreates;
  unsigned meshCaptures = mesh::captures, meshSubmissions = mesh::submissions;
  unsigned supportCalls = native_owner_source::supportCalls, modelCalls = native_owner_source::modelCalls;
  unsigned updateBegun = native_update_source::begun, updateCompleted = native_update_source::completed;
  unsigned rootCalls = native_update_source::roots;
  void Unchanged() const {
    Check(com == direct_test::comCalls, "geometry capture performs no COM calls with pure current predicate");
    Check(materials == direct_test::materialCalls && meshes == direct_test::meshCalls && draws == direct_test::drawCalls &&
      cameras == direct_test::cameraCalls && lights == direct_test::lightCreates,
      "geometry capture creates or submits no API resource camera light or instance");
    Check(meshCaptures == mesh::captures && meshSubmissions == mesh::submissions &&
      supportCalls == native_owner_source::supportCalls && modelCalls == native_owner_source::modelCalls &&
      updateBegun == native_update_source::begun && updateCompleted == native_update_source::completed &&
      rootCalls == native_update_source::roots, "geometry capture invokes no native producer hook");
  }
};
static source::Input Input(Fixture& fixture, unsigned model = 0) {
  return source::inputs.at({fixture.Support(), Ptr(&fixture.models[model])});
}
static bool Empty(const Builder::Packet& packet) {
  return !packet.valid && packet.vertices.empty() && !packet.provenance.scene && !packet.provenance.scope &&
    packet.provenance.vertexCoverage.empty() && packet.provenance.indexCoverage.empty() &&
    !packet.material.recipe.valid && !packet.material.texture.valid && !packet.material.recipe.material;
}
static bool Current() { return true; }
static void CompareMaterial(const source::Input& input,const Builder::Packet& packet) {
  const auto& material=packet.material;const auto& recipe=material.recipe;const auto& texture=material.texture;
  Check(recipe.valid&&recipe.renderer==input.owner.renderer&&recipe.material==input.material.material&&
    recipe.pass==input.material.pass&&recipe.layer==input.material.layer&&recipe.textureOwner==input.material.textureOwner&&
    recipe.texture==input.material.texture&&recipe.textureCOM==input.textureCOM&&recipe.effectiveOverride==input.effectiveOverride,
    "packet owns native material pass layer holder texture and override provenance");
  Check(!memcmp(recipe.rawTexture.data(),input.material.raw,sizeof(input.material.raw))&&
    !memcmp(recipe.rawMaterial.data(),input.materialStates,sizeof(input.materialStates))&&
    recipe.mappedStates==input.mappedStates&&recipe.mappedPresent==input.mappedPresent,
    "packet owns raw texture material mapped states and emitted state presence exactly");
  Check(recipe.sampler[1]==input.material.sampler.u&&recipe.sampler[2]==input.material.sampler.v&&
    recipe.sampler[5]==input.material.sampler.mag&&recipe.sampler[6]==input.material.sampler.min&&
    recipe.sampler[7]==input.material.sampler.mip,
    "owned recipe sampler retains every consumed native sampler word");
  Check(texture.valid&&texture.device==reinterpret_cast<uintptr_t>(input.textureWitness.device)&&
    texture.texture==reinterpret_cast<uintptr_t>(input.textureWitness.texture)&&texture.generation==input.textureWitness.generation&&
    texture.contentGeneration==input.textureWitness.contentGeneration&&texture.width==input.textureWitness.width&&
    texture.height==input.textureWitness.height&&texture.levels==input.textureWitness.levels&&texture.format==uint32_t(input.textureWitness.format),
    "packet texture metadata distinguishes numeric identity lifecycle content and copied layout");
  const auto& equation=material.equation;const auto& contract=input.material.contract;
  Check(equation.rgb.operation==contract.rgb.operation&&equation.rgb.first==contract.rgb.first&&equation.rgb.second==contract.rgb.second&&
    equation.alpha.operation==contract.alpha.operation&&equation.alpha.first==contract.alpha.first&&equation.alpha.second==contract.alpha.second&&
    equation.factor==contract.factor&&equation.coordinates==contract.coordinates&&equation.transformFlags==contract.transformFlags&&
    !memcmp(equation.transform,&contract.transform,sizeof(equation.transform)),
    "owned equation preserves the adapter projection and exact bootstrap transform words");
  const auto& instance=material.instance;const auto& expected=input.instance;
  Check(instance.cull==expected.cull&&instance.test==expected.test&&instance.function==expected.function&&
    instance.reference==expected.reference&&instance.writeMask==expected.writeMask&&instance.blendEnabled==expected.blendEnabled&&
    instance.source==expected.source&&instance.destination==expected.destination&&instance.operation==expected.operation&&
    instance.channels==expected.channels&&material.bootstrap.textureSrgb==input.textureSrgb&&
    material.bootstrap.separateAlpha==input.separateAlpha&&material.bootstrap.stencil==input.stencil,
    "owned instance state and device bootstrap stay independent from raw native material words");
}

// Independent legacy SDK-output instantiation of the pre-extraction converter.
// All borrowed bytes/layout are consumed before returning from this helper.
static void CompareLegacy(const std::unique_lock<std::recursive_mutex>& borrow, Fixture& fixture,
                          const source::Input& input, const Builder::Packet& packet) {
  mesh::TransportWitness witness{}; mesh::Geometry geometry{};
  const auto& stamp = input.resources;
  Check(transport::Witness(borrow, fixture.Device(), stamp.vertexBuffer, stamp.indexBuffer, stamp.declaration, witness) &&
    mesh::ResolveCurrentResources(borrow, fixture.Device(), input.owner.mesh, input.owner.renderer, input.owner.world, witness, geometry),
    "legacy comparator freshly borrows the same current native resources");
  D3DVERTEXELEMENT9 layout[MAXD3DDECLLENGTH + 1]{};
  const auto count = geometry.layout->size();
  Check(count && count <= MAXD3DDECLLENGTH + 1, "legacy comparator declaration bounded");
  memcpy(layout, geometry.layout->data(), count * sizeof(layout[0]));
  const auto channels = material_channels::UnlitInput(preserveUnlitColor);
  SurfaceGeometryInput conversion{&geometry.vertices->data, &geometry.indices->data, layout, UINT(count), geometry.stride, 0, 2,
    geometry.range, input.instance.cull, &input.material.contract, &channels};
  std::vector<remixapi_HardcodedVertex> legacy;
  material_channels::Plan plan{};
  Check(ExpandSurfaceGeometry(conversion, legacy, plan), "legacy SDK vertex converter accepts fixture geometry");
  Check(packet.valid && legacy.size() == packet.vertices.size(), "packet preserves legacy triangle vertex count");
  for (size_t i = 0; i < legacy.size(); ++i) {
    Check(!memcmp(legacy[i].position, packet.vertices[i].position, sizeof(legacy[i].position)), "packet preserves exact legacy position words");
    Check(!memcmp(legacy[i].normal, packet.vertices[i].normal, sizeof(legacy[i].normal)), "packet preserves exact legacy normal words");
    Check(!memcmp(legacy[i].texcoord, packet.vertices[i].texcoord, sizeof(legacy[i].texcoord)), "packet preserves exact legacy UV words");
    Check(legacy[i].color == packet.vertices[i].color, "packet preserves exact legacy packed color");
  }
  Check(!memcmp(plan.albedo.v, packet.plan.albedo, sizeof(plan.albedo.v)) &&
    !memcmp(plan.emission.v, packet.plan.emission, sizeof(plan.emission.v)), "packet preserves exact legacy channel coefficients");
  Check(plan.vertexRGB == packet.plan.vertexRGB && plan.vertexAlpha == packet.plan.vertexAlpha && plan.alpha == packet.plan.alpha,
    "packet preserves legacy vertex channel policy");
}

static void Basic() {
  for (unsigned policy = 0; policy < 2; ++policy) for (unsigned selected = 0; selected < 2; ++selected) {
    Fixture fixture; preserveUnlitColor = policy != 0;
    auto& vertices = mesh::buffers.at(&fixture.bufferTokens[0]);
    const float position[] = {-0.0f, 1.25f, -2.0f};
    const DWORD color = 0x7f204080;
    memcpy(vertices.data.data(), position, sizeof(position)); memcpy(vertices.data.data() + 24, &color, sizeof(color));
    surfaceBuffers.at(&fixture.bufferTokens[0]).bytes = vertices.data;
    scene_geometry::Scope scope(Ptr(&fixture.scene), Ptr(&fixture.camera)); fixture.Inputs(scope, selected != 0);
    std::unique_lock<std::recursive_mutex> borrow(guard);
    for (unsigned model = 0; model < 2; ++model) {
      const auto input = Input(fixture, model);
      mesh::buffers.at(&fixture.bufferTokens[0]).verified = false;
      mesh::buffers.at(&fixture.bufferTokens[1]).verified = true;
      const Effects effects;
      unsigned fences = 0;
      const auto packet = source::CaptureRigidGeometry(borrow, fixture.Device(), input, [&] { ++fences; return true; });
      Check(packet.valid && packet.vertices.size() == 3 && fences == 2, "selected and unselected source produce one owned triangle between two current fences");
      const auto& value = packet.provenance;
      Check(value.scene == input.owner.scene && value.camera == input.owner.camera && value.renderer == input.owner.renderer &&
        value.support == input.owner.support && value.object == input.owner.object && value.model == input.owner.model &&
        value.mesh == input.owner.mesh, "packet preserves current native scene and object join values");
      Check(value.scope == scope.serial && value.frame == frameId && value.mutation == input.owner.mutation &&
        value.updateSequence == input.witness.sequence && value.deviceEpoch == input.witness.deviceEpoch,
        "packet preserves operation-scoped update and device boundary values");
      Check(value.modelOrdinal == (model ? 2u : 0u) && value.modelOccurrences == (model ? 1u : 2u) &&
        value.registrations == 1 && value.originallySelected == (selected != 0),
        "model vector multiplicity ordinal and selection provenance stay distinct from registry membership");
      Check(!memcmp(value.world, &input.owner.world, sizeof(value.world)) && value.computedWorld == input.computedWorld,
        "packet owns exact world words and source computation provenance");
      Check(value.cpuVertexGeneration == 17 && value.cpuIndexGeneration == 17 &&
        value.vertexGeneration == input.resources.vertexGeneration && value.indexGeneration == input.resources.indexGeneration &&
        value.declarationGeneration == input.resources.declarationGeneration &&
        value.textureGeneration == input.textureWitness.generation && value.textureContentGeneration == input.textureWitness.contentGeneration,
        "packet records CPU pair transport and texture generations separately");
      Check(!value.cpuVertexPartial && !value.cpuIndexPartial && value.vertexCoverage.empty() && value.indexCoverage.empty(),
        "full captures do not invent interval provenance");
      Check(packet.plan.albedo[0] == 1 && packet.plan.albedo[1] == 1 && packet.plan.albedo[2] == 1 &&
        packet.plan.emission[0] == float(policy) && packet.plan.emission[1] == float(policy) && packet.plan.emission[2] == float(policy),
        "existing unlit policy reaches packet material channel coefficients");
      CompareMaterial(input,packet);
      CompareLegacy(borrow, fixture, input, packet);
      Check(!mesh::buffers.at(&fixture.bufferTokens[0]).verified && mesh::buffers.at(&fixture.bufferTokens[1]).verified,
        "capture and legacy comparison never grant or clear CPU upload verification");
      effects.Unchanged();
    }
  }
  preserveUnlitColor = false;
}

static void MakePartial(Fixture& fixture) {
  auto& vertex = mesh::buffers.at(&fixture.bufferTokens[0]);
  auto& index = mesh::buffers.at(&fixture.bufferTokens[1]);
  auto oldVertices = vertex.data, oldIndices = index.data;
  vertex.data.assign(180, 0x5a); index.data.assign(10, 0xa5);
  memcpy(vertex.data.data() + 36, oldVertices.data(), oldVertices.size());
  memcpy(index.data.data() + 2, oldIndices.data(), oldIndices.size());
  vertex.partial = index.partial = true; vertex.verified = index.verified = false;
  vertex.ranges = {{36, 144}}; index.ranges = {{2, 8}};
  mesh::retainedBytes = vertex.data.size() + index.data.size();
  fixture.mesh.vertexBegin = 1; fixture.mesh.indexBegin = 1;
  fixture.vb.byteSize = UINT(vertex.data.size()); fixture.ib.byteSize = UINT(index.data.size());
  surfaceBuffers.at(&fixture.bufferTokens[0]).bytes = vertex.data;
  surfaceBuffers.at(&fixture.bufferTokens[1]).bytes = index.data;
  Check(transport::Remember(fixture.Device(), &fixture.bufferTokens[0], transport::Kind::Vertex, fixture.vb.byteSize, D3DFMT_UNKNOWN) &&
    transport::Remember(fixture.Device(), &fixture.bufferTokens[1], transport::Kind::Index, fixture.ib.byteSize, D3DFMT_INDEX16),
    "partial fixture records resized owned D3D transport metadata");
}
static void PartialCoverage() {
  Fixture fixture; MakePartial(fixture);
  scene_geometry::Scope scope(Ptr(&fixture.scene), Ptr(&fixture.camera)); fixture.Inputs(scope);
  const auto input = Input(fixture);
  std::unique_lock<std::recursive_mutex> borrow(guard);
  auto& vertex = mesh::buffers.at(&fixture.bufferTokens[0]);
  auto& index = mesh::buffers.at(&fixture.bufferTokens[1]);
  vertex.verified = index.verified = false;
  const Effects effects;
  const auto packet = source::CaptureRigidGeometry(borrow, fixture.Device(), input, Current);
  Check(packet.valid && packet.provenance.cpuVertexPartial && packet.provenance.cpuIndexPartial &&
    packet.provenance.vertexCoverage == vertex.ranges && packet.provenance.indexCoverage == index.ranges,
    "required windows covered inside larger partially captured buffers retain exact interval provenance");
  CompareLegacy(borrow, fixture, input, packet);
  vertex.ranges = {{36, 72}, {73, 144}};
  Check(Empty(source::CaptureRigidGeometry(borrow, fixture.Device(), input, Current)), "one-byte hole in required vertex window rejects whole packet");
  vertex.ranges = {{36, 144}}; index.ranges = {{2, 4}, {5, 8}};
  Check(Empty(source::CaptureRigidGeometry(borrow, fixture.Device(), input, Current)), "one-byte hole in required index window rejects whole packet");
  Check(!vertex.verified && !index.verified, "partial captures and rejected holes never mutate CPU verified flags");
  effects.Unchanged();
}

static void Retire(Fixture& fixture) {
  mesh::Forget(&fixture.bufferTokens[0]); mesh::Forget(&fixture.bufferTokens[1]);
  mesh::layouts.clear(); transport::RetireDevice(fixture.Device());
}
static void OwnedRetirement() {
  Fixture fixture;
  scene_geometry::Scope scope(Ptr(&fixture.scene), Ptr(&fixture.camera)); fixture.Inputs(scope);
  auto input = Input(fixture);input.textureSrgb=1;input.instance.reference=17;
  std::unique_lock<std::recursive_mutex> borrow(guard);
  const Effects effects;
  const auto packet = source::CaptureRigidGeometry(borrow, fixture.Device(), input, Current);
  Check(packet.valid && packet.vertices[1].position[0] == 1, "retirement fixture first obtains expected owned geometry");
  CompareMaterial(input,packet);
  mesh::buffers.at(&fixture.bufferTokens[0]).data.assign(108, 0);
  mesh::buffers.at(&fixture.bufferTokens[1]).data.assign(6, 0);
  for (auto& layout : mesh::layouts) layout.second.clear();
  fixture.world._41 = 88; memset(fixture.object + 0x8c, 0, 64);
  fixture.material.base.renderStates[8]=0;fixture.pass.layers[0]=0;fixture.texture.textureStates[1]=0;
  fixture.nativeTexture.texture=0;input={};
  Retire(fixture);
  Check(mesh::buffers.empty() && mesh::layouts.empty() && transport::records.empty(), "fixture retires all resource borrows and declaration cache");
  Check(packet.valid && packet.vertices.size() == 3 && packet.vertices[1].position[0] == 1 &&
    packet.vertices[2].position[1] == 1 && packet.vertices[0].normal[2] == 1 && packet.vertices[0].color == 0xffffffff &&
    packet.plan.albedo[0] == 1 && packet.provenance.world[12] == 12 && packet.provenance.cpuVertexGeneration == 17,
    "owned geometry plan world and generation stamps survive source mutation and retirement");
  Check(packet.material.recipe.rawMaterial[8]==2&&packet.material.recipe.rawTexture[1]==3&&
    packet.material.recipe.pass==Ptr(&fixture.pass)&&packet.material.recipe.textureCOM==Ptr(fixture.Texture())&&
    packet.material.texture.width==1&&packet.material.texture.height==1&&packet.material.texture.levels==1&&
    packet.material.texture.generation==packet.provenance.textureGeneration&&
    packet.material.texture.contentGeneration==packet.provenance.textureContentGeneration&&
    packet.material.instance.reference==17&&packet.material.bootstrap.textureSrgb==1,
    "owned recipe texture layout generations and bootstrap survive native mutation adapter input retirement and resource deletion");
  effects.Unchanged();
}
static void FinalRetirement() {
  Fixture fixture;
  scene_geometry::Scope scope(Ptr(&fixture.scene), Ptr(&fixture.camera)); fixture.Inputs(scope);
  const auto input = Input(fixture);
  std::unique_lock<std::recursive_mutex> borrow(guard);
  const Effects effects;
  unsigned calls = 0;
  const auto packet = source::CaptureRigidGeometry(borrow, fixture.Device(), input, [&] {
    if (++calls == 1) return true;
    Retire(fixture); return false;
  });
  Check(calls == 2 && Empty(packet) && mesh::buffers.empty() && mesh::layouts.empty(),
    "retirement in final current callback invalidates complete packet without a stale borrowed read");
  effects.Unchanged();
}
static void InitialReplacement() {
  Fixture fixture;
  scene_geometry::Scope scope(Ptr(&fixture.scene), Ptr(&fixture.camera)); fixture.Inputs(scope);
  const auto input = Input(fixture);
  auto vertex = mesh::buffers.at(&fixture.bufferTokens[0]);
  auto index = mesh::buffers.at(&fixture.bufferTokens[1]);
  vertex.generation = index.generation = 71;
  const float changed = 4.5f; memcpy(vertex.data.data(), &changed, sizeof(changed));
  std::unique_lock<std::recursive_mutex> borrow(guard);
  const Effects effects;
  unsigned calls = 0;
  const auto packet = source::CaptureRigidGeometry(borrow, fixture.Device(), input, [&] {
    if (++calls == 1) {
      mesh::Forget(&fixture.bufferTokens[0]);mesh::Forget(&fixture.bufferTokens[1]);mesh::layouts.clear();
      for(auto& token:fixture.bufferTokens)transport::Forget(&token);
      mesh::buffers.emplace(&fixture.bufferTokens[0], vertex); mesh::buffers.emplace(&fixture.bufferTokens[1], index);
      mesh::retainedBytes = vertex.data.size() + index.data.size();
      Check(transport::Remember(fixture.Device(), &fixture.bufferTokens[0], transport::Kind::Vertex, fixture.vb.byteSize, D3DFMT_UNKNOWN) &&
        transport::Remember(fixture.Device(), &fixture.bufferTokens[1], transport::Kind::Index, fixture.ib.byteSize, D3DFMT_INDEX16) &&
        transport::Remember(fixture.Device(), &fixture.bufferTokens[2], transport::Kind::Declaration, 0, D3DFMT_UNKNOWN, fixture.layout, 5),
        "first current callback publishes new owned resource records at the same addresses");
    }
    return true;
  });
  Check(packet.valid && calls == 2 && packet.vertices[0].position[0] == changed &&
    packet.provenance.cpuVertexGeneration == 71 && packet.provenance.cpuIndexGeneration == 71 &&
    packet.provenance.vertexGeneration != input.resources.vertexGeneration,
    "resource borrowing occurs inside converter after initial current callback replacement");
  effects.Unchanged();
}
static void GenerationReplacement() {
  Fixture fixture;
  scene_geometry::Scope scope(Ptr(&fixture.scene), Ptr(&fixture.camera)); fixture.Inputs(scope);
  const auto oldInput = Input(fixture);
  std::unique_lock<std::recursive_mutex> borrow(guard);
  const auto oldPacket = source::CaptureRigidGeometry(borrow, fixture.Device(), oldInput, Current);
  Check(oldPacket.valid, "generation fixture captures original resource stamps");
  mesh::buffers.at(&fixture.bufferTokens[0]).generation = mesh::buffers.at(&fixture.bufferTokens[1]).generation = 117;
  mesh::buffers.at(&fixture.bufferTokens[0]).verified = mesh::buffers.at(&fixture.bufferTokens[1]).verified = false;
  Check(transport::Remember(fixture.Device(), &fixture.bufferTokens[0], transport::Kind::Vertex, fixture.vb.byteSize, D3DFMT_UNKNOWN) &&
    transport::Remember(fixture.Device(), &fixture.bufferTokens[1], transport::Kind::Index, fixture.ib.byteSize, D3DFMT_INDEX16) &&
    transport::Remember(fixture.Device(), &fixture.bufferTokens[2], transport::Kind::Declaration, 0, D3DFMT_UNKNOWN, fixture.layout, 5),
    "owned transport fixture replaces all records at unchanged addresses");
  D3DSURFACE_DESC description{}; description.Format = D3DFMT_A8R8G8B8; description.Type = D3DRTYPE_SURFACE;
  description.Pool = D3DPOOL_MANAGED; description.Width = description.Height = 1;
  Check(transport::RememberTexture(fixture.Device(), fixture.Texture(), fixture.Texture(), &description, 1),
    "owned texture fixture replaces texture at unchanged address");
  fixture.Inputs(scope);
  const auto newInput = Input(fixture);
  const Effects effects;
  const auto next = source::CaptureRigidGeometry(borrow, fixture.Device(), newInput, Current);
  Check(next.valid && newInput.resources.vertexBuffer == oldInput.resources.vertexBuffer &&
    newInput.resources.indexBuffer == oldInput.resources.indexBuffer && newInput.resources.declaration == oldInput.resources.declaration &&
    newInput.textureWitness.texture == oldInput.textureWitness.texture, "fresh source observes unchanged numeric resource addresses");
  Check(next.provenance.cpuVertexGeneration == 117 && next.provenance.cpuIndexGeneration == 117 &&
    next.provenance.vertexGeneration != oldPacket.provenance.vertexGeneration &&
    next.provenance.indexGeneration != oldPacket.provenance.indexGeneration &&
    next.provenance.declarationGeneration != oldPacket.provenance.declarationGeneration &&
    next.provenance.textureGeneration != oldPacket.provenance.textureGeneration &&
    next.provenance.textureContentGeneration != oldPacket.provenance.textureContentGeneration,
    "fresh source packet preserves every new CPU transport and texture generation independently");
  Check(oldPacket.provenance.cpuVertexGeneration == 17 && oldPacket.provenance.textureGeneration == oldInput.textureWitness.generation,
    "resource replacement cannot rewrite prior owned packet generation stamps");
  CompareMaterial(newInput,next);
  Check(next.material.texture.generation!=oldPacket.material.texture.generation&&
    next.material.texture.contentGeneration!=oldPacket.material.texture.contentGeneration&&
    oldPacket.material.texture.generation==oldInput.textureWitness.generation&&
    oldPacket.material.texture.contentGeneration==oldInput.textureWitness.contentGeneration,
    "same-address texture replacement changes fresh owned material stamps without rewriting an older packet");
  effects.Unchanged();
}
static void MaterialAndTextureFences() {
  for(unsigned reason=0;reason<7;++reason) {
    Fixture fixture;scene_geometry::Scope scope(Ptr(&fixture.scene),Ptr(&fixture.camera));fixture.Inputs(scope);
    const auto input=Input(fixture);std::unique_lock<std::recursive_mutex> borrow(guard);const Effects effects;
    const auto current=[&](){return source::NativeInputCurrent(borrow,fixture.Device(),input);};
    const auto packet=source::CaptureRigidGeometry(borrow,fixture.Device(),input,current);
    Check(packet.valid,"native current predicate qualifies ordinary owned geometry material and texture before mutation");
    direct_test::ChangePacketSource(fixture,reason);
    Check(Empty(source::CaptureRigidGeometry(borrow,fixture.Device(),input,current)),
      "native current capture rejects material pass writable retired reused reset or NULL texture transitions");
    Check(packet.valid&&packet.material.recipe.pass==input.material.pass&&
      packet.material.recipe.rawMaterial[8]==2&&packet.material.texture.generation==input.textureWitness.generation&&
      packet.material.texture.contentGeneration==input.textureWitness.contentGeneration,
      "native source transition cannot mutate an already captured owned packet");
    effects.Unchanged();
  }
  {Fixture fixture;scene_geometry::Scope scope(Ptr(&fixture.scene),Ptr(&fixture.camera));fixture.Inputs(scope);
    const auto input=Input(fixture);std::unique_lock<std::recursive_mutex> borrow(guard);const Effects effects;
    unsigned calls=0;
    const auto packet=source::CaptureRigidGeometry(borrow,fixture.Device(),input,[&](){
      if(++calls==2)direct_test::ChangePacketSource(fixture,1);
      return source::NativeInputCurrent(borrow,fixture.Device(),input);
    });
    Check(calls==2&&Empty(packet),"pass identity replacement after all owned copies rejects the complete packet at final native fence");
    effects.Unchanged();}
}
static void TextureAssetCancellation() {
  Fixture fixture;std::unique_lock<std::recursive_mutex> borrow(guard);
  constexpr wchar_t path[]=L"guarded-material-packet.dds";
  constexpr wchar_t temporary[]=L"guarded-material-packet.dds.tmp";
  Check(GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES&&GetFileAttributesW(temporary)==INVALID_FILE_ATTRIBUTES,
    "guarded material asset fixture starts with a fresh unpublished DDS and temporary path");
  transport::TextureWitness witness{};
  Check(transport::BorrowTexture(borrow,fixture.Device(),fixture.Texture(),witness),
    "DDS cancellation fixture obtains an observed managed texture content witness");
  const auto original=witness;const auto before=material_channels::assetBytes;
  const auto current=[&](){return transport::CurrentTexture(borrow,witness);};
  const material_channels::RGB unitTint{{1,1,1}};
  direct_test::onTextureRead=[](){direct_test::onTextureRead=nullptr;
    direct_test::current->comTexture.pixel=0xff2468ac;direct_test::ChangedContent();};
  Check(!material_channels::WriteTextureSourceCurrent(fixture.Texture(),path,unitTint,current),
    "content mutation during DDS readonly mip acquisition cancels publication under the old content witness");
  Check(GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES&&GetFileAttributesW(temporary)==INVALID_FILE_ATTRIBUTES&&
    material_channels::assetBytes==before,
    "cancelled DDS owns no final file temporary file or accepted asset byte accounting");
  Check(!transport::CurrentTexture(borrow,original)&&transport::BorrowTexture(borrow,fixture.Device(),fixture.Texture(),witness)&&
    witness.generation==original.generation&&witness.contentGeneration!=original.contentGeneration,
    "DDS retry reacquires the same resource with a distinct current content generation");
  Check(material_channels::WriteTextureSourceCurrent(fixture.Texture(),path,unitTint,current),
    "fresh DDS retry publishes coherent bytes after content mutation");
  Check(GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES&&GetFileAttributesW(temporary)==INVALID_FILE_ATTRIBUTES&&
    material_channels::assetBytes==before+33*sizeof(uint32_t),
    "only one completed one-pixel DDS contributes its header and pixel to asset byte accounting");
  FILE* file=nullptr;Check(!_wfopen_s(&file,path,L"rb")&&file,"coherent DDS remains available as fixture evidence");
  uint32_t words[33]{};const bool read=fread(words,sizeof(words[0]),33,file)==33;
  const bool complete=fgetc(file)==EOF;const bool closed=fclose(file)==0;
  Check(read&&complete&&closed&&words[0]==0x20534444&&words[3]==1&&words[4]==1&&words[7]==1&&
    words[32]==fixture.comTexture.pixel&&words[32]==0xff2468ac,
    "saved DDS contains exactly the current pixel with its original alpha and a coherent one-level header");
  Check(!direct_test::materialCalls&&!direct_test::meshCalls&&!direct_test::drawCalls,
    "DDS cancellation and retry require no Remix API resource or instance creation");
}
using MipSource=material_channels::TextureMipSource;
static bool EmptyMip(const MipSource::Snapshot& snapshot) {
  return !snapshot.valid&&snapshot.mips.empty()&&!snapshot.description.width&&!snapshot.description.height&&
    !snapshot.description.format&&!snapshot.description.levels;
}
// Independent padded row fixture. Padding and XRGB high bytes intentionally
// differ from both the packed hash input and normalized DDS output.
struct MipData {
  uint32_t rows[6]={0x00102030,0x7f405060,0xdeadbeef,0x80402010,0x004080c0,0xcafebabe};
  uint32_t tail[2]={0x11224488,0xabcdef01};
  unsigned failure=0,lockedLevel=0;
};
static MipData* mipData;
static DWORD STDMETHODCALLTYPE PaddedLevels(direct_test::FakeTexture* texture) {
  ++direct_test::comCalls;direct_test::TextureAlive(texture);return mipData->failure==8?1:2;
}
static HRESULT STDMETHODCALLTYPE PaddedDesc(direct_test::FakeTexture* texture,UINT level,D3DSURFACE_DESC* out) {
  ++direct_test::comCalls;direct_test::TextureAlive(texture);
  if(!out||level>1||(mipData->failure==1&&!level)||(mipData->failure==2&&level==1))return D3DERR_INVALIDCALL;
  *out={};out->Format=D3DFMT_X8R8G8B8;out->Type=D3DRTYPE_SURFACE;out->Pool=D3DPOOL_MANAGED;
  out->Width=level?1:2;out->Height=out->Width;
  if(mipData->failure==8){out->Width=1;out->Height=2048;}
  if(mipData->failure==9&&level==1)out->Width=2;
  return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE PaddedLock(direct_test::FakeTexture* texture,UINT level,D3DLOCKED_RECT* out,const RECT*,DWORD flags) {
  ++direct_test::comCalls;direct_test::TextureAlive(texture);++direct_test::textureReadCalls;
  Check(level<2&&flags==D3DLOCK_READONLY&&!texture->locked,"padded snapshot opens only a matching readonly mip");
  if(mipData->failure==3)return D3DERR_INVALIDCALL;
  Check(texture->refs==2,"snapshot adapter retains its own texture reference throughout every successful mip lock");
  texture->locked=true;++texture->locks;mipData->lockedLevel=level;
  out->pBits=level?mipData->tail:mipData->rows;out->Pitch=level?8:12;
  if(mipData->failure==5)out->pBits=nullptr;
  if(mipData->failure==6)out->Pitch=4;
  if(mipData->failure==7)out->Pitch=-12;
  if(mipData->failure==8)out->Pitch=0x7fffffff;
  transport::TextureLockResult(reinterpret_cast<IDirect3DTexture9*>(texture),level,flags,D3D_OK);
  if(direct_test::onTextureRead)direct_test::onTextureRead();return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE PaddedUnlock(direct_test::FakeTexture* texture,UINT level) {
  ++direct_test::comCalls;direct_test::TextureAlive(texture);
  Check(texture->locked&&level==mipData->lockedLevel,"padded snapshot closes precisely its currently open mip");
  texture->locked=false;++texture->unlocks;
  const auto result=mipData->failure==4?D3DERR_INVALIDCALL:D3D_OK;
  transport::TextureUnlockResult(reinterpret_cast<IDirect3DTexture9*>(texture),level,result);
  if(direct_test::onTextureUnlock)direct_test::onTextureUnlock();return result;
}
struct MipFixture:Fixture {
  MipData data;
  explicit MipFixture(unsigned failure=0) {
    data.failure=failure;mipData=&data;
    textureTable[13]=reinterpret_cast<void*>(PaddedLevels);textureTable[17]=reinterpret_cast<void*>(PaddedDesc);
    textureTable[19]=reinterpret_cast<void*>(PaddedLock);textureTable[20]=reinterpret_cast<void*>(PaddedUnlock);
    D3DSURFACE_DESC layout[2]{};
    for(auto& description:layout) {
      description.Format=D3DFMT_X8R8G8B8;description.Type=D3DRTYPE_SURFACE;description.Pool=D3DPOOL_MANAGED;
    }
    layout[0].Width=failure==8?1:2;layout[0].Height=failure==8?2048:2;
    layout[1].Width=layout[1].Height=1;
    Check(transport::RememberTexture(Device(),Texture(),Texture(),layout,failure==8?1:2),
      "padded mip fixture publishes a separate current two-level texture description");
  }
};
static transport::TextureWitness MipWitness(const std::unique_lock<std::recursive_mutex>& borrow,Fixture& fixture) {
  transport::TextureWitness witness{};
  Check(transport::BorrowTexture(borrow,fixture.Device(),fixture.Texture(),witness),"mip capture starts with a qualified current source witness");
  return witness;
}
static void OwnedTextureMips() {
  MipFixture fixture;std::unique_lock<std::recursive_mutex> borrow(guard);
  const auto witness=MipWitness(borrow,fixture);
  const auto snapshot=material_channels::CaptureTextureMipSnapshotCurrent(fixture.Texture(),[&](){return transport::CurrentTexture(borrow,witness);});
  Check(MipSource::IsValid(snapshot)&&snapshot.description.width==2&&snapshot.description.height==2&&
    snapshot.description.format==D3DFMT_X8R8G8B8&&snapshot.description.levels==2&&snapshot.mips.size()==2,
    "snapshot owns the qualified XRGB description and every mip level");
  Check(snapshot.mips[0].width==2&&snapshot.mips[0].height==2&&snapshot.mips[0].pixels==
    std::vector<uint32_t>({0x00102030,0x7f405060,0x80402010,0x004080c0})&&
    snapshot.mips[1].width==1&&snapshot.mips[1].height==1&&snapshot.mips[1].pixels==std::vector<uint32_t>({0x11224488}),
    "snapshot preserves raw XRGB high bytes and strips twelve-byte and eight-byte row padding");
  Check(fixture.comTexture.refs==1&&fixture.comTexture.addRefs==1&&fixture.comTexture.releases==1&&
    fixture.comTexture.locks==2&&fixture.comTexture.unlocks==2&&!fixture.comTexture.locked,
    "successful two-mip capture releases its temporary owner after closing both readonly locks");
  XXH3_state_t legacy{};XXH3_64bits_reset(&legacy);
  const uint32_t layout[]={2,2,D3DFMT_X8R8G8B8,2};XXH3_64bits_update(&legacy,layout,sizeof(layout));
  XXH3_64bits_update(&legacy,fixture.data.rows,8);XXH3_64bits_update(&legacy,fixture.data.rows+3,8);
  XXH3_64bits_update(&legacy,fixture.data.tail,4);
  const auto hash=XXH3_64bits_digest(&legacy);
  Check(hash&&material_channels::TextureMipSnapshotHash(snapshot)==hash,
    "owned mip hash exactly preserves independent legacy layout followed by tightly packed raw rows");
  memset(fixture.data.rows,0,sizeof(fixture.data.rows));memset(fixture.data.tail,0,sizeof(fixture.data.tail));
  direct_test::ReleaseNativeTexture();
  Check(!fixture.comTexture.alive&&!fixture.comTexture.refs&&fixture.comTexture.destroyed==1&&
    !transport::CurrentTexture(borrow,witness),"fixture retires the native texture after obtaining an independent pixel copy");
  const auto com=direct_test::comCalls;const auto assets=material_channels::assetBytes;
  constexpr wchar_t path[]=L"owned-padded-mip-snapshot.dds";
  constexpr wchar_t temporary[]=L"owned-padded-mip-snapshot.dds.tmp";
  Check(GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES&&GetFileAttributesW(temporary)==INVALID_FILE_ATTRIBUTES,
    "owned mip DDS fixture starts with an absent output and temporary file");
  const material_channels::RGB tint{{0.5f,1.0f,0.25f}};
  Check(material_channels::TextureMipSnapshotHash(snapshot)==hash&&
    material_channels::WriteTextureMipSnapshotCurrent(snapshot,path,tint,Current),
    "hash and tinted DDS consume the same owned words after source mutation and final destruction");
  Check(direct_test::comCalls==com&&material_channels::assetBytes==assets+37*sizeof(uint32_t)&&
    GetFileAttributesW(temporary)==INVALID_FILE_ATTRIBUTES,"owned hash and DDS use no COM and publish exactly one complete asset");
  FILE* file=nullptr;Check(!_wfopen_s(&file,path,L"rb")&&file,"owned padded DDS is available for byte verification");
  uint32_t actual[37]{};const bool read=fread(actual,sizeof(actual[0]),37,file)==37;
  const bool complete=fgetc(file)==EOF;const bool closed=fclose(file)==0;
  uint32_t expected[37]{};expected[0]=0x20534444;expected[1]=124;expected[2]=0x2100f;
  expected[3]=2;expected[4]=2;expected[5]=8;expected[7]=2;expected[19]=32;expected[20]=0x41;
  expected[22]=32;expected[23]=0xff0000;expected[24]=0xff00;expected[25]=0xff;expected[26]=0xff000000;expected[27]=0x401008;
  expected[32]=0xff08200c;expected[33]=0xff205018;expected[34]=0xff202004;expected[35]=0xff208030;expected[36]=0xff114422;
  Check(read&&complete&&closed&&!memcmp(actual,expected,sizeof(expected)),
    "DDS exact header and both tinted mip payloads normalize XRGB alpha only at file output");
  Check(!direct_test::materialCalls&&!direct_test::meshCalls&&!direct_test::drawCalls,
    "owned pixel capture hash and DDS require no Remix material mesh or instance creation");
}
static void TextureMipTransitions() {
  for(unsigned boundary=0;boundary<4;++boundary)for(unsigned change=0;change<2;++change) {
    Fixture fixture;std::unique_lock<std::recursive_mutex> borrow(guard);const auto witness=MipWitness(borrow,fixture);
    static unsigned mutation,hits,at;mutation=change;hits=0;at=boundary;
    const auto mutate=[](){direct_test::onTextureAddRef=direct_test::onTextureRead=
      direct_test::onTextureUnlock=direct_test::onTextureRelease=nullptr;++hits;
      Check(direct_test::current->comTexture.refs==(at==3?1u:2u),"reentrant mutation observes the requested temporary-reference boundary");
      if(mutation){transport::BeginReset(direct_test::current->Device());transport::EndReset(direct_test::current->Device());}
      else direct_test::ChangedContent();};
    if(boundary==0)direct_test::onTextureAddRef=mutate;
    if(boundary==1)direct_test::onTextureRead=mutate;
    if(boundary==2)direct_test::onTextureUnlock=mutate;
    if(boundary==3)direct_test::onTextureRelease=mutate;
    const auto snapshot=material_channels::CaptureTextureMipSnapshotCurrent(fixture.Texture(),[&](){return transport::CurrentTexture(borrow,witness);});
    Check(hits==1&&EmptyMip(snapshot)&&fixture.comTexture.refs==1&&fixture.comTexture.alive&&
      fixture.comTexture.addRefs==1&&fixture.comTexture.releases==1&&fixture.comTexture.locks==fixture.comTexture.unlocks&&
      !fixture.comTexture.locked,"content or Reset at AddRef Lock Unlock or Release rejects the snapshot with balanced ownership");
    if(!boundary)Check(!fixture.comTexture.locks&&direct_test::comCalls>=2,
      "AddRef reentry is fenced before opening any mip");
  }
  {Fixture fixture;std::unique_lock<std::recursive_mutex> borrow(guard);const auto witness=MipWitness(borrow,fixture);
    static unsigned hits;hits=0;
    direct_test::onTextureRead=[](){direct_test::onTextureRead=nullptr;++hits;
      Check(direct_test::current->comTexture.refs==2&&direct_test::current->comTexture.locked,
        "native release callback starts inside a readonly mip with the temporary reference held");
      direct_test::ReleaseNativeTexture();
      Check(direct_test::current->comTexture.refs==1&&direct_test::current->comTexture.alive&&
        transport::textures.count(direct_test::current->Texture())==1,
        "native two-to-one Release preserves both live object and transport record during mip copy");};
    const auto snapshot=material_channels::CaptureTextureMipSnapshotCurrent(fixture.Texture(),[&](){return transport::CurrentTexture(borrow,witness);});
    Check(hits==1&&EmptyMip(snapshot)&&!fixture.comTexture.alive&&!fixture.comTexture.refs&&fixture.comTexture.destroyed==1&&
      fixture.comTexture.addRefs==1&&fixture.comTexture.releases==2&&fixture.comTexture.locks==1&&fixture.comTexture.unlocks==1&&
      !transport::textures.count(fixture.Texture()),
      "final adapter Release destroys only after Unlock and the final current fence rejects without a dead-object method");}
  {Fixture fixture;const auto before=direct_test::comCalls;
    Check(EmptyMip(material_channels::CaptureTextureMipSnapshotCurrent(fixture.Texture(),[]{return false;}))&&
      direct_test::comCalls==before,"initially stale snapshot makes no COM call or temporary acquisition");}
}
static void TextureMipFailures() {
  for(unsigned failure=1;failure<=9;++failure) {
    MipFixture fixture(failure);std::unique_lock<std::recursive_mutex> borrow(guard);const auto witness=MipWitness(borrow,fixture);
    const auto snapshot=material_channels::CaptureTextureMipSnapshotCurrent(fixture.Texture(),[&](){return transport::CurrentTexture(borrow,witness);});
    Check(EmptyMip(snapshot)&&fixture.comTexture.refs==1&&fixture.comTexture.addRefs==1&&fixture.comTexture.releases==1&&
      fixture.comTexture.locks==fixture.comTexture.unlocks&&!fixture.comTexture.locked,
      "descriptor Lock Unlock null rows short negative oversized pitch or later-mip mismatch rejects all copied bytes with balanced cleanup");
    if(failure==8)Check(fixture.comTexture.locks==1&&fixture.comTexture.unlocks==1,
      "unrepresentable positive row span is rejected after Unlock before accessing rows beyond the owned word");
  }
  for(unsigned boundary=0;boundary<2;++boundary) {
    Fixture fixture;bool threw=false;
    try {material_channels::CaptureTextureMipSnapshotCurrent(fixture.Texture(),[&](){
      if((!boundary&&fixture.comTexture.locked)||(boundary&&fixture.comTexture.addRefs&&fixture.comTexture.releases))
        throw std::runtime_error("owned snapshot current exception");return true;
    });}catch(const std::runtime_error&){threw=true;}
    Check(threw&&fixture.comTexture.refs==1&&fixture.comTexture.addRefs==1&&fixture.comTexture.releases==1&&
      fixture.comTexture.locks==1&&fixture.comTexture.unlocks==1&&!fixture.comTexture.locked,
      "throwing current predicate while locked or after final temporary Release propagates with exactly balanced cleanup");
  }
  for(int allocation=0;allocation<3;++allocation) {
    MipFixture fixture;bool threw=false;failAllocationAfter=allocation;
    try {material_channels::CaptureTextureMipSnapshotCurrent(fixture.Texture(),Current);}
    catch(const std::bad_alloc&){threw=true;}
    failAllocationAfter=-1;
    Check(threw&&fixture.comTexture.refs==1&&fixture.comTexture.addRefs==1&&fixture.comTexture.releases==1&&
      fixture.comTexture.locks==fixture.comTexture.unlocks&&!fixture.comTexture.locked,
      "each mip container and pixel allocation failure unwinds the temporary reference and every prior lock");
    Check(material_channels::CaptureTextureMipSnapshotCurrent(fixture.Texture(),Current).valid,
      "allocation failure leaves a fresh independent two-mip capture usable");
  }
}
static void TextureMipHashCache() {
  MipFixture fixture;MipSource::Snapshot snapshot;
  const MipSource::Description expected{2,2,D3DFMT_X8R8G8B8,2};
  const auto hash=ChannelTextureHashCurrent(fixture.Texture(),Current,&snapshot,&expected);
  Check(hash&&MipSource::IsValid(snapshot)&&surfaceChannelTextureHashes.at(fixture.Texture())==hash,
    "cold channel lookup captures one owned description and publishes its same-content hash");
  const auto before=direct_test::comCalls;const auto reads=direct_test::textureReadCalls;
  Check(ChannelTextureHashCurrent(fixture.Texture(),Current,&snapshot,&expected)==hash&&EmptyMip(snapshot)&&
    direct_test::comCalls==before&&direct_test::textureReadCalls==reads,
    "warm channel cache returns identity without allocating or copying another pixel snapshot");
  InvalidateSurfaceTexture(fixture.Texture());surfaceTextureHashes[fixture.Texture()]=0x1234;
  const MipSource::Description different{1,2,D3DFMT_X8R8G8B8,2};
  Check(!ChannelTextureHashCurrent(fixture.Texture(),Current,&snapshot,&different)&&EmptyMip(snapshot)&&
    !surfaceChannelTextureHashes.count(fixture.Texture())&&!surfaceTextureHashes.count(fixture.Texture()),
    "captured description mismatch refuses the snapshot and clears both channel and legacy pointer hashes");
}
static void AllocationFailure() {
  Fixture fixture;
  scene_geometry::Scope scope(Ptr(&fixture.scene), Ptr(&fixture.camera)); fixture.Inputs(scope);
  const auto input = Input(fixture);
  std::unique_lock<std::recursive_mutex> borrow(guard);
  const Effects effects;
  bool propagated = false; failAllocationAfter = 0;
  try { source::CaptureRigidGeometry(borrow, fixture.Device(), input, Current); }
  catch (const std::bad_alloc&) { propagated = true; }
  failAllocationAfter = -1;
  Check(propagated, "converter allocation failure propagates through capture boundary");
  Check(source::CaptureRigidGeometry(borrow, fixture.Device(), input, Current).valid,
    "allocation failure leaves later independent capture usable");
  effects.Unchanged();
}
static void Run() {
  Basic(); PartialCoverage(); OwnedRetirement(); FinalRetirement(); InitialReplacement(); GenerationReplacement(); MaterialAndTextureFences(); TextureAssetCancellation();
  OwnedTextureMips(); TextureMipTransitions(); TextureMipFailures(); TextureMipHashCache(); AllocationFailure();
  Check(direct_test::liveMeshes.empty() && direct_test::liveMaterials.empty() && direct_test::liveLights.empty(),
    "geometry-only fixture leaves no recording API resources");
}
} // namespace rigid_capture_test

int main() {
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
  try {
    direct_test::Watchdog watchdog;
    rigid_capture_test::Run();
    printf("{\"status\":\"PASS\",\"checks\":%u,\"gpu\":false,\"nativeGameCodeExecuted\":false,\"scope\":\"owned rigid packet adapter capture\"}\n", direct_test::checks);
    return 0;
  } catch (const std::exception& error) { fprintf(stderr, "FAIL %s\n", error.what()); return 1; }
}
