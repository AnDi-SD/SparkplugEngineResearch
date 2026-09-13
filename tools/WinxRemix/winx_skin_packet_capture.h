#pragma once
#include "winx_skin_packet.h"
#include <tuple>
#include <io.h>
#include <fcntl.h>
namespace skin_packet_capture {
namespace packet=winx_remix::skin_packet;
static wchar_t directory[MAX_PATH]{};
static FILE* journal;
static uint32_t files,attempts,rejections;
static size_t writtenBytes;
static bool stopped;
using Identity=std::tuple<uint32_t,uint32_t,uint32_t,uint64_t>;
static std::map<Identity,unsigned> samples;
struct Saved {unsigned frame,file;uint32_t scene,skin,mesh;uint64_t call,submission,generation;};
static std::vector<Saved> saved;
static bool Enabled(){return directory[0]!=0;}
static bool Wanted(const native_skin_source::Observation& skin,uint64_t generation){
  if(!Enabled()||!journal||stopped)return false;
  const auto found=samples.find({skin.scene,skin.skin,skin.mesh,generation});
  return found==samples.end()?samples.size()<256:found->second<3;
}
static void Rejected(const native_skin_source::Observation& skin,unsigned reason){
  if(!Enabled()||!journal||stopped||_ftelli64(journal)>=1024*1024)return;
  ++rejections;
  fprintf(journal,"{\"event\":\"rejected\",\"frame\":%u,\"modelCall\":%llu,\"submission\":%llu,\"reason\":%u}\n",frameId,skin.modelCall,skin.submission,reason);fflush(journal);
}
static bool Save(const native_skin_source::Observation& skin,uint64_t generation,const packet::Packet& input){
  if(!Wanted(skin,generation))return false;
  std::vector<uint8_t> bytes;packet::Error error;
  if(!packet::Encode(input,bytes,&error)){Rejected(skin,100+unsigned(error));return false;}
  if(files>=64||writtenBytes+bytes.size()>32*1024*1024){
    stopped=true;fprintf(journal,"{\"event\":\"limit\",\"files\":%u,\"bytes\":%zu}\n",files,writtenBytes);fflush(journal);return false;
  }
  auto& sampleCount=samples.try_emplace(Identity{skin.scene,skin.skin,skin.mesh,generation},0).first->second;
  const auto ordinal=++attempts;wchar_t path[MAX_PATH]{};
  if(swprintf_s(path,L"%s/packet-%04u.skp",directory,ordinal)<0){Rejected(skin,200);stopped=true;return false;}
  const auto file=CreateFileW(path,GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(file==INVALID_HANDLE_VALUE){Rejected(skin,201);stopped=true;return false;}
  DWORD written=0;const bool complete=WriteFile(file,bytes.data(),DWORD(bytes.size()),&written,nullptr)&&written==bytes.size();
  const bool closed=CloseHandle(file)!=FALSE;
  if(!complete||!closed){Rejected(skin,202);stopped=true;return false;}
  ++files;writtenBytes+=bytes.size();++sampleCount;
  saved.push_back({frameId,ordinal,skin.scene,skin.skin,skin.mesh,skin.modelCall,skin.submission,generation});
  fprintf(journal,"{\"event\":\"packet\",\"file\":\"packet-%04u.skp\",\"frame\":%u,\"scene\":%u,\"skin\":%u,\"mesh\":%u,\"modelCall\":%llu,\"submission\":%llu,\"generation\":%llu,\"bytes\":%zu,\"vertices\":%zu,\"triangles\":%zu,\"influences\":%u,\"bones\":%zu,\"shaderVerified\":false,\"materialCaptured\":false}\n",
    ordinal,frameId,skin.scene,skin.skin,skin.mesh,skin.modelCall,skin.submission,generation,bytes.size(),input.vertices.size(),input.indices.size()/3,input.influences,input.palette.size());
  fflush(journal);return true;
}
static void Initialize(){
  wchar_t path[MAX_PATH]{};const auto size=GetEnvironmentVariableW(L"WINX_REMIX_NATIVE_SKIN_PACKETS",path,MAX_PATH);
  if(!size||size>=MAX_PATH-40||!native_skin_source::enabled)return;
  const auto attributes=GetFileAttributesW(path);
  if(attributes==INVALID_FILE_ATTRIBUTES||!(attributes&FILE_ATTRIBUTE_DIRECTORY))return;
  try {saved.reserve(64);}catch(...){return;}
  wchar_t log[MAX_PATH]{};if(swprintf_s(log,L"%s/packets.jsonl",path)<0)return;
  // The launcher owns a fresh directory; exclusive creation also rejects reuse.
  const auto file=CreateFileW(log,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(file==INVALID_HANDLE_VALUE)return;
  const auto descriptor=_open_osfhandle(reinterpret_cast<intptr_t>(file),_O_WRONLY|_O_TEXT);
  if(descriptor==-1){CloseHandle(file);return;}
  journal=_fdopen(descriptor,"w");if(!journal){_close(descriptor);return;}
  wcscpy_s(directory,path);
  fprintf(journal,"{\"event\":\"init\",\"schema\":1,\"pid\":%lu,\"maxFiles\":64,\"maxBytes\":33554432,\"maxSamplesPerIdentity\":3,\"scope\":\"owned native geometry and palette candidates; no selected-shader or material qualification, no submission\"}\n",GetCurrentProcessId());fflush(journal);
}
}
