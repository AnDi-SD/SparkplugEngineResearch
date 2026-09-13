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
struct OwnedLight {
  remixapi_LightHandle handle=nullptr; uint64_t id=0; ConvertedLight state{}; unsigned seen=0;
  bool usable=false,retiring=false;uint64_t cleanupAttempt=0;
};
using LightKey=std::pair<uintptr_t,uint32_t>;
static constexpr size_t maxOwnedLights=4096,maxSeenLightScenes=128;
static bool lightsBusy,retireAllLights;
static uint64_t lightRevision=1,lightOperationId;
static bool attemptedLightType[3]{},restoreLegacyPending[3]{};
static unsigned attemptedLightFrame[3]{};
// Reentry never edits ownership or calls the API recursively. The outer call
// still owns all returned handles, then quarantines them on invalidation.
static void InvalidateLights(){++lightRevision;retireAllLights=true;}
struct LightOperation {
  std::lock_guard<std::recursive_mutex> lock{guard};
  bool outer=false;uint64_t revision=0;unsigned frame=frameId;float gain=sceneLightGain;
  bool enabled=sceneLightsEnabled,comparison=keepSceneLightsForComparison;
  LightOperation(){if(lightsBusy){InvalidateLights();return;}lightsBusy=true;outer=true;revision=lightRevision;++lightOperationId;}
  ~LightOperation(){if(outer)lightsBusy=false;}
  bool Stable() const{return outer&&revision==lightRevision&&frame==frameId&&gain==sceneLightGain&&
    enabled==sceneLightsEnabled&&comparison==keepSceneLightsForComparison;}
};
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
#if defined(WINX_REMIX_NATIVE_LIGHT_SOURCE_AVAILABLE)
  (void)native_light_source::Resolve(source,d);
