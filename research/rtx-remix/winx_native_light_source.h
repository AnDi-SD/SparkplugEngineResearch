#pragma once
// Own bounded reader of the already owned, verified scene light record.
// The arithmetic is shared recovered PC4B53C0, not a second light formula.
#include "../../Sparkplug/Code/SparkplugDX/spPCLightPayload.h"
#define WINX_REMIX_NATIVE_LIGHT_SOURCE_AVAILABLE 1
namespace native_light_source {
static bool enabled,submitEnabled,keepForComparison;
static FILE* output;
static unsigned attempts,matched,used,mismatches,dirty,invalid;
#if defined(_M_IX86)
using Inputs=sparkplug::reconstruction::PCLightPayloadInputsForAnalysis;
static uint32_t ConsumedMask(uint32_t type) {
  uint32_t mask=0xFu; // Type and diffuse RGB. Diffuse alpha is not consumed.
  if(type!=1)mask|=7u<<16; // World direction, normalized by the RT policy.
  if(type!=0)mask|=(7u<<13)|(1u<<19)|(7u<<21);
  if(type==2)mask|=(1u<<20)|(3u<<24);
  return mask;
}
static bool Resolve(const scene_audit::LightRecord& record,D3DLIGHT9& resolved) {
  if(!enabled)return false;
  ++attempts;const auto raw=record.raw;const auto type=scene_audit::At(raw,0xc0);
  if(!record.address||scene_audit::At(raw,0)!=0x6f0c88||!scene_audit::At(raw,0x3c)||
     type>2||raw[0xd4]>1||raw[0xed]!=1||!(scene_audit::At(raw,0xb0)&0x100)) {++invalid;return false;}
  // A marker can precede a native producer. Do not clear it or refresh the
  // game's payload; retain its observable old value through the old path.
  if(scene_audit::At(raw,0xb0)&0xfu){++dirty;return false;}
  Inputs input{};input.kind=type;input.attenuationEnabled=raw[0xd4]!=0;
  memcpy(input.color.data(),raw+0xc4,16);memcpy(input.position.data(),raw+0x74,12);
  memcpy(input.direction.data(),raw+0xa4,12);memcpy(&input.intensity,raw+0xd8,4);
  memcpy(&input.range,raw+0xe0,4);memcpy(&input.hotspot,raw+0xe4,4);memcpy(&input.falloff,raw+0xe8,4);
  // The original globals feed only fields ignored by the current physical
  // converter. Zero explicit scratch inputs are NOT inferred live globals.
  std::array<uint32_t,26> computed{},actual{};
  memcpy(actual.data(),raw+0xf0,sizeof(actual));
  if(!sparkplug::reconstruction::RefreshPCLightPayloadForAnalysis(input,computed)){++invalid;return false;}
  const uint32_t mask=ConsumedMask(type);uint32_t different=0;
  for(unsigned i=0;i<26;++i)if((mask&(1u<<i))&&computed[i]!=actual[i])different|=1u<<i;
  if(output&&_ftelli64(output)<16*1024*1024&&(different||frameId%300==0||triggered))
    fprintf(output,"{\"event\":\"compare\",\"frame\":%u,\"scene\":%u,\"object\":%u,\"type\":%u,\"consumedMask\":%u,\"differenceMask\":%u,\"source\":\"spLight_fields_world_shared_PC4B53C0\"}\n",
      frameId,scene_audit::At(raw,0x3c),record.address,type,mask,different);
  if(different){++mismatches;return false;}
  ++matched;
  if(!submitEnabled||keepForComparison)return false;
  // Only qualified consumed words change source. Unused payload words are
  // preserved, including point.direction currently present in cache memcmp.
  for(unsigned i=0;i<26;++i)if(mask&(1u<<i))actual[i]=computed[i];
  memcpy(&resolved,actual.data(),sizeof(resolved));++used;return true;
}
#endif
static void Initialize() {
  wchar_t path[MAX_PATH]{},option[8]{};
  const auto length=GetEnvironmentVariableW(L"WINX_REMIX_NATIVE_LIGHT_SOURCE",path,MAX_PATH);
  if(!length||length>=MAX_PATH||!sceneLightsEnabled)return;
  enabled=sizeof(void*)==4;output=_wfsopen(path,L"wb",_SH_DENYNO);
  submitEnabled=enabled&&GetEnvironmentVariableW(L"WINX_REMIX_NATIVE_LIGHT_SUBMIT",option,8)&&wcscmp(option,L"1")==0;
  if(output){fprintf(output,"{\"event\":\"init\",\"schema\":1,\"enabled\":%s,\"submit\":%s,\"pid\":%lu,\"maxLogBytes\":16777216,\"scope\":\"verified current registry; consumed light fields only; exact payload comparison; original update scheduling\"}\n",
    enabled?"true":"false",submitEnabled?"true":"false",GetCurrentProcessId());fflush(output);}
}
static void EndFrame() {
  if(output&&_ftelli64(output)<16*1024*1024&&(attempts||frameId%300==0)) {
    fprintf(output,"{\"event\":\"frame\",\"frame\":%u,\"attempts\":%u,\"matched\":%u,\"used\":%u,\"mismatches\":%u,\"dirty\":%u,\"invalid\":%u,\"comparison\":%s}\n",
      frameId,attempts,matched,used,mismatches,dirty,invalid,keepForComparison?"true":"false");fflush(output);
  }
  attempts=matched=used=mismatches=dirty=invalid=0;
}
}
