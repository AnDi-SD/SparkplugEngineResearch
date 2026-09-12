// Our scene/API lifetime adapter. Source values are original spDXLight payloads;
// the physical conversion is a pinned NVIDIA CPU helper, not game logic.
#include "third-party/remix_light_conversion.h"

#if defined(_M_IX86)
namespace scene_audit {
struct ConvertedLight {
  uint32_t type=0;
  remixapi_Float3D radiance{}, position{}, direction{};
  float radius=4, angle=0.0349f*180.0f/remix_light_conversion::kPi, softness=0, focus=0;
};
struct OwnedLight { remixapi_LightHandle handle=nullptr; uint64_t id=0; ConvertedLight state{}; unsigned seen=0; };
static std::map<std::pair<uintptr_t,uint32_t>,OwnedLight> ownedLights;
static std::map<uintptr_t,unsigned> scenesSeen;
static uint64_t nextLightId=0x57584c0000000001ull;
static bool ignoredLegacy[3]{};
static unsigned lightCreates,lightUpdates,lightDestroys,lightDraws,lightFailures,unsupportedLights;

static void LightEvent(const char* event,uintptr_t scene,uint32_t object,uint64_t id,int result) {
  if(sceneLightLog && _ftelli64(sceneLightLog)<16*1024*1024)
    fprintf(sceneLightLog,"{\"event\":\"%s\",\"frame\":%u,\"scene\":%u,\"object\":%u,\"id\":%llu,\"result\":%d}\n",event,frameId,scene,object,id,result);
}
static bool Normalize(remixapi_Float3D& v) {
  const float length=std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);
  if(!std::isfinite(length) || length<1e-8f) return false;
  v.x/=length;v.y/=length;v.z/=length;return true;
}
static bool Convert(const LightRecord& source, ConvertedLight& output) {
  D3DLIGHT9 d{};memcpy(&d,source.raw+0xf0,sizeof(d));
  const auto type=At(source.raw,0xc0);if(type>2) return false;
  if(d.Type!=(type==0?D3DLIGHT_DIRECTIONAL:type==1?D3DLIGHT_POINT:D3DLIGHT_SPOT)) return false;
  const float color[3]={d.Diffuse.r,d.Diffuse.g,d.Diffuse.b};
  for(auto value:color) if(!std::isfinite(value) || value<0) return false;
  output.type=type;
  output.direction={d.Direction.x,d.Direction.y,d.Direction.z};
  if(type!=1 && !Normalize(output.direction)) return false;
  if(type==0) {
    output.radiance={color[0]*sceneLightGain,color[1]*sceneLightGain,color[2]*sceneLightGain};
  } else {
    const float inputs[]={d.Position.x,d.Position.y,d.Position.z,d.Range,d.Attenuation0,d.Attenuation1,d.Attenuation2};
    for(auto value:inputs) if(!std::isfinite(value)) return false;
    if(d.Range<=0 || d.Attenuation0<0 || d.Attenuation1<0 || d.Attenuation2<0 ||
       d.Attenuation0+d.Attenuation1+d.Attenuation2<=0) return false;
    const float brightness=(std::max)(color[0],(std::max)(color[1],color[2]));
    const float intensity=brightness>0?remix_light_conversion::calculateIntensity(d,output.radius,sceneLightGain):0;
    if(!std::isfinite(intensity) || intensity<0) return false;
    const float scale=brightness>0?intensity/brightness:0;
    output.radiance={color[0]*scale,color[1]*scale,color[2]*scale};
    output.position={d.Position.x,d.Position.y,d.Position.z};
    if(type==2) {
      if(!std::isfinite(d.Theta) || !std::isfinite(d.Phi) || !std::isfinite(d.Falloff) ||
         d.Theta<0 || d.Theta>d.Phi || d.Phi>remix_light_conversion::kPi || d.Falloff<0) return false;
      output.angle=d.Phi*90.0f/remix_light_conversion::kPi;
      output.softness=std::cos(d.Theta/2)-std::cos(d.Phi/2);output.focus=d.Falloff;
    }
  }
  return std::isfinite(output.radiance.x) && std::isfinite(output.radiance.y) && std::isfinite(output.radiance.z);
}
static bool IgnoreLegacy(unsigned type,bool ignore) {
  if(ignoredLegacy[type]==ignore) return true;
  static const char* keys[]={"rtx.ignoreGameDirectionalLights","rtx.ignoreGamePointLights","rtx.ignoreGameSpotLights"};
  auto api=GetRemixApi();if(!api) return false;
  const auto result=api->SetConfigVariable(keys[type],ignore?"True":"False");
  LightEvent("legacy_policy",0,type,0,result);
  if(result!=REMIXAPI_ERROR_CODE_SUCCESS) {++lightFailures;return false;}
  ignoredLegacy[type]=ignore;return true;
}
static void Destroy(OwnedLight& light,uintptr_t scene,uint32_t object) {
  if(!light.handle) return;
  auto api=GetRemixApi();if(!api) return;
  const auto result=api->DestroyLight(light.handle);
  LightEvent("destroy",scene,object,light.id,result);
  if(result!=REMIXAPI_ERROR_CODE_SUCCESS) ++lightFailures;
  light.handle=nullptr;++lightDestroys;
}
static bool Create(OwnedLight& light,const ConvertedLight& state,uintptr_t scene,uint32_t object) {
  auto api=GetRemixApi();if(!api || !api->CreateLight || !api->DrawLightInstance || !api->DestroyLight) return false;
  const bool update=light.handle!=nullptr;
  // x86 bridge allocates a new client handle on every CreateLight. Remove the
  // old mapping BEFORE recreating the same server hash; otherwise it leaks.
  if(update) Destroy(light,scene,object);
  if(!light.id) light.id=nextLightId++;
  remixapi_LightInfo info{};info.sType=REMIXAPI_STRUCT_TYPE_LIGHT_INFO;info.hash=light.id;info.radiance=state.radiance;
  remixapi_LightInfoDistantEXT distant{};
  remixapi_LightInfoSphereEXT sphere{};
  if(state.type==0) {
    distant.sType=REMIXAPI_STRUCT_TYPE_LIGHT_INFO_DISTANT_EXT;distant.direction=state.direction;
    distant.angularDiameterDegrees=state.angle;distant.volumetricRadianceScale=1;info.pNext=&distant;
  } else {
    sphere.sType=REMIXAPI_STRUCT_TYPE_LIGHT_INFO_SPHERE_EXT;sphere.position=state.position;
    sphere.radius=state.radius;sphere.volumetricRadianceScale=1;sphere.shaping_hasvalue=state.type==2;
    sphere.shaping_value={state.direction,state.angle,state.softness,state.focus};info.pNext=&sphere;
  }
  const auto result=api->CreateLight(&info,&light.handle);
  LightEvent(update?"update":"create",scene,object,light.id,result);
  if(sceneLightLog && _ftelli64(sceneLightLog)<16*1024*1024)
    fprintf(sceneLightLog,"{\"event\":\"parameters\",\"frame\":%u,\"id\":%llu,\"type\":%u,\"radiance\":[%.9g,%.9g,%.9g],\"position\":[%.9g,%.9g,%.9g],\"direction\":[%.9g,%.9g,%.9g],\"radius\":%.9g,\"angle\":%.9g}\n",frameId,light.id,state.type,state.radiance.x,state.radiance.y,state.radiance.z,state.position.x,state.position.y,state.position.z,state.direction.x,state.direction.y,state.direction.z,state.radius,state.angle);
  if(result!=REMIXAPI_ERROR_CODE_SUCCESS || !light.handle) {++lightFailures;return false;}
  light.state=state;if(update) ++lightUpdates;else ++lightCreates;return true;
}
static void ClearLights() {
  for(auto& pair:ownedLights) Destroy(pair.second,pair.first.first,pair.first.second);
  ownedLights.clear();scenesSeen.clear();
  for(unsigned type=0;type<3;++type) IgnoreLegacy(type,false);
}
static void SyncLights(uintptr_t scene) {
  if(!sceneLightsEnabled || keepSceneLightsForComparison) {ClearLights();return;}
  if(scenesSeen.count(scene) && scenesSeen[scene]==frameId) return;
  scenesSeen[scene]=frameId;
  const auto registry=ReadLights(scene);
  if(!registry.valid) {++lightFailures;LightEvent("invalid_registry",scene,0,0,-1);ClearLights();return;}
  std::map<uint32_t,ConvertedLight> wanted;
  bool supported[3]={true,true,true};
  for(const auto& light:registry.lights) {
    const auto type=At(light.raw,0xc0);
    // Original enabled + hierarchy-active predicate, independent of visibility.
    if(!light.raw[0xed] || !(At(light.raw,0xb0)&0x100)) continue;
    if(type>2) {++unsupportedLights;continue;} // Ambient is a per-object material input.
    ConvertedLight state{};
    if(light.raw[0xec] || !Convert(light,state)) {supported[type]=false;++unsupportedLights;continue;}
    wanted.emplace(light.address,state);
  }
  // A type with unsupported source parameters stays entirely on the legacy path.
  for(const auto& pair:wanted) if(supported[pair.second.type]) {
    auto& light=ownedLights[{scene,pair.first}];light.seen=frameId;
    if(!light.handle || memcmp(&light.state,&pair.second,sizeof(ConvertedLight)))
      if(!Create(light,pair.second,scene,pair.first)) supported[pair.second.type]=false;
  }
  for(auto it=ownedLights.begin();it!=ownedLights.end();) {
    if(!it->second.handle || it->first.first!=scene || it->second.seen!=frameId || !supported[it->second.state.type]) {
      Destroy(it->second,it->first.first,it->first.second);it=ownedLights.erase(it);
    } else ++it;
  }
  for(unsigned type=0;type<3;++type) if(!IgnoreLegacy(type,supported[type])) supported[type]=false;
  auto api=GetRemixApi();if(!api) return;
  for(auto& pair:ownedLights) if(pair.first.first==scene && pair.second.handle && supported[pair.second.state.type]) {
    const auto result=api->DrawLightInstance(pair.second.handle);
    if(result==REMIXAPI_ERROR_CODE_SUCCESS) ++lightDraws;else {++lightFailures;IgnoreLegacy(pair.second.state.type,false);}
  }
}