#endif
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
static auto LightApi()->decltype(GetRemixApi()) {
  try {return GetRemixApi();}catch(const std::bad_alloc&){++lightFailures;return nullptr;}
}
static bool IgnoreLegacy(unsigned type,bool ignore) {
  if(type>=3)return false;
  if(!ignore&&attemptedLightType[type]&&attemptedLightFrame[type]==frameId) {
    restoreLegacyPending[type]=true;return false;
  }
  if(ignoredLegacy[type]==ignore){restoreLegacyPending[type]=false;return true;}
  static const char* keys[]={"rtx.ignoreGameDirectionalLights","rtx.ignoreGamePointLights","rtx.ignoreGameSpotLights"};
  auto api=LightApi();if(!api||!api->SetConfigVariable){if(!ignore)restoreLegacyPending[type]=true;return false;}
  remixapi_ErrorCode result=REMIXAPI_ERROR_CODE_GENERAL_FAILURE;
  try {result=api->SetConfigVariable(keys[type],ignore?"True":"False");}catch(const std::bad_alloc&){}
  LightEvent("legacy_policy",0,type,0,result);
  if(result!=REMIXAPI_ERROR_CODE_SUCCESS){++lightFailures;if(!ignore)restoreLegacyPending[type]=true;return false;}
  ignoredLegacy[type]=ignore;restoreLegacyPending[type]=false;return true;
}
static void MarkLightsRetired() {
  for(auto& pair:ownedLights){pair.second.retiring=true;pair.second.usable=false;}
  scenesSeen.clear();retireAllLights=false;
}
static bool Destroy(const LightKey& key) {
  OwnedLight light{};
  {const auto found=ownedLights.find(key);if(found==ownedLights.end())return true;
    found->second.usable=false;found->second.retiring=true;
    if(!found->second.handle){ownedLights.erase(found);return true;}
    if(found->second.cleanupAttempt==lightOperationId)return false;
    found->second.cleanupAttempt=lightOperationId;light=found->second;}
  auto api=LightApi();if(!api||!api->DestroyLight)return false;
  remixapi_ErrorCode result=REMIXAPI_ERROR_CODE_GENERAL_FAILURE;
  try {result=api->DestroyLight(light.handle);}catch(const std::bad_alloc&){}
  LightEvent("destroy",key.first,key.second,light.id,result);
  // Nested operations cannot erase or replace this preexisting owner node.
  const auto found=ownedLights.find(key);
  if(result!=REMIXAPI_ERROR_CODE_SUCCESS){++lightFailures;return false;}
  if(found!=ownedLights.end()&&found->second.handle==light.handle){ownedLights.erase(found);++lightDestroys;}
  return true;
}
static void SweepRetiredLights() {
  LightKey cursor{};bool first=true;
  for(size_t n=0;n<maxOwnedLights;++n) {
    LightKey key{};bool retiring=false;
    {const auto it=first?ownedLights.begin():ownedLights.upper_bound(cursor);if(it==ownedLights.end())break;
      key=it->first;retiring=it->second.retiring;}
    first=false;cursor=key;if(retiring)Destroy(key);
  }
}
static void FinishLightOperation(const LightOperation& operation) {
  if(!operation.Stable()||retireAllLights)MarkLightsRetired();
  SweepRetiredLights();
  if(retireAllLights)MarkLightsRetired(); // no unbounded drain on repeated reentry
  if(scenesSeen.empty())for(unsigned type=0;type<3;++type)IgnoreLegacy(type,false);
}
static bool Create(const LightKey& key,const ConvertedLight& state,const LightOperation& operation) {
  OwnedLight before{};
  {const auto found=ownedLights.find(key);if(found==ownedLights.end())return false;before=found->second;}
  auto api=LightApi();if(!operation.Stable()||!api||!api->CreateLight||!api->DrawLightInstance||!api->DestroyLight)return false;
  // Reuse the server hash only after a successful client Destroy return (no server ACK).
  // Preserve the preallocated cache node across update; Destroy() erases only
  // retired entries, so the update uses its own non-erasing destroy boundary.
  if(before.handle) {
    {auto& entry=ownedLights.find(key)->second;entry.usable=false;entry.retiring=true;
      if(entry.cleanupAttempt==lightOperationId)return false;entry.cleanupAttempt=lightOperationId;}
    remixapi_ErrorCode destroyed=REMIXAPI_ERROR_CODE_GENERAL_FAILURE;
    try {destroyed=api->DestroyLight(before.handle);}catch(const std::bad_alloc&){}
    LightEvent("destroy",key.first,key.second,before.id,destroyed);
    if(destroyed!=REMIXAPI_ERROR_CODE_SUCCESS){++lightFailures;return false;}
    auto& entry=ownedLights.find(key)->second;entry.handle=nullptr;++lightDestroys;
    if(!operation.Stable())return false;
  }
  {auto& entry=ownedLights.find(key)->second;
    if(!entry.id){if(!nextLightId)return false;entry.id=nextLightId++;}
    entry.state=state;entry.usable=false;entry.retiring=true;before.id=entry.id;}
  remixapi_LightInfo info{};info.sType=REMIXAPI_STRUCT_TYPE_LIGHT_INFO;info.hash=before.id;info.radiance=state.radiance;
  remixapi_LightInfoDistantEXT distant{};remixapi_LightInfoSphereEXT sphere{};
  if(state.type==0) {
    distant.sType=REMIXAPI_STRUCT_TYPE_LIGHT_INFO_DISTANT_EXT;distant.direction=state.direction;
    distant.angularDiameterDegrees=state.angle;distant.volumetricRadianceScale=1;info.pNext=&distant;
  } else {
    sphere.sType=REMIXAPI_STRUCT_TYPE_LIGHT_INFO_SPHERE_EXT;sphere.position=state.position;
    sphere.radius=state.radius;sphere.volumetricRadianceScale=1;sphere.shaping_hasvalue=state.type==2;
    sphere.shaping_value={state.direction,state.angle,state.softness,state.focus};info.pNext=&sphere;
  }
  remixapi_LightHandle handle=nullptr;remixapi_ErrorCode result=REMIXAPI_ERROR_CODE_GENERAL_FAILURE;
  try {result=api->CreateLight(&info,&handle);}catch(const std::bad_alloc&){}
  // No allocation after API: even an error/exception with output retains its
  // handle in the preallocated node, unusable until cleanup succeeds.
  {auto& entry=ownedLights.find(key)->second;entry.handle=handle;entry.state=state;
    entry.usable=result==REMIXAPI_ERROR_CODE_SUCCESS&&handle&&operation.Stable();entry.retiring=!entry.usable;}
  LightEvent(before.handle?"update":"create",key.first,key.second,before.id,result);
  if(sceneLightLog&&_ftelli64(sceneLightLog)<16*1024*1024)
    fprintf(sceneLightLog,"{\"event\":\"parameters\",\"frame\":%u,\"id\":%llu,\"type\":%u,\"radiance\":[%.9g,%.9g,%.9g],\"position\":[%.9g,%.9g,%.9g],\"direction\":[%.9g,%.9g,%.9g],\"radius\":%.9g,\"angle\":%.9g}\n",frameId,before.id,state.type,state.radiance.x,state.radiance.y,state.radiance.z,state.position.x,state.position.y,state.position.z,state.direction.x,state.direction.y,state.direction.z,state.radius,state.angle);
  if(!ownedLights.find(key)->second.usable){++lightFailures;return false;}
  if(before.handle)++lightUpdates;else ++lightCreates;return true;
}
static void ClearLights() {
  LightOperation operation;if(!operation.outer)return;
  MarkLightsRetired();FinishLightOperation(operation);
}
static void SyncLights(uintptr_t scene) {
  LightOperation operation;if(!operation.outer)return;
  if(!sceneLightsEnabled||keepSceneLightsForComparison){MarkLightsRetired();FinishLightOperation(operation);return;}
  if(retireAllLights)MarkLightsRetired();
  SweepRetiredLights();
  if(!operation.Stable()){FinishLightOperation(operation);return;}
  const auto seen=scenesSeen.find(scene);if(seen!=scenesSeen.end()&&seen->second==frameId)return;
  try {
    if(scenesSeen.size()>=maxSeenLightScenes&&!scenesSeen.count(scene))throw std::bad_alloc();
    scenesSeen[scene]=frameId;
    const auto registry=ReadLights(scene);
    if(!registry.valid){++lightFailures;LightEvent("invalid_registry",scene,0,0,-1);MarkLightsRetired();FinishLightOperation(operation);return;}
    std::map<uint32_t,ConvertedLight> wanted;bool supported[3]={true,true,true};
    for(const auto& light:registry.lights) {
      const auto type=At(light.raw,0xc0);
      if(!light.raw[0xed]||!(At(light.raw,0xb0)&0x100))continue;
      if(type>2){++unsupportedLights;continue;}
      ConvertedLight state{};
      if(light.raw[0xec]||!Convert(light,state)){supported[type]=false;++unsupportedLights;continue;}
      wanted.emplace(light.address,state);
    }
    // Allocate every required owner before any Create. Allocation failure can
    // only retire existing ownership, never strand a newly returned handle.
    for(const auto& pair:wanted)if(supported[pair.second.type]) {
      const LightKey key{scene,pair.first};
      if(ownedLights.size()>=maxOwnedLights&&!ownedLights.count(key))throw std::bad_alloc();
      auto& light=ownedLights.try_emplace(key).first->second;light.seen=frameId;
    }
    for(const auto& pair:wanted)if(supported[pair.second.type]) {
      const LightKey key{scene,pair.first};const auto light=ownedLights.find(key)->second;
      if(!light.usable||memcmp(&light.state,&pair.second,sizeof(ConvertedLight)))
        if(!Create(key,pair.second,operation))supported[pair.second.type]=false;
      if(!operation.Stable()){FinishLightOperation(operation);return;}
    }
    for(auto& pair:ownedLights)if(pair.first.first!=scene||pair.second.seen!=frameId||
        !pair.second.usable||!supported[pair.second.state.type]){pair.second.usable=false;pair.second.retiring=true;}
    SweepRetiredLights();
    auto api=LightApi();if(!api||!api->DrawLightInstance) {
      for(unsigned type=0;type<3;++type)IgnoreLegacy(type,false);
      FinishLightOperation(operation);return;
    }
    for(unsigned type=0;type<3;++type) {
      if(!operation.Stable()){FinishLightOperation(operation);return;}
      if(!IgnoreLegacy(type,supported[type]))supported[type]=false;
    }
    LightKey cursor{};bool first=true;
    for(size_t n=0;n<maxOwnedLights;++n) {
      LightKey key{};OwnedLight light{};
      {const auto it=first?ownedLights.begin():ownedLights.upper_bound(cursor);if(it==ownedLights.end())break;
        key=it->first;light=it->second;}
      first=false;cursor=key;
      if(!operation.Stable()){FinishLightOperation(operation);return;}
      if(key.first!=scene||!light.handle||!light.usable||!supported[light.state.type])continue;
      // Error after submission is not proof of rollback. Keep legacy disabled
      // for this type until frameId advances, including comparison/reset calls.
      attemptedLightType[light.state.type]=true;attemptedLightFrame[light.state.type]=frameId;
      remixapi_ErrorCode result=REMIXAPI_ERROR_CODE_GENERAL_FAILURE;
      try {result=api->DrawLightInstance(light.handle);}catch(const std::bad_alloc&){}
      if(result==REMIXAPI_ERROR_CODE_SUCCESS)++lightDraws;
      else {
        ++lightFailures;supported[light.state.type]=false;
        for(auto& item:ownedLights)if(item.second.state.type==light.state.type) {
          item.second.usable=false;item.second.retiring=true;
        }
        IgnoreLegacy(light.state.type,false);
      }
    }
  }catch(const std::bad_alloc&){++lightFailures;LightEvent("ownership_capacity_or_allocation",scene,0,0,-1);MarkLightsRetired();}
  FinishLightOperation(operation);
}
static void RetireAbsentLights(uintptr_t scene) {
  LightOperation operation;if(!operation.outer)return;
  try {
    const auto registry=ReadLights(scene);
    if(!registry.valid){MarkLightsRetired();FinishLightOperation(operation);return;}
    std::set<uint32_t> present;
    for(const auto& light:registry.lights)if(light.raw[0xed]&&(At(light.raw,0xb0)&0x100))present.insert(light.address);
    for(auto& pair:ownedLights)if(pair.first.first!=scene||!present.count(pair.first.second)) {
      pair.second.usable=false;pair.second.retiring=true;
    }
  }catch(const std::bad_alloc&){++lightFailures;MarkLightsRetired();}
  FinishLightOperation(operation);
}
} // namespace scene_audit
#endif

