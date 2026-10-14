#pragma once
// Own bounded reader of the already owned, verified scene light record.
// The arithmetic is shared recovered PC4B53C0, not a second light formula.
#include "winx_light_payload_source.h"
#define WINX_REMIX_NATIVE_LIGHT_SOURCE_AVAILABLE 1
namespace native_light_source {
static bool enabled,submitEnabled,keepForComparison;
static FILE* output;
static unsigned attempts,matched,used,mismatches,dirty,invalid;
#if defined(_M_IX86)
static bool Resolve(const scene_audit::LightRecord& record,D3DLIGHT9& resolved) {
  if(!enabled)return false;
  ++attempts;
  using Source=winx_remix::LightPayloadSource;
  const auto evaluation=Source{}.Evaluate(record);
  if(evaluation.status==Source::Status::Invalid){++invalid;return false;}
  // Preserve original scheduling. A candidate from cached world fields does
  // not establish producer freshness and never permits clearing game flags.
  if(evaluation.status==Source::Status::Dirty){++dirty;return false;}
  const auto type=evaluation.type,mask=evaluation.consumedMask,different=evaluation.differenceMask;
  if(output&&_ftelli64(output)<16*1024*1024&&(different||frameId%300==0||triggered))
    fprintf(output,"{\"event\":\"compare\",\"frame\":%u,\"scene\":%u,\"object\":%u,\"type\":%u,\"consumedMask\":%u,\"differenceMask\":%u,\"source\":\"spLight_fields_world_shared_PC4B53C0\"}\n",
      frameId,scene_audit::At(record.raw,0x3c),record.address,type,mask,different);
  if(different){++mismatches;return false;}
  ++matched;
  if(!submitEnabled||keepForComparison)return false;
  Source::Payload actual{};
  if(!evaluation.CopyCachedEquivalentPayload(actual))return false;
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
