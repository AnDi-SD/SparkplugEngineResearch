// Own bounded adapter contract tests. Literal PC ABI buffers, recording API;
// no game instructions are run. GPU/bridge behavior is checked separately live.
#define WINX_REMIX_TEST
#include "../../src/runtime/winx_d3d9_probe.cpp"
#include <cstdlib>

static unsigned checks,created,destroyed,drawn;
static bool failCreate;
static std::set<remixapi_LightHandle> handles;
static std::vector<scene_audit::ConvertedLight> submitted;
static void Check(bool value,const char* message) {
  ++checks;if(!value) {fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
#include "test_scene_read_window.h"
static remixapi_ErrorCode REMIXAPI_CALL TestCreate(const remixapi_LightInfo* info,remixapi_LightHandle* out) {
  if(failCreate) return REMIXAPI_ERROR_CODE_GENERAL_FAILURE;
  scene_audit::ConvertedLight state{};state.radiance=info->radiance;
  const auto type=*static_cast<const remixapi_StructType*>(info->pNext);
  if(type==REMIXAPI_STRUCT_TYPE_LIGHT_INFO_DISTANT_EXT) {
    const auto p=static_cast<remixapi_LightInfoDistantEXT*>(info->pNext);state.type=0;state.direction=p->direction;
  } else {
    Check(type==REMIXAPI_STRUCT_TYPE_LIGHT_INFO_SPHERE_EXT,"sphere extension");
    const auto p=static_cast<remixapi_LightInfoSphereEXT*>(info->pNext);
    state.type=p->shaping_hasvalue?2:1;state.position=p->position;state.radius=p->radius;
    state.direction=p->shaping_value.direction;state.angle=p->shaping_value.coneAngleDegrees;
  }
  // Model x86 bridge: every create has a NEW client handle, even for same hash.
  *out=reinterpret_cast<remixapi_LightHandle>(uintptr_t(++created));handles.insert(*out);submitted.push_back(state);
  return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL TestDestroy(remixapi_LightHandle handle) {
  Check(handles.erase(handle)==1,"destroy existing client handle exactly once");++destroyed;return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL TestDraw(remixapi_LightHandle handle) {
  Check(handles.count(handle)==1,"draw only existing handle");++drawn;return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL TestConfig(const char*,const char*) {return REMIXAPI_ERROR_CODE_SUCCESS;}
static void Put(unsigned char* bytes,unsigned offset,uint32_t value) {memcpy(bytes+offset,&value,4);}
struct Fixture {
  unsigned char scene[0x54]{},manager[0x24]{},lights[3][0x158]{};
  uintptr_t address() {return reinterpret_cast<uintptr_t>(scene);}
  Fixture() {
    Put(scene,0x34,reinterpret_cast<uintptr_t>(manager));Put(manager,0,0x6e8ca4);Put(manager,0x20,address());
    for(unsigned i=0;i<3;++i) {
      auto raw=lights[i];Put(raw,0,0x6f0c88);Put(raw,0x3c,address());Put(raw,0xb0,0x100);
      Put(raw,0xc0,i);raw[0xed]=1;
      D3DLIGHT9 d{};d.Type=i==0?D3DLIGHT_DIRECTIONAL:i==1?D3DLIGHT_POINT:D3DLIGHT_SPOT;
      d.Diffuse={.5f,.25f,.125f,1};d.Direction={0,-2,0};d.Position={12,34,56};
      d.Range=100;d.Attenuation0=1;d.Theta=.4f;d.Phi=.8f;d.Falloff=1;
      memcpy(raw+0xf0,&d,sizeof(d));
    }
    membership(3);
  }
  void membership(unsigned count) {
    Put(manager,0x18,count);Put(manager,0x10,count?reinterpret_cast<uintptr_t>(lights[0]):0);
    Put(manager,0x14,count?reinterpret_cast<uintptr_t>(lights[count-1]):0);
    for(unsigned i=0;i<count;++i) {
      Put(lights[i],0xb8,i?reinterpret_cast<uintptr_t>(lights[i-1]):0);
      Put(lights[i],0xbc,i+1<count?reinterpret_cast<uintptr_t>(lights[i+1]):0);
    }
  }
};
static void LightAttributionTests(Fixture& fixture) {
  namespace profile=winx_remix::frame_profile;
  using Context=profile::Context;using Metric=profile::Metric;using Count=profile::LightCount;
  const bool previouslyEnabled=profile::enabled.load(std::memory_order_relaxed);
  profile::Enable(false);profile::Reset();
  std::unique_lock<std::recursive_mutex> borrow(guard);
  scene_audit::DirectLightWitness witness;scene_audit::LightRegistry scratch;
  Check(scene_audit::ReadSubmittedDirectLights(borrow,fixture.address(),witness,&scratch),"profiling fixture starts with a real current direct witness");
  Check(!profile::Get(Context::Other,Metric::LightCapture).calls,"disabled light profiling does not collect timing");
  profile::Enable();
  {
    profile::ScopedContext group(Context::IndependentGroup);
    Check(scene_audit::CurrentSubmittedDirectLights(borrow,witness,&scratch),"observed light validation retains accepted result");
  }
  const auto count=[&](Count value){return profile::GetLightWork(Context::IndependentGroup,value);};
  Check(profile::Get(Context::IndependentGroup,Metric::LightCapture).calls==1&&
    profile::Get(Context::IndependentGroup,Metric::LightValidation).calls==1&&count(Count::CaptureValid)==1&&
    count(Count::CapturedRecords)==3&&count(Count::RecordReadCalls)==3&&count(Count::RecordReadSucceeded)==3,
    "one capture scope records complete fresh membership and record reads");
  Check(count(Count::ValidationAccepted)==1&&count(Count::ValidationRecords)==3&&count(Count::EligibleRecords)==3&&
    count(Count::ConvertCalls)==3&&count(Count::WitnessComparisons)==6&&count(Count::OwnerLookups)==3,
    "one post-capture batch counts original conversions linear comparisons and owner lookups");
  Check(count(Count::VirtualQueryCalls)>0&&count(Count::VirtualQueryCalls)+count(Count::RegionCacheHits)>=5,
    "capture exposes protection query and metadata-hit work without changing the reader");
  profile::Reset();fixture.lights[0][0xec]=1;
  {
    profile::ScopedContext group(Context::IndependentGroup);
    Check(!scene_audit::CurrentSubmittedDirectLights(borrow,witness,&scratch),"observed dirty light remains rejected");
  }
  Check(count(Count::CaptureValid)==1&&count(Count::ValidationRejected)==1&&count(Count::ValidationRecords)==1&&
    count(Count::EligibleRecords)==0&&count(Count::ConvertCalls)==0&&count(Count::OwnerLookups)==0,
    "rejected validation preserves its exact work prefix before conversion and owner lookup");
  fixture.lights[0][0xec]=0;profile::Reset();
  float originalRed=0;memcpy(&originalRed,fixture.lights[0]+0xf4,4);
  const float invalidRed=-1;memcpy(fixture.lights[0]+0xf4,&invalidRed,4);
  {
    profile::ScopedContext group(Context::IndependentGroup);
    Check(!scene_audit::CurrentSubmittedDirectLights(borrow,witness,&scratch),"observed invalid conversion remains rejected");
  }
  Check(count(Count::ValidationRejected)==1&&count(Count::ValidationRecords)==1&&count(Count::EligibleRecords)==1&&
    count(Count::ConvertCalls)==1&&count(Count::WitnessComparisons)==0&&count(Count::OwnerLookups)==0,
    "conversion failure counts the entered conversion without inventing later comparisons or lookups");
  memcpy(fixture.lights[0]+0xf4,&originalRed,4);profile::Reset();
  auto& firstOwner=scene_audit::ownedLights.at({fixture.address(),uint32_t(reinterpret_cast<uintptr_t>(fixture.lights[0]))});
  const bool wasUsable=firstOwner.usable;firstOwner.usable=false;
  {
    profile::ScopedContext group(Context::IndependentGroup);
    Check(!scene_audit::CurrentSubmittedDirectLights(borrow,witness,&scratch),"observed unusable API owner remains rejected");
  }
  Check(count(Count::ValidationRejected)==1&&count(Count::ValidationRecords)==1&&count(Count::ConvertCalls)==1&&
    count(Count::WitnessComparisons)==1&&count(Count::OwnerLookups)==1,
    "owner failure retains conversion and witness work through exactly one map lookup");
  firstOwner.usable=wasUsable;profile::Reset();Put(fixture.manager,0,0);
  {
    profile::ScopedContext group(Context::IndependentGroup);
    Check(!scene_audit::CurrentSubmittedDirectLights(borrow,witness,&scratch),"observed malformed header remains rejected");
  }
  Check(count(Count::CaptureInvalid)==1&&count(Count::CapturedRecords)==0&&count(Count::RecordReadCalls)==0&&
    count(Count::ValidationRejected)==1&&count(Count::ValidationRecords)==0,
    "invalid capture and empty post-capture validation have separate observed outcomes");
  Put(fixture.manager,0,0x6e8ca4);profile::Reset();
  auto stale=witness;++stale.frame;
  Check(!scene_audit::CurrentSubmittedDirectLights(borrow,stale,&scratch)&&
    !profile::Get(Context::Other,Metric::LightCapture).calls&&!profile::Get(Context::Other,Metric::LightValidation).calls,
    "pre-capture witness rejection cannot fabricate entered capture or validation scopes");
  profile::Enable(previouslyEnabled);profile::Reset();
}
int main() {
  using namespace scene_audit;
  SceneReadWindowTests();
  remixapi_Interface api{};api.CreateLight=TestCreate;api.DestroyLight=TestDestroy;api.DrawLightInstance=TestDraw;api.SetConfigVariable=TestConfig;
  testRemixApi=&api;sceneLightsEnabled=true;Fixture a;
  Check(ReadLights(a.address()).valid,"valid complete linked scene registry");
  frameId=1;SyncLights(a.address());Check(created==3 && drawn==3 && ownedLights.size()==3,"all three geometric light types submitted");
  LightAttributionTests(a);
  {
    std::unique_lock<std::recursive_mutex> borrow(guard);DirectLightWitness witness;LightRegistry scratch,otherScratch;
    Check(ReadSubmittedDirectLights(borrow,a.address(),witness,&scratch),"direct witness captures operation-local registry scratch");
    const auto storage=scratch.lights.data();const auto capacity=scratch.lights.capacity();
    Check(CurrentSubmittedDirectLights(borrow,witness,&otherScratch)&&otherScratch.lights.data()!=storage,
      "separate operation scratch owns separate freshly observed bytes");
    float direction=-4;memcpy(a.lights[0]+0x134,&direction,4);
    Check(CurrentSubmittedDirectLights(borrow,witness,&scratch)&&scratch.lights.data()==storage&&scratch.lights.capacity()==capacity,
      "reused storage still performs original conversion for semantically equivalent changed direction");
    float color=.75f;memcpy(a.lights[0]+0xf4,&color,4);
    Check(!CurrentSubmittedDirectLights(borrow,witness,&scratch),"changed live payload cannot use a previous accepted scratch");
    color=.5f;memcpy(a.lights[0]+0xf4,&color,4);
    Check(CurrentSubmittedDirectLights(borrow,witness,&scratch),"restored source is freshly read after rejection");
    Put(a.lights[2],0xc0,3);a.lights[2][0xed]=0;Put(a.lights[2],0xbc,reinterpret_cast<uintptr_t>(a.lights[0]));
    Check(!CurrentSubmittedDirectLights(borrow,witness,&scratch)&&!scratch.valid,
      "malformed inactive ambient membership is rejected before conversion and owner comparison");
    Put(a.lights[2],0xc0,2);a.lights[2][0xed]=1;Put(a.lights[2],0xbc,0);
    Check(CurrentSubmittedDirectLights(borrow,witness,&scratch),"complete fresh registry replaces malformed partial scratch");
    Put(a.manager,0,0);
    Check(!CurrentSubmittedDirectLights(borrow,witness,&scratch)&&!scratch.valid&&scratch.lights.empty()&&scratch.manager==0,
      "unreadable next registry header clears prior accepted membership");
    Put(a.manager,0,0x6e8ca4);
    Check(CurrentSubmittedDirectLights(borrow,witness,&scratch),"scratch capacity remains usable after failed header");
    lightsBusy=true;
    Check(!CurrentSubmittedDirectLights(borrow,witness,&scratch)&&!scratch.valid,"reentrant light callback cannot bless old scratch");
    lightsBusy=false;direction=-2;memcpy(a.lights[0]+0x134,&direction,4);
    Check(CurrentSubmittedDirectLights(borrow,witness,&scratch),"post-callback validation captures current registry again");
  }
  Check(std::abs(submitted[0].radiance.x-5)<1e-6 && submitted[0].direction.y==-1,"native directional color and normalized world direction");
  Check(std::abs(submitted[1].radiance.x-19.894367f)<1e-4 && submitted[1].position.z==56,"pinned Remix point conversion");
  Check(std::abs(submitted[2].angle-22.918312f)<1e-4,"spot outer half-angle converted to degrees");
  SyncLights(a.address());Check(created==3 && drawn==3,"no duplicate submission within frame");
  ++frameId;SyncLights(a.address());Check(created==3 && drawn==6,"static resources reused next frame");
  RetireAbsentLights(a.address());++frameId;RetireAbsentLights(a.address());
  Check(destroyed==0 && ownedLights.size()==3,"no draws/culling calls does not mean deletion");
  float changed=.75f;memcpy(a.lights[0]+0xf4,&changed,4);
  ++frameId;SyncLights(a.address());Check(created==4 && destroyed==1 && handles.size()==3,"update replaces client mapping without leak");
  a.lights[1][0xed]=0;++frameId;SyncLights(a.address());Check(ownedLights.size()==2 && destroyed==2,"disabled light retired");
  Put(a.lights[2],0xb0,0);RetireAbsentLights(a.address());Check(ownedLights.size()==1,"hierarchy-inactive light retired without draw");
  a.membership(0);RetireAbsentLights(a.address());Check(ownedLights.empty() && handles.empty(),"scene unlink retires all handles");
  Fixture b;++frameId;SyncLights(b.address());Check(ownedLights.size()==3,"new scene resources");
  ++frameId;a.membership(1);SyncLights(a.address());Check(ownedLights.size()==1 && handles.size()==1,"scene replacement retires old ownership");
  Put(a.manager,0x18,513);++frameId;SyncLights(a.address());Check(ownedLights.empty() && !ignoredLegacy[0],"invalid registry rolls back to legacy");
  a.membership(1);failCreate=true;++frameId;SyncLights(a.address());
  Check(ownedLights.empty() && !ignoredLegacy[0],"API failure keeps no fake resource");failCreate=false;
  // Ambient is not made into a global lamp. Its per-object semantics are pending.
  Put(a.lights[0],0xc0,3);++frameId;SyncLights(a.address());Check(ownedLights.empty(),"ambient is explicit unsupported material input");
  Put(a.lights[0],0xc0,0);++frameId;SyncLights(a.address());ResetSceneLights();
  Check(handles.empty() && ownedLights.empty() && ignoredLegacy[0],"device reset releases resources and retains submitted-frame policy");
  ++frameId;EndSceneLightFrame();
  Check(!ignoredLegacy[0],"device reset restores legacy on the next frame");
  Fixture gains;++frameId;SyncLights(gains.address());
  const auto baseline=ownedLights;const unsigned gainCreates=created,gainDestroys=destroyed;
  const char* invalid[]={"", "-1", "1001", "nan", "inf", "1e99", "1e-99", "10x", "10 20"};
  for(auto value:invalid) Check(!SetSceneLightGain(value) && sceneLightGain==10,"invalid gain leaves state unchanged");
  Check(!SetSceneLightGain(nullptr),"null gain rejected");
  Check(SetSceneLightGain("1000"),"bounded maximum gain accepted");
  ++frameId;SyncLights(gains.address());
  Check(created==gainCreates+3 && destroyed==gainDestroys+3 && handles.size()==3,"gain updates replace all three client handles without leaks");
  for(const auto& entry:baseline) {
    const auto& now=ownedLights.at(entry.first);
    Check(now.id==entry.second.id,"gain preserves stable API hash");
    Check(std::abs(now.state.radiance.x/entry.second.state.radiance.x-100)<1e-3f,"gain scales directional point and spot radiance");
    Check(memcmp(&now.state.position,&entry.second.state.position,sizeof(now.state.position))==0 &&
      memcmp(&now.state.direction,&entry.second.state.direction,sizeof(now.state.direction))==0,"gain preserves spatial light parameters");
  }
  Check(SetSceneLightGain("0"),"zero gain accepted for matched camera comparison");
  ++frameId;SyncLights(gains.address());
  for(const auto& entry:ownedLights) Check(entry.second.state.radiance.x==0 && entry.second.state.radiance.y==0 && entry.second.state.radiance.z==0,"zero gain removes all radiance");
  Check(SetSceneLightGain("1e1"),"default gain restored");
  ++frameId;SyncLights(gains.address());
  for(const auto& entry:baseline) Check(memcmp(&ownedLights.at(entry.first).state,&entry.second.state,sizeof(ConvertedLight))==0,"gain round trip restores original conversion exactly");
  ResetSceneLights();Check(handles.empty(),"gain comparison releases resources");
  printf("PASS %u checks; creates=%u destroys=%u; live handles=%zu\n",checks,created,destroyed,handles.size());
}