static void PrepareSceneLights(IDirect3DDevice9* device) {
#if defined(_M_IX86)
  using namespace scene_audit;
  std::lock_guard<std::recursive_mutex> lightLock(guard);
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
  scene_audit::ClearLights();
#endif
}
static void EndSceneLightFrame() {
#if defined(_M_IX86)
  using namespace scene_audit;
  std::lock_guard<std::recursive_mutex> lightLock(guard);
  if(!sceneLightsEnabled||keepSceneLightsForComparison) ClearLights();
  else if(!ownedLights.empty()) {
    const auto engine=Word(0x755274);RetireAbsentLights(engine?Word(engine+0x18):0);
  }
  for(auto it=scenesSeen.begin();it!=scenesSeen.end();) {
    if(it->second!=frameId || keepSceneLightsForComparison) it=scenesSeen.erase(it);else ++it;
  }
  {LightOperation operation;
    if(operation.outer)for(unsigned type=0;type<3;++type)
      if(scenesSeen.empty()||restoreLegacyPending[type])IgnoreLegacy(type,false);
  }
  if(sceneLightLog && _ftelli64(sceneLightLog)<16*1024*1024 && (frameId%60==0 || lightCreates || lightUpdates || lightDestroys || lightFailures)) {
    fprintf(sceneLightLog,"{\"event\":\"frame\",\"frame\":%u,\"scenes\":%u,\"owned\":%u,\"creates\":%u,\"updates\":%u,\"destroys\":%u,\"draws\":%u,\"failures\":%u,\"unsupported\":%u,\"legacyIgnored\":[%d,%d,%d]}\n",frameId,static_cast<unsigned>(scenesSeen.size()),static_cast<unsigned>(ownedLights.size()),lightCreates,lightUpdates,lightDestroys,lightDraws,lightFailures,unsupportedLights,ignoredLegacy[0],ignoredLegacy[1],ignoredLegacy[2]);
    fflush(sceneLightLog);
  }
  lightCreates=lightUpdates=lightDestroys=lightDraws=lightFailures=unsupportedLights=0;
#endif
}
