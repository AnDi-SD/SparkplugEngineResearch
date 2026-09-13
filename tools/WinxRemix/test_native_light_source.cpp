// Integration against literal native fields and independent known payloads.
// Original-instruction arithmetic comparisons are run separately (CP44/CP90).
#define main old_light_test_main
#include "test_scene_lights.cpp"
#undef main
#include <limits>

static void FloatAt(unsigned char* raw,unsigned offset,float value){memcpy(raw+offset,&value,4);}
static void NativeFields(Fixture& fixture) {
  for(unsigned i=0;i<3;++i) {
    auto raw=fixture.lights[i];D3DLIGHT9 d{};memcpy(&d,raw+0xf0,sizeof(d));
    memcpy(raw+0x74,&d.Position,12);memcpy(raw+0xa4,&d.Direction,12);memcpy(raw+0xc4,&d.Diffuse,16);
    raw[0xd4]=0;FloatAt(raw,0xd8,1);FloatAt(raw,0xe0,d.Range);
    FloatAt(raw,0xe4,d.Theta);FloatAt(raw,0xe8,d.Phi);
  }
}
int main() {
  using namespace scene_audit;
  Fixture fixture;NativeFields(fixture);auto registry=ReadLights(fixture.address());
  Check(registry.valid&&registry.lights.size()==3,"owned verified registry snapshot");
  native_light_source::enabled=true;native_light_source::submitEnabled=true;
  for(const auto& record:registry.lights) {
    D3DLIGHT9 original{};memcpy(&original,record.raw+0xf0,sizeof(original));
    D3DLIGHT9 resolved{};
    Check(native_light_source::Resolve(record,resolved),"all three literal native light inputs qualify");
    Check(memcmp(&original,&resolved,sizeof(original))==0,"all payload bytes unchanged including unused words");
    native_light_source::keepForComparison=true;ConvertedLight before{};Check(Convert(record,before),"old physical converter");
    native_light_source::keepForComparison=false;ConvertedLight after{};Check(Convert(record,after),"new native source physical converter");
    Check(memcmp(&before,&after,sizeof(before))==0,"identical physical light output");
  }
  auto point=registry.lights[1];D3DLIGHT9 sentinel{};memset(&sentinel,0x5a,sizeof(sentinel));
  const auto before=sentinel;
  FloatAt(point.raw,0xd8,2);
  Check(!native_light_source::Resolve(point,sentinel),"raw intensity change without refresh preserves stale device payload");
  Check(memcmp(&before,&sentinel,sizeof(before))==0,"rejected input does not overwrite caller payload");
  FloatAt(point.raw,0x144,.5f);
  Check(native_light_source::Resolve(point,sentinel),"matching refreshed native intensity accepted");
  Put(point.raw,0xb0,0x108);
  Check(!native_light_source::Resolve(point,sentinel),"pending producer dirty bit rejected even when values agree");
  Put(point.raw,0xb0,0x101);
  Check(!native_light_source::Resolve(point,sentinel),"pending world update rejected");
  Put(point.raw,0xb0,0x100);FloatAt(point.raw,0x74,90);
  Check(!native_light_source::Resolve(point,sentinel),"stale world position rejected");
  FloatAt(point.raw,0x124,90);
  Check(native_light_source::Resolve(point,sentinel),"matching updated world position accepted");
  FloatAt(point.raw,0x130,42);
  Check(native_light_source::Resolve(point,sentinel)&&sentinel.Direction.x==42,"unused point direction remains original payload");
  FloatAt(point.raw,0xd8,std::numeric_limits<float>::quiet_NaN());
  Check(!native_light_source::Resolve(point,sentinel),"invalid raw input rejected");
  point=registry.lights[1];point.raw[0xd4]=2;
  Check(!native_light_source::Resolve(point,sentinel),"unsupported bool representation rejected");
  point=registry.lights[1];point.raw[0xed]=0;
  Check(!native_light_source::Resolve(point,sentinel),"disabled light rejected");
  point=registry.lights[1];Put(point.raw,0xb0,0);
  Check(!native_light_source::Resolve(point,sentinel),"hierarchy inactive rejected");
  point=registry.lights[1];Put(point.raw,0,0);
  Check(!native_light_source::Resolve(point,sentinel),"unknown concrete light rejected");
  point=registry.lights[1];Put(point.raw,0xc0,3);
  Check(!native_light_source::Resolve(point,sentinel),"ambient not reinterpreted as geometric lamp");
  point=registry.lights[1];native_light_source::submitEnabled=false;
  const auto matches=native_light_source::matched,uses=native_light_source::used;
  Check(!native_light_source::Resolve(point,sentinel)&&native_light_source::matched==matches+1&&native_light_source::used==uses,"observe-only compares without switching source");
  native_light_source::submitEnabled=true;
  remixapi_Interface api{};api.CreateLight=TestCreate;api.DestroyLight=TestDestroy;api.DrawLightInstance=TestDraw;api.SetConfigVariable=TestConfig;
  testRemixApi=&api;sceneLightsEnabled=true;frameId=1;SyncLights(fixture.address());
  Check(created==3&&drawn==3&&ownedLights.size()==3,"full current scene submits three native input lights");
  const auto nativeUses=native_light_source::used;
  native_light_source::keepForComparison=true;++frameId;SyncLights(fixture.address());
  Check(created==3&&drawn==6&&native_light_source::used==nativeUses,"A/B keeps identical resources and old inputs");
  native_light_source::keepForComparison=false;++frameId;SyncLights(fixture.address());
  Check(created==3&&drawn==9&&native_light_source::used==nativeUses+3,"native inputs restored without resource churn");
  fixture.membership(0);++frameId;RetireAbsentLights(fixture.address());
  Check(handles.empty()&&ownedLights.empty()&&destroyed==3,"scene unlink releases all three native input lights");
  Check(native_light_source::attempts==native_light_source::matched+native_light_source::mismatches+native_light_source::dirty+native_light_source::invalid,"source counters conserve attempts");
  printf("{\"status\":\"PASS\",\"checks\":%u,\"created\":%u,\"destroyed\":%u,\"remaining\":%zu,\"nativeMatches\":%u,\"nativeUses\":%u}\n",checks,created,destroyed,handles.size(),native_light_source::matched,native_light_source::used);
}
