// Own cross-architecture oracle for the real bridge material serializer.
#include <windows.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>
#include "util_remixapi.h"

namespace ledger {
thread_local bool armed=false;
thread_local size_t attempts=0,failAt=SIZE_MAX;
thread_local long live=0,mismatches=0;
struct alignas(std::max_align_t) Header {bool tracked,array;};
void* Allocate(size_t size,bool array){
  if(armed&&attempts++==failAt)throw std::bad_alloc();
  if(size>SIZE_MAX-sizeof(Header))throw std::bad_alloc();
  auto* h=static_cast<Header*>(std::malloc(sizeof(Header)+(size?size:1)));
  if(!h)throw std::bad_alloc();h->tracked=armed;h->array=array;if(armed)++live;return h+1;
}
void Free(void* p,bool array)noexcept{
  if(!p)return;auto* h=static_cast<Header*>(p)-1;
  if(h->tracked){--live;if(h->array!=array)++mismatches;}std::free(h);
}
}
void* operator new(size_t n){return ledger::Allocate(n,false);}
void* operator new[](size_t n){return ledger::Allocate(n,true);}
void operator delete(void* p)noexcept{ledger::Free(p,false);}
void operator delete[](void* p)noexcept{ledger::Free(p,true);}
void operator delete(void* p,size_t)noexcept{ledger::Free(p,false);}
void operator delete[](void* p,size_t)noexcept{ledger::Free(p,true);}