static void RetireAbsentLights(uintptr_t scene) {
  // Not being drawn in a frame is NOT deletion. Check the actual owner/list.
  const auto registry=ReadLights(scene);
  if(!registry.valid) {ClearLights();return;}
  std::set<uint32_t> present;
  for(const auto& light:registry.lights)
    if(light.raw[0xed] && (At(light.raw,0xb0)&0x100)) present.insert(light.address);
  for(auto it=ownedLights.begin();it!=ownedLights.end();) {
    if(it->first.first!=scene || !present.count(it->first.second)) {
      Destroy(it->second,it->first.first,it->first.second);it=ownedLights.erase(it);
    } else ++it;
  }
}
} // namespace scene_audit
#endif

static void PrepareSceneLights(IDirect3DDevice9* device) {
#if defined(_M_IX86)
  using namespace scene_audit;
  if(!sceneLightsEnabled || keepSceneLightsForComparison) return;
  const auto engine=Word(0x755274), scene=engine?Word(engine+0x18):0, camera=engine?Word(engine+0x1c):0;
  if(!scene || !camera || (scenesSeen.count(scene) && scenesSeen[scene]==frameId)) return;
  // Source camera identity + primary target establish the world pass even when
  // visibility selection is cached. No executable hook is needed for lighting.
  unsigned char raw[0x238]{};
  if(!Read(camera,raw,sizeof(raw)) || (At(raw,0)!=0x6ef1e0 && At(raw,0)!=0x6dea20 && At(raw,0)!=0x6dcbc0)) return;
  D3DMATRIX view{},projection{};IDirect3DSurface9* target=nullptr;
  if(FAILED(device->GetTransform(D3DTS_VIEW,&view)) || FAILED(device->GetTransform(D3DTS_PROJECTION,&projection)) ||
      projection._34==0 || memcmp(&view,raw+0xcc,sizeof(view)) || memcmp(&projection,raw+0x10c,sizeof(projection))) return;
  if(FAILED(device->GetRenderTarget(0,&target)) || !target) return;
  const auto found=primaryTargets.find(device);const bool primary=found!=primaryTargets.end() && found->second==target;
  target->Release();if(primary) SyncLights(scene);
#else
  (void)device;
#endif
}

