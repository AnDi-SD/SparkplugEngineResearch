// Own bounded adapter contract tests. Literal PC ABI buffers, recording API;
// no game instructions are run. GPU/bridge behavior is checked separately live.
#define WINX_REMIX_TEST
#include "winx_d3d9_probe.cpp"
#include <cstdlib>

static unsigned checks,created,destroyed,drawn;
static bool failCreate;
static std::set<remixapi_LightHandle> handles;
static std::vector<scene_audit::ConvertedLight> submitted;
static void Check(bool value,const char* message) {
  ++checks;if(!value) {fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
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
int main() {
  using namespace scene_audit;
  remixapi_Interface api{};api.CreateLight=TestCreate;api.DestroyLight=TestDestroy;api.DrawLightInstance=TestDraw;api.SetConfigVariable=TestConfig;
  testRemixApi=&api;sceneLightsEnabled=true;Fixture a;
  Check(ReadLights(a.address()).valid,"valid complete linked scene registry");
  frameId=1;SyncLights(a.address());Check(created==3 && drawn==3 && ownedLights.size()==3,"all three geometric light types submitted");
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
  Check(handles.empty() && ownedLights.empty() && !ignoredLegacy[0],"device reset releases resources and policy");
  printf("PASS %u checks; creates=%u destroys=%u; live handles=%zu\n",checks,created,destroyed,handles.size());
}