namespace {
using Info=remixapi_MaterialInfoOpaqueSubsurfaceEXT;
using Codec=remixapi::util::serialize::MaterialInfoOpaqueSubsurface;
using Bytes=std::vector<uint8_t>;
unsigned checks=0,errors=0;
void Check(bool ok,const char* what){++checks;if(!ok){++errors;std::fprintf(stderr,"FAIL %s\n",what);}}
void Need(bool ok,const char* what){Check(ok,what);if(!ok)throw std::runtime_error(what);}
template<class T>void Put(Bytes& b,const T& x){const auto* p=reinterpret_cast<const uint8_t*>(&x);b.insert(b.end(),p,p+sizeof(x));}
void Path(Bytes& b,const wchar_t* p){Put(b,uint8_t(p?0xff:0));if(p)for(size_t i=0;i<=wcslen(p);++i)Put(b,uint16_t(p[i]));}
Info Input(unsigned mask){
  Info v{};v.sType=REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_OPAQUE_SUBSURFACE_EXT;
  v.pNext=reinterpret_cast<void*>(uintptr_t(0x13579)); // Not part of the payload.
  v.subsurfaceTransmittanceTexture=(mask&1)?L"own/transmittance.dds":nullptr;
  v.subsurfaceThicknessTexture=(mask&2)?L"":nullptr;
  v.subsurfaceSingleScatteringAlbedoTexture=(mask&4)?L"own/scatter_\u03a9.dds":nullptr;
  v.subsurfaceRadiusTexture=(mask&8)?L"own/radius_\u0416.dds":nullptr;
  v.subsurfaceTransmittanceColor={.125f+mask,.25f,.5f};v.subsurfaceMeasurementDistance=.375f+mask;
  v.subsurfaceSingleScatteringAlbedo={.75f,.625f,-.125f};v.subsurfaceVolumetricAnisotropy=-.25f;
  v.subsurfaceDiffusionProfile=1;v.subsurfaceRadius={.125f,.375f,2.5f+mask};
  v.subsurfaceRadiusScale=.875f;v.subsurfaceMaxSampleRadius=4.25f+mask;return v;
}
Bytes Oracle(const Info& v,bool extended){
  static_assert(sizeof(float)==4&&sizeof(wchar_t)==2&&sizeof(remixapi_Bool)==4);
  Bytes b;Put(b,uint32_t(0));Put(b,uint32_t(v.sType));
  Path(b,v.subsurfaceTransmittanceTexture);Path(b,v.subsurfaceThicknessTexture);Path(b,v.subsurfaceSingleScatteringAlbedoTexture);
  Put(b,v.subsurfaceTransmittanceColor.x);Put(b,v.subsurfaceTransmittanceColor.y);Put(b,v.subsurfaceTransmittanceColor.z);
  Put(b,v.subsurfaceMeasurementDistance);
  Put(b,v.subsurfaceSingleScatteringAlbedo.x);Put(b,v.subsurfaceSingleScatteringAlbedo.y);Put(b,v.subsurfaceSingleScatteringAlbedo.z);
  Put(b,v.subsurfaceVolumetricAnisotropy);
  if(extended){Put(b,uint32_t(v.subsurfaceDiffusionProfile));Put(b,v.subsurfaceRadius.x);Put(b,v.subsurfaceRadius.y);Put(b,v.subsurfaceRadius.z);
    Put(b,v.subsurfaceRadiusScale);Put(b,v.subsurfaceMaxSampleRadius);Path(b,v.subsurfaceRadiusTexture);}
  const auto size=static_cast<uint32_t>(b.size());std::memcpy(b.data(),&size,4);return b;
}
void Equal(const Info& actual,const Info& want){
  Check(actual.sType==want.sType&&actual.pNext==nullptr,"decoded structure/chain");
#define FIELD(name) Check(std::memcmp(&actual.name,&want.name,sizeof(want.name))==0,#name)
  FIELD(subsurfaceTransmittanceColor);FIELD(subsurfaceMeasurementDistance);FIELD(subsurfaceSingleScatteringAlbedo);
  FIELD(subsurfaceVolumetricAnisotropy);FIELD(subsurfaceDiffusionProfile);FIELD(subsurfaceRadius);
  FIELD(subsurfaceRadiusScale);FIELD(subsurfaceMaxSampleRadius);
#undef FIELD
  const wchar_t* a[]={actual.subsurfaceTransmittanceTexture,actual.subsurfaceThicknessTexture,actual.subsurfaceSingleScatteringAlbedoTexture,actual.subsurfaceRadiusTexture};
  const wchar_t* w[]={want.subsurfaceTransmittanceTexture,want.subsurfaceThicknessTexture,want.subsurfaceSingleScatteringAlbedoTexture,want.subsurfaceRadiusTexture};
  for(unsigned i=0;i<4;++i){Check(bool(a[i])==bool(w[i]),"path presence");if(a[i]&&w[i])Check(a[i]!=w[i]&&wcscmp(a[i],w[i])==0,"owned UTF16 path");}
}
void Decode(Bytes& bytes,const Info& want){
  ledger::attempts=0;ledger::failAt=SIZE_MAX;ledger::armed=true;
  {Codec decoded(bytes.data());decoded.deserialize();Equal(decoded,want);}
  ledger::armed=false;Check(ledger::live==0&&ledger::mismatches==0,"balanced typed array ownership");
}
void Faults(){
  auto want=Input(15);auto bytes=Oracle(want,true);
  for(size_t failure=0;failure<=4;++failure){
    ledger::attempts=0;ledger::failAt=failure;bool threw=false;ledger::armed=true;
    try{Codec decoded(bytes.data());decoded.deserialize();Equal(decoded,want);}
    catch(const std::bad_alloc&){threw=true;}
    ledger::armed=false;
    Check(threw==(failure<4),"each path allocation failure reached");
    Check(ledger::live==0&&ledger::mismatches==0,"partial decode cleanup");
  }
  ledger::failAt=SIZE_MAX;
}
}
int main(int argc,char** argv){
  try{
    Need(argc==3,"mode and payload required");const bool baseline=std::strcmp(argv[1],"--baseline")==0;
    const bool writing=baseline||std::strcmp(argv[1],"--write")==0;
    Need(writing||std::strcmp(argv[1],"--read")==0,"supported mode");
    FILE* file=nullptr;Need(fopen_s(&file,argv[2],writing?"wb":"rb")==0&&file,"payload file");
    for(unsigned mask=0;mask<16;++mask){
      const auto want=Input(mask);const auto expected=Oracle(want,!baseline);Bytes bytes;
      if(writing){
        Codec encoder(want);Need(encoder.size()==expected.size(),"independent payload size");
        bytes.resize(encoder.size()+32,0xa7);encoder.serialize(bytes.data());
        Check(std::all_of(bytes.begin()+encoder.size(),bytes.end(),[](uint8_t x){return x==0xa7;}),"serializer canary");
        bytes.resize(encoder.size());Need(bytes==expected,"independent payload bytes");
        Need(fwrite(bytes.data(),1,bytes.size(),file)==bytes.size(),"write full record");
        if(baseline)Check(bytes!=Oracle(want,true),"baseline omits complete extension tail");
      }else{
        uint32_t size=0;Need(fread(&size,1,4,file)==4,"read record header");Need(size==expected.size(),"reader oracle size");
        bytes.resize(size);std::memcpy(bytes.data(),&size,4);Need(fread(bytes.data()+4,1,size-4,file)==size-4,"read full record");
        Need(bytes==expected,"cross-architecture bytes");
      }
      if(!baseline)Decode(bytes,want);
    }
    if(!writing)Check(fgetc(file)==EOF,"no trailing records");Need(fclose(file)==0,"close payload");
    if(!baseline)Faults();
    std::printf("{\"mode\":\"%s\",\"checks\":%u,\"errors\":%u,\"remainingAllocations\":%ld,\"wrongDeleteForms\":%ld}\n",argv[1],checks,errors,ledger::live,ledger::mismatches);
    return errors?1:0;
  }catch(const std::exception& e){ledger::armed=false;std::fprintf(stderr,"ERROR %s\n",e.what());return 2;}
}