static void ResetSceneLights() {
#if defined(_M_IX86)
  if(sceneLightsEnabled) scene_audit::ClearLights();
#endif
}
static void EndSceneLightFrame() {
#if defined(_M_IX86)
  using namespace scene_audit;
  if(!sceneLightsEnabled) return;
  if(keepSceneLightsForComparison) ClearLights();
  else if(!ownedLights.empty()) {
    const auto engine=Word(0x755274);RetireAbsentLights(engine?Word(engine+0x18):0);
  }
  for(auto it=scenesSeen.begin();it!=scenesSeen.end();) {
    if(it->second!=frameId || keepSceneLightsForComparison) it=scenesSeen.erase(it);else ++it;
  }
  if(scenesSeen.empty()) for(unsigned type=0;type<3;++type) IgnoreLegacy(type,false);
  if(sceneLightLog && _ftelli64(sceneLightLog)<16*1024*1024 && (frameId%60==0 || lightCreates || lightUpdates || lightDestroys || lightFailures)) {
    fprintf(sceneLightLog,"{\"event\":\"frame\",\"frame\":%u,\"scenes\":%u,\"owned\":%u,\"creates\":%u,\"updates\":%u,\"destroys\":%u,\"draws\":%u,\"failures\":%u,\"unsupported\":%u,\"legacyIgnored\":[%d,%d,%d]}\n",frameId,static_cast<unsigned>(scenesSeen.size()),static_cast<unsigned>(ownedLights.size()),lightCreates,lightUpdates,lightDestroys,lightDraws,lightFailures,unsupportedLights,ignoredLegacy[0],ignoredLegacy[1],ignoredLegacy[2]);
    fflush(sceneLightLog);
  }
  lightCreates=lightUpdates=lightDestroys=lightDraws=lightFailures=unsupportedLights=0;
#endif
}
