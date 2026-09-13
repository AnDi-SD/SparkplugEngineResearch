// Own optional D3D9 boundary diagnostics, not reconstructed game logic.
// Included after Original/Patch in the probe translation unit.
#include <vector>
#include <cstring>
#include <limits>

static FILE* shaderAuditFile;
static wchar_t shaderAuditPath[MAX_PATH]{};
static std::map<std::vector<unsigned char>,unsigned> shaderBytecodes;
static std::map<void*,unsigned> shaderIdentities;
static size_t shaderAuditBytes;
static unsigned shaderSequence;
static unsigned long long shaderCreates[2]{},shaderCreateFailures[2]{},shaderBinds[2]{},shaderBindFailures[2]{};
static unsigned long long shaderConstants[6]{},shaderConstantFailures[6]{},shaderQueryFailures,shaderReadFailures,shaderDumpFailures;
static unsigned long long shaderDrawFailures,shaderInputMismatches;
static std::map<std::pair<unsigned,unsigned>,unsigned long long> shaderDrawTotals,shaderFrameDraws;
static constexpr unsigned unknownShader=(std::numeric_limits<unsigned>::max)();

static void InitializeShaderAudit() {
  const DWORD length=GetEnvironmentVariableW(L"WINX_REMIX_SHADER_AUDIT",shaderAuditPath,MAX_PATH);
  if(!length || length>=MAX_PATH-40) { shaderAuditPath[0]=0; return; }
  wchar_t file[MAX_PATH]{};
  swprintf_s(file,L"%s\\audit.jsonl",shaderAuditPath);
  shaderAuditFile=_wfsopen(file,L"wb",_SH_DENYNO);
  if(shaderAuditFile) {
    fprintf(shaderAuditFile,"{\"event\":\"init\",\"pid\":%lu,\"pointerBytes\":%zu}\n",GetCurrentProcessId(),sizeof(void*));
    fflush(shaderAuditFile);
  }
}

template<class Shader> static unsigned RememberShader(Shader* shader,bool created=false,const DWORD* input=nullptr) {
  if(!shader) return 0;
  const auto found=shaderIdentities.find(shader);
  if(!created && found!=shaderIdentities.end()) return found->second;
  // A new Create call refreshes identity even if a released address is reused.
  UINT size=0;
  HRESULT hr=shader->GetFunction(nullptr,&size);
  if(FAILED(hr) || size<8 || size>1024*1024 || size%4) { ++shaderReadFailures; return unknownShader; }
  std::vector<unsigned char> bytes(size);
  hr=shader->GetFunction(bytes.data(),&size);
  if(FAILED(hr) || size!=bytes.size()) { ++shaderReadFailures; return unknownShader; }
  if(input && std::memcmp(input,bytes.data(),size)!=0) ++shaderInputMismatches;
  auto entry=shaderBytecodes.find(bytes);
  if(entry==shaderBytecodes.end()) {
    if(shaderBytecodes.size()>=4096 || shaderAuditBytes+size>16*1024*1024) { ++shaderReadFailures; return unknownShader; }
    const unsigned id=++shaderSequence;
    DWORD version=0; std::memcpy(&version,bytes.data(),sizeof(version));
    wchar_t path[MAX_PATH]{}; swprintf_s(path,L"%s\\shader-%04u.bin",shaderAuditPath,id);
    FILE* file=nullptr; _wfopen_s(&file,path,L"wb");
    bool dumped=false;
    if(file) { dumped=fwrite(bytes.data(),1,size,file)==size; if(fclose(file)!=0) dumped=false; }
    if(!dumped) ++shaderDumpFailures;
    fprintf(shaderAuditFile,"{\"event\":\"shader\",\"frame\":%u,\"id\":%u,\"stage\":\"%s\",\"major\":%lu,\"minor\":%lu,\"bytes\":%u,\"dumped\":%s}\n",
      frameId,id,(version>>16)==0xfffe?"vs":"ps",(version>>8)&255,version&255,size,dumped?"true":"false");
    shaderAuditBytes+=size;
    entry=shaderBytecodes.emplace(std::move(bytes),id).first;
    fflush(shaderAuditFile);
  }
  if(shaderIdentities.size()<16384 || found!=shaderIdentities.end()) shaderIdentities[shader]=entry->second;
  return entry->second;
}

template<class Shader,unsigned Slot> static HRESULT STDMETHODCALLTYPE AuditCreateShader(IDirect3DDevice9* d,const DWORD* code,Shader** result) {
  using F=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,const DWORD*,Shader**);
  const HRESULT hr=Original<F>(d,Slot)(d,code,result);
  std::lock_guard<std::recursive_mutex> lock(guard);
  constexpr unsigned stage=Slot==91?0:1;
  ++shaderCreates[stage]; if(FAILED(hr)) ++shaderCreateFailures[stage];
  const unsigned id=SUCCEEDED(hr) && result && *result?RememberShader(*result,true,code):unknownShader;
  fprintf(shaderAuditFile,"{\"event\":\"create\",\"frame\":%u,\"stage\":\"%s\",\"id\":%u,\"hr\":%ld}\n",frameId,stage?"ps":"vs",id,hr);
  fflush(shaderAuditFile);
  return hr;
}

