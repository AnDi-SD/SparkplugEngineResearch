// Own CPU recording API fixture; no D3D device, renderer, or native execution.
#include <windows.h>
#include <d3d9.h>
#define REMIX_ALLOW_X86
#include <remix/remix_c.h>
#include <map>
#include <set>
#include <vector>
#include <mutex>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <new>
static int failAllocation=-1;
void* operator new(size_t size){if(failAllocation==0){failAllocation=-1;throw std::bad_alloc();}
  if(failAllocation>0)--failAllocation;auto p=std::malloc(size?size:1);if(!p)throw std::bad_alloc();return p;}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete(void* p,size_t) noexcept{std::free(p);}
static std::recursive_mutex guard;
static unsigned frameId=1;
static bool sceneLightsEnabled=true,keepSceneLightsForComparison;
static float sceneLightGain=10;
static FILE* sceneLightLog;
static std::map<IDirect3DDevice9*,IDirect3DSurface9*> primaryTargets;
static remixapi_Interface recordingApi{};
static bool apiMissing;
static remixapi_Interface* GetRemixApi(){return apiMissing?nullptr:&recordingApi;}
namespace scene_audit {
struct LightRecord {uint32_t address;unsigned char raw[0x158];};
struct LightRegistry {uint32_t manager=0,count=0;bool valid=true;std::vector<LightRecord> lights;};
static LightRegistry sourceRegistry;
static uintptr_t activeScene=1;
static uint32_t At(const unsigned char* raw,size_t offset){uint32_t result;memcpy(&result,raw+offset,4);return result;}
static uint32_t Word(uintptr_t address){return address==0x755274?0x1000:address==0x1018?uint32_t(activeScene):0;}
static bool Read(uintptr_t,void*,size_t){return false;}
static LightRegistry ReadLights(uintptr_t scene){auto result=sourceRegistry;result.valid=result.valid&&scene==activeScene;return result;}
}
#include "winx_scene_lights.h"
using namespace scene_audit;
struct Live {uint64_t id=0;bool live=false;ConvertedLight state{};};
static Live live[16384];
static unsigned issued,creates,destroys,draws,policyFalse,checks,duplicateHashes,policyFalseType[3]{};
static unsigned failDestroy,failCreate,failDraw,failPolicy;
static bool outputOnError,nullSuccess,throwCreateAfter,throwDestroy,throwDraw;
static void (*createCallback)(),(*destroyCallback)(),(*drawCallback)(),(*policyCallback)();
static void CheckAt(bool yes,unsigned line){++checks;if(!yes){fprintf(stderr,"CHECK %u failed at line %u (frame %u)\n",checks,line,frameId);std::exit(2);}}
#define Check(yes) CheckAt((yes),__LINE__)
static unsigned Remaining(){unsigned count=0;for(unsigned i=1;i<=issued;++i)count+=live[i].live;return count;}
static remixapi_ErrorCode REMIXAPI_CALL CreateApi(const remixapi_LightInfo* info,remixapi_LightHandle* out) {
  ++creates;const bool failed=failCreate&&!--failCreate;
  if(!nullSuccess&&(!failed||outputOnError)) {
    for(unsigned i=1;i<=issued;++i)if(live[i].live&&live[i].id==info->hash)++duplicateHashes;
    const unsigned id=++issued;Check(id<16384);live[id].id=info->hash;live[id].live=true;
    *out=reinterpret_cast<remixapi_LightHandle>(uintptr_t(id));
  }
  if(createCallback){auto callback=createCallback;createCallback=nullptr;callback();}
  if(throwCreateAfter){throwCreateAfter=false;throw std::bad_alloc();}
  return failed?REMIXAPI_ERROR_CODE_GENERAL_FAILURE:REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL DestroyApi(remixapi_LightHandle handle) {
  ++destroys;const auto id=uintptr_t(handle);Check(id&&id<=issued&&live[id].live);
  if(destroyCallback){auto callback=destroyCallback;destroyCallback=nullptr;callback();}
  if(throwDestroy){throwDestroy=false;throw std::bad_alloc();}
  if(failDestroy){--failDestroy;return REMIXAPI_ERROR_CODE_GENERAL_FAILURE;}
  live[id].live=false;return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL DrawApi(remixapi_LightHandle handle) {
  const auto id=uintptr_t(handle);Check(id&&id<=issued&&live[id].live);++draws;
  if(drawCallback){auto callback=drawCallback;drawCallback=nullptr;callback();}
  if(throwDraw){throwDraw=false;throw std::bad_alloc();}
  return failDraw&&!--failDraw?REMIXAPI_ERROR_CODE_GENERAL_FAILURE:REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL PolicyApi(const char* key,const char* value) {
  if(!strcmp(value,"False")){++policyFalse;++policyFalseType[strstr(key,"Directional")?0:strstr(key,"Point")?1:2];}
  if(policyCallback){auto callback=policyCallback;policyCallback=nullptr;callback();}
  if(failPolicy){--failPolicy;return REMIXAPI_ERROR_CODE_GENERAL_FAILURE;}
  return REMIXAPI_ERROR_CODE_SUCCESS;
}
static void Put(LightRecord& light,size_t offset,uint32_t value){memcpy(light.raw+offset,&value,4);}
static void Sources(unsigned count=1) {
  sourceRegistry={};sourceRegistry.valid=true;sourceRegistry.lights.resize(count);sourceRegistry.count=count;
  for(unsigned i=0;i<count;++i){auto& light=sourceRegistry.lights[i];light.address=100+i;Put(light,0xc0,0);Put(light,0xb0,0x100);light.raw[0xed]=1;
    D3DLIGHT9 d{};d.Type=D3DLIGHT_DIRECTIONAL;d.Diffuse={1,.5f,.25f,1};d.Direction={0,-1,0};memcpy(light.raw+0xf0,&d,sizeof(d));}
}
static void ResetFixture() {
  failAllocation=-1;apiMissing=false;recordingApi.CreateLight=CreateApi;recordingApi.DestroyLight=DestroyApi;recordingApi.DrawLightInstance=DrawApi;recordingApi.SetConfigVariable=PolicyApi;
  failDestroy=failCreate=failDraw=failPolicy=0;outputOnError=nullSuccess=throwCreateAfter=throwDestroy=throwDraw=false;
  createCallback=destroyCallback=drawCallback=policyCallback=nullptr;frameId++;ClearLights();Check(ownedLights.empty()&&Remaining()==0);
  scenesSeen.clear();retireAllLights=false;keepSceneLightsForComparison=false;sceneLightsEnabled=true;sceneLightGain=10;activeScene=1;
  for(unsigned i=0;i<3;++i){ignoredLegacy[i]=false;attemptedLightType[i]=restoreLegacyPending[i]=false;}
  creates=destroys=draws=policyFalse=0;Sources();
}
static void NestedClear(){ClearLights();}
static void NestedSync(){SyncLights(activeScene);}
static void NestedRetire(){sourceRegistry.lights.clear();RetireAbsentLights(activeScene);}
static void ChangeGain(){sceneLightGain=20;}
static void CompareAfterDraw(){keepSceneLightsForComparison=true;ClearLights();}
int main() {
  ResetFixture();SyncLights(1);Check(creates==1&&draws==1&&ownedLights.size()==1&&Remaining()==1);
  const auto id=ownedLights.begin()->second.id;const auto handle=ownedLights.begin()->second.handle;
  SyncLights(1);Check(creates==1&&draws==1);++frameId;SyncLights(1);Check(creates==1&&draws==2);
  ++frameId;sceneLightGain=20;SyncLights(1);Check(creates==2&&destroys==1&&draws==3&&ownedLights.begin()->second.id==id&&ownedLights.begin()->second.handle!=handle);
  failDestroy=1;ClearLights();Check(ownedLights.size()==1&&Remaining()==1&&!ownedLights.begin()->second.usable&&ownedLights.begin()->second.retiring);
  ClearLights();Check(ownedLights.empty()&&Remaining()==0);Check(ignoredLegacy[0]);++frameId;EndSceneLightFrame();Check(!ignoredLegacy[0]);

  ResetFixture();SyncLights(1);++frameId;sceneLightGain=20;failDestroy=1;SyncLights(1);
  Check(creates==1&&Remaining()==1&&ownedLights.size()==1&&!ownedLights.begin()->second.usable);
  ++frameId;SyncLights(1);Check(Remaining()==1&&creates==2&&draws==2&&duplicateHashes==0);
  ResetFixture();outputOnError=true;failCreate=1;failDestroy=1;SyncLights(1);
  Check(Remaining()==1&&ownedLights.size()==1&&!ownedLights.begin()->second.usable&&draws==0);ClearLights();Check(Remaining()==0);
  ResetFixture();nullSuccess=true;SyncLights(1);Check(ownedLights.empty()&&Remaining()==0&&draws==0);
  ResetFixture();throwCreateAfter=true;failDestroy=1;SyncLights(1);Check(ownedLights.size()==1&&Remaining()==1&&draws==0);ClearLights();Check(Remaining()==0);
  ResetFixture();SyncLights(1);throwDestroy=true;ClearLights();Check(ownedLights.size()==1&&Remaining()==1);ClearLights();Check(Remaining()==0);

  ResetFixture();createCallback=NestedClear;failDestroy=1;SyncLights(1);Check(draws==0&&Remaining()==1&&ownedLights.size()==1);ClearLights();Check(Remaining()==0);
  ResetFixture();createCallback=NestedSync;SyncLights(1);Check(creates==1&&draws==0&&Remaining()==0);
  ResetFixture();createCallback=ChangeGain;SyncLights(1);Check(draws==0&&Remaining()==0);
  ResetFixture();SyncLights(1);destroyCallback=NestedSync;ClearLights();Check(Remaining()==0&&ownedLights.empty());
  ResetFixture();SyncLights(1);destroyCallback=NestedRetire;ClearLights();Check(Remaining()==0&&ownedLights.empty());
  ResetFixture();policyCallback=NestedClear;SyncLights(1);Check(draws==0&&Remaining()==0);
  ResetFixture();Sources(3);failDraw=2;SyncLights(1);Check(draws==2&&ignoredLegacy[0]&&restoreLegacyPending[0]&&Remaining()==0);
  const auto falseBefore=policyFalseType[0];ClearLights();Check(policyFalseType[0]==falseBefore&&ignoredLegacy[0]);
  ++frameId;keepSceneLightsForComparison=true;EndSceneLightFrame();Check(!ignoredLegacy[0]&&Remaining()==0);
  ResetFixture();throwDraw=true;SyncLights(1);Check(draws==1&&ignoredLegacy[0]);ResetSceneLights();Check(ignoredLegacy[0]);
  ++frameId;sceneLightsEnabled=false;EndSceneLightFrame();Check(!ignoredLegacy[0]);
  ResetFixture();drawCallback=CompareAfterDraw;SyncLights(1);Check(draws==1&&ignoredLegacy[0]&&Remaining()==0);
  ++frameId;EndSceneLightFrame();Check(!ignoredLegacy[0]);

  ResetFixture();SyncLights(1);activeScene=2;sourceRegistry.lights[0].raw[0xec]=1;SyncLights(2);
  Check(draws==1&&ignoredLegacy[0]&&Remaining()==0);++frameId;EndSceneLightFrame();Check(!ignoredLegacy[0]);

  ResetFixture();failAllocation=0;SyncLights(1);Check(creates==0&&ownedLights.empty()&&Remaining()==0);
  // Sweep each allocation point before Create: scenesSeen, sourceRegistry copy,
  // wanted nodes, and cache ownership nodes. No API output can be stranded.
  for(int allocation=0;allocation<9;++allocation){ResetFixture();Sources(2);failAllocation=allocation;SyncLights(1);failAllocation=-1;
    Check(Remaining()==ownedLights.size());ClearLights();Check(Remaining()==0);}
  ResetFixture();SyncLights(1);apiMissing=true;ClearLights();Check(ownedLights.size()==1&&Remaining()==1);apiMissing=false;ClearLights();Check(Remaining()==0);
  ResetFixture();SyncLights(1);recordingApi.DestroyLight=nullptr;ClearLights();Check(ownedLights.size()==1&&Remaining()==1);recordingApi.DestroyLight=DestroyApi;ClearLights();Check(Remaining()==0);
  ResetFixture();SyncLights(1);sourceRegistry.lights.clear();failDestroy=1;RetireAbsentLights(1);Check(Remaining()==1&&ownedLights.size()==1);RetireAbsentLights(1);Check(Remaining()==0);
  ResetFixture();SyncLights(1);sourceRegistry.valid=false;failDestroy=1;RetireAbsentLights(1);Check(Remaining()==1);RetireAbsentLights(1);Check(Remaining()==0);
  ResetFixture();for(size_t i=0;i<maxOwnedLights;++i)ownedLights.emplace(LightKey{2,uint32_t(i)},OwnedLight{});
  SyncLights(1);Check(creates==0&&ownedLights.empty()&&Remaining()==0);
  ResetFixture();
  // Occupy the full owner budget with resources whose previous cleanup failed.
  for(size_t i=0;i<maxOwnedLights;++i) {
    remixapi_LightInfo info{};info.hash=nextLightId++;remixapi_LightHandle handle=nullptr;
    Check(CreateApi(&info,&handle)==REMIXAPI_ERROR_CODE_SUCCESS);
    OwnedLight light{};light.id=info.hash;light.handle=handle;light.retiring=true;
    ownedLights.emplace(LightKey{2,uint32_t(i)},light);
  }
  const auto capCreates=creates;failDestroy=unsigned(maxOwnedLights);SyncLights(1);
  Check(creates==capCreates&&ownedLights.size()==maxOwnedLights&&Remaining()==maxOwnedLights&&destroys==maxOwnedLights);
  ClearLights();Check(ownedLights.empty()&&Remaining()==0);
  ResetFixture();SyncLights(1);++frameId;recordingApi.DrawLightInstance=nullptr;SyncLights(1);
  Check(draws==1&&!ignoredLegacy[0]&&Remaining()==1);recordingApi.DrawLightInstance=DrawApi;
  ResetFixture();failPolicy=1;SyncLights(1);Check(draws==0&&Remaining()==1&&!ignoredLegacy[0]);++frameId;SyncLights(1);Check(draws==1&&ignoredLegacy[0]);
  ResetFixture();Check(duplicateHashes==0);printf("{\"status\":\"PASS\",\"checks\":%u,\"issued\":%u,\"remaining\":%u,\"duplicateHashes\":%u,\"gpu\":false,\"nativeCode\":false}\n",checks,issued,Remaining(),duplicateHashes);return 0;
}