template<class Shader,unsigned Slot> static HRESULT STDMETHODCALLTYPE AuditSetShader(IDirect3DDevice9* d,Shader* shader) {
  d3d9_state_witness::Mutation mutation;
  using F=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,Shader*);
  const HRESULT hr=Original<F>(d,Slot)(d,shader);
  std::lock_guard<std::recursive_mutex> lock(guard);
  constexpr unsigned stage=Slot==92?0:1;
  ++shaderBinds[stage]; if(FAILED(hr)) ++shaderBindFailures[stage];
  // Draw-time GetShader queries also cover state-block Apply and SetFVF changes.
  return hr;
}

template<class T,unsigned Slot,unsigned Index> static HRESULT STDMETHODCALLTYPE AuditSetConstants(IDirect3DDevice9* d,UINT start,const T* data,UINT count) {
  using F=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,UINT,const T*,UINT);
  const HRESULT hr=Original<F>(d,Slot)(d,start,data,count);
  std::lock_guard<std::recursive_mutex> lock(guard);
  ++shaderConstants[Index]; if(FAILED(hr)) ++shaderConstantFailures[Index];
  return hr;
}

static void AuditShaderDraw(IDirect3DDevice9* d) {
  if(!shaderAuditFile) return;
  std::lock_guard<std::recursive_mutex> lock(guard);
  IDirect3DVertexShader9* vs=nullptr; IDirect3DPixelShader9* ps=nullptr;
  const HRESULT vhr=d->GetVertexShader(&vs),phr=d->GetPixelShader(&ps);
  if(FAILED(vhr)) ++shaderQueryFailures;
  if(FAILED(phr)) ++shaderQueryFailures;
  const unsigned vi=SUCCEEDED(vhr)?RememberShader(vs):unknownShader;
  const unsigned pi=SUCCEEDED(phr)?RememberShader(ps):unknownShader;
  if(vs) vs->Release(); if(ps) ps->Release();
  ++shaderDrawTotals[{vi,pi}]; ++shaderFrameDraws[{vi,pi}];
}

static HRESULT AuditShaderDrawResult(HRESULT hr) {
  if(shaderAuditFile && FAILED(hr)) { std::lock_guard<std::recursive_mutex> lock(guard); ++shaderDrawFailures; }
  return hr;
}

static void ShaderAuditSnapshot(bool force=false) {
  if(!shaderAuditFile) return;
  std::lock_guard<std::recursive_mutex> lock(guard);
  static ULONGLONG next=0; const auto now=GetTickCount64();
  if(force || now>=next) {
    next=now+2000;
    fprintf(shaderAuditFile,"{\"event\":\"snapshot\",\"frame\":%u,\"tick\":%llu,\"unique\":%u,\"create\":[%llu,%llu],\"createFailures\":[%llu,%llu],\"bind\":[%llu,%llu],\"bindFailures\":[%llu,%llu],\"queryFailures\":%llu,\"readFailures\":%llu,\"dumpFailures\":%llu,\"drawFailures\":%llu,\"inputMismatches\":%llu",
      frameId,now,shaderSequence,shaderCreates[0],shaderCreates[1],shaderCreateFailures[0],shaderCreateFailures[1],shaderBinds[0],shaderBinds[1],shaderBindFailures[0],shaderBindFailures[1],shaderQueryFailures,shaderReadFailures,shaderDumpFailures,shaderDrawFailures,shaderInputMismatches);
    fputs(",\"constants\":[",shaderAuditFile);
    for(unsigned i=0;i<6;++i) fprintf(shaderAuditFile,"%s%llu",i?",":"",shaderConstants[i]);
    fputs("],\"constantFailures\":[",shaderAuditFile);
    for(unsigned i=0;i<6;++i) fprintf(shaderAuditFile,"%s%llu",i?",":"",shaderConstantFailures[i]);
    for(unsigned pass=0;pass<2;++pass) {
      fprintf(shaderAuditFile,"],\"%s\":[",pass?"frameDraws":"totalDraws");
      bool first=true;
      for(const auto& pair:pass?shaderFrameDraws:shaderDrawTotals) {
        fprintf(shaderAuditFile,"%s[%u,%u,%llu]",first?"":",",pair.first.first,pair.first.second,pair.second); first=false;
      }
    }
    fputs("]}\n",shaderAuditFile); fflush(shaderAuditFile);
  }
  shaderFrameDraws.clear();
}

static void InstallShaderAudit(IDirect3DDevice9* d) {
  if(!shaderAuditFile) return;
  Patch(d,91,reinterpret_cast<void*>(AuditCreateShader<IDirect3DVertexShader9,91>));
  Patch(d,92,reinterpret_cast<void*>(AuditSetShader<IDirect3DVertexShader9,92>));
  Patch(d,106,reinterpret_cast<void*>(AuditCreateShader<IDirect3DPixelShader9,106>));
  Patch(d,107,reinterpret_cast<void*>(AuditSetShader<IDirect3DPixelShader9,107>));
  Patch(d,94,reinterpret_cast<void*>(AuditSetConstants<float,94,0>));
  Patch(d,96,reinterpret_cast<void*>(AuditSetConstants<int,96,1>));
  Patch(d,98,reinterpret_cast<void*>(AuditSetConstants<BOOL,98,2>));
  Patch(d,109,reinterpret_cast<void*>(AuditSetConstants<float,109,3>));
  Patch(d,111,reinterpret_cast<void*>(AuditSetConstants<int,111,4>));
  Patch(d,113,reinterpret_cast<void*>(AuditSetConstants<BOOL,113,5>));
}
