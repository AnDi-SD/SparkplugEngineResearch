// Own read-only material boundary audit. It does not reconstruct light selection,
// invent ambient lamps, change shader constants or substitute unknown semantics.
namespace material_audit {
struct Constant { std::string name; unsigned first=0,count=0; };
struct Reflection { bool valid=false; std::vector<Constant> colors; };
static uint32_t Word(const uint8_t* p) { uint32_t v;memcpy(&v,p,4);return v; }
static uint16_t Half(const uint8_t* p) { uint16_t v;memcpy(&v,p,2);return v; }

static Reflection ReflectColors(const std::vector<uint8_t>& bytes) {
  if(bytes.size()<8 || bytes.size()%4 || bytes.size()>1024*1024) return {};
  const uint32_t version=Word(bytes.data());
  if((version>>16)!=0xfffe) return {}; // vertex shader only
  for(size_t i=4;i+8<=bytes.size();i+=4) {
    const auto token=Word(bytes.data()+i);
    if((token&0xffff)!=0xfffe || Word(bytes.data()+i+4)!=0x42415443) continue;
    const size_t words=(token>>16)&0x7fff;
    if(words<8 || words>(bytes.size()-i-4)/4) return {};
    const uint8_t* table=bytes.data()+i+8;const size_t size=(words-1)*4;
    if(Word(table)!=28 || Word(table+8)!=version) return {};
    const auto count=Word(table+12), offset=Word(table+16);
    if(count>256 || offset>size || uint64_t(count)*20>size-offset) return {};
    Reflection result;std::set<std::string> names;
    for(unsigned j=0;j<count;++j) {
      const uint8_t* entry=table+offset+j*20;
      const auto nameOffset=Word(entry),typeOffset=Word(entry+12);
      if(nameOffset>=size || typeOffset>size || size-typeOffset<16) return {};
      const auto end=static_cast<const uint8_t*>(memchr(table+nameOffset,0,(std::min)(size-nameOffset,size_t(128))));
      if(!end) return {};
      const std::string name(reinterpret_cast<const char*>(table+nameOffset),size_t(end-table-nameOffset));
      if(!names.insert(name).second) return {};
      // CTAB names are shader metadata, not an asset hash allowlist. Only these
      // known color inputs are reported; all other shader behavior stays unknown.
      if(name!="AmbientCol" && name!="MatDiffuse" && name!="MatSpecular" && name!="ConstColor" &&
         name!="LightAmbientColorDir0" && name!="LightDiffuseColorDir0") continue;
      const unsigned first=Half(entry+6), registers=Half(entry+8);
      const auto type=table+typeOffset;
      if(Half(entry+4)!=2 || registers!=1 || first>=256 || Half(type)!=1 || Half(type+2)!=3 ||
         Half(type+4)!=1 || Half(type+6)!=4 || Half(type+8)>1 || Half(type+10)!=0) return {};
      result.colors.push_back({name,first,registers});
    }
    result.valid=true;return result;
  }
  return {}; // no CTAB is not proof that the shader has no lighting
}

static FILE* output;
static wchar_t outputPath[MAX_PATH]{};
static std::map<std::vector<uint8_t>,std::pair<unsigned,Reflection>> shaders;
static size_t shaderBytes;
static std::map<std::string,unsigned> inputs;
static unsigned samples, failures, rejected, nextShader;
static void Initialize() {
  const auto length=GetEnvironmentVariableW(L"WINX_REMIX_MATERIAL_AUDIT",outputPath,MAX_PATH);
  if(!length || length>=MAX_PATH-40) return;
  output=_wfsopen(outputPath,L"wb",_SH_DENYNO);
  if(output) {fputs("{\"event\":\"init\",\"maxBytes\":16777216,\"maxInputs\":1024,\"maxShaderBytes\":1048576,\"scope\":\"sampled original draw inputs; no GPU output or inferred ambient lamp\"}\n",output);fflush(output);}
}
static void Color(std::ostringstream& out,const D3DCOLORVALUE& color) {
  out<<'[';const float values[]={color.r,color.g,color.b,color.a};
  for(unsigned i=0;i<4;++i) {if(i)out<<',';if(std::isfinite(values[i]))out<<values[i];else out<<"null";}
  out<<']';
}
static void Draw(IDirect3DDevice9* d,const char* call) {
  if(!output || (frameId>1 && frameId%300 && !triggered && frameId>=traceUntilFrame)) return;
  if(_ftelli64(output)>=16*1024*1024) return;
  ++samples;
  // Get state at draw, including StateBlock changes. Querying never changes it.
  static const D3DRENDERSTATETYPE keys[]={D3DRS_LIGHTING,D3DRS_AMBIENT,D3DRS_COLORVERTEX,
    D3DRS_DIFFUSEMATERIALSOURCE,D3DRS_AMBIENTMATERIALSOURCE,D3DRS_EMISSIVEMATERIALSOURCE,
    D3DRS_SPECULARMATERIALSOURCE,D3DRS_SPECULARENABLE,D3DRS_TEXTUREFACTOR,
    D3DRS_ZENABLE,D3DRS_ZWRITEENABLE,D3DRS_ALPHABLENDENABLE,D3DRS_ALPHATESTENABLE,
    D3DRS_ALPHAFUNC,D3DRS_ALPHAREF};
  DWORD values[sizeof(keys)/sizeof(keys[0])]{};
  D3DMATERIAL9 material{};D3DMATRIX projection{};
  if(FAILED(d->GetMaterial(&material)) || FAILED(d->GetTransform(D3DTS_PROJECTION,&projection))) {++failures;return;}
  for(unsigned i=0;i<sizeof(keys)/sizeof(keys[0]);++i)
    if(FAILED(d->GetRenderState(keys[i],&values[i]))) {++failures;return;}
  std::ostringstream out;out.precision(9);
  IDirect3DSurface9* target=nullptr;
  if(FAILED(d->GetRenderTarget(0,&target)) || !target) {++failures;return;}
  const auto primary=primaryTargets.find(d);const bool primaryTarget=primary!=primaryTargets.end() && primary->second==target;
  target->Release();
  IDirect3DVertexDeclaration9* declaration=nullptr;
  if(FAILED(d->GetVertexDeclaration(&declaration))) {++failures;return;}
  DWORD channels=0;
  if(declaration) {
    D3DVERTEXELEMENT9 elements[MAXD3DDECLLENGTH+1]{};UINT count=MAXD3DDECLLENGTH+1;
    const auto hr=declaration->GetDeclaration(elements,&count);declaration->Release();
    if(FAILED(hr) || count>MAXD3DDECLLENGTH+1) {++failures;return;}
    for(unsigned i=0;i<count && elements[i].Stream!=0xff;++i) {
      const auto& element=elements[i];
      if(element.Usage==D3DDECLUSAGE_COLOR && element.UsageIndex<2)channels|=1u<<element.UsageIndex;
      if(element.Usage==D3DDECLUSAGE_NORMAL)channels|=4;
      if(element.Usage==D3DDECLUSAGE_POSITIONT)channels|=8;
    }
  }
  out<<"{\"perspective\":"<<(projection._34!=0?"true":"false")<<",\"primaryTarget\":"<<(primaryTarget?"true":"false")<<",\"channels\":"<<channels<<",\"states\":[";
  for(unsigned i=0;i<sizeof(keys)/sizeof(keys[0]);++i) {if(i)out<<',';out<<'['<<keys[i]<<','<<values[i]<<']';}
  out<<"],\"material\":{\"diffuse\":";Color(out,material.Diffuse);
  out<<",\"ambient\":";Color(out,material.Ambient);out<<",\"emissive\":";Color(out,material.Emissive);
  out<<",\"specular\":";Color(out,material.Specular);out<<",\"power\":";
  if(std::isfinite(material.Power))out<<material.Power;else out<<"null";
  out<<"},\"stages\":[";
  static const D3DTEXTURESTAGESTATETYPE stageKeys[]={D3DTSS_COLOROP,D3DTSS_COLORARG1,D3DTSS_COLORARG2,
    D3DTSS_ALPHAOP,D3DTSS_ALPHAARG1,D3DTSS_ALPHAARG2,D3DTSS_TEXCOORDINDEX,D3DTSS_TEXTURETRANSFORMFLAGS};
  for(unsigned stage=0;stage<8;++stage) {
    if(stage)out<<',';out<<'[';
    for(unsigned i=0;i<8;++i) {
      DWORD value=0;if(FAILED(d->GetTextureStageState(stage,stageKeys[i],&value))) {++failures;return;}
      if(i)out<<',';out<<value;
    }
    out<<']';
  }
  IDirect3DVertexShader9* vs=nullptr;IDirect3DPixelShader9* ps=nullptr;
  const auto vhr=d->GetVertexShader(&vs),phr=d->GetPixelShader(&ps);
  struct Release {IDirect3DVertexShader9* vs;IDirect3DPixelShader9* ps;~Release(){if(vs)vs->Release();if(ps)ps->Release();}} release{vs,ps};
  if(FAILED(vhr) || FAILED(phr)) {++failures;return;}
  out<<"],\"pixelShader\":"<<(ps?"true":"false")<<",\"vertexShader\":";
  if(!vs)out<<"null";
  else {
    UINT size=0;
    if(FAILED(vs->GetFunction(nullptr,&size)) || size<8 || size>1024*1024) {++failures;return;}
    std::vector<uint8_t> bytes(size);
    if(FAILED(vs->GetFunction(bytes.data(),&size)) || size!=bytes.size()) {++failures;return;}
    auto found=shaders.find(bytes);
    if(found==shaders.end()) {
      if(shaders.size()>=256 || shaderBytes+size>1024*1024) {++rejected;return;}
      const auto reflection=ReflectColors(bytes);
      shaderBytes+=size;found=shaders.emplace(std::move(bytes),std::make_pair(++nextShader,reflection)).first;
      const auto hash=XXH3_64bits(found->first.data(),found->first.size());
      wchar_t path[MAX_PATH]{};swprintf_s(path,L"%s.shader-%04u.bin",outputPath,found->second.first);
      FILE* file=nullptr;_wfopen_s(&file,path,L"wb");bool dumped=false;
      if(file) {dumped=fwrite(found->first.data(),1,size,file)==size;if(fclose(file)!=0)dumped=false;}
      if(!dumped)++failures;
      fprintf(output,"{\"event\":\"shader\",\"id\":%u,\"bytecodeHash\":\"%016llX\",\"bytes\":%u,\"reflectionValid\":%s,\"dumped\":%s}\n",found->second.first,hash,size,reflection.valid?"true":"false",dumped?"true":"false");
    }
    const auto& reflection=found->second.second;
    out<<"{\"id\":"<<found->second.first<<",\"reflectionValid\":"<<(reflection.valid?"true":"false")<<",\"colors\": [";
    bool first=true;
    for(const auto& constant:reflection.colors) {
      D3DCOLORVALUE color{};if(FAILED(d->GetVertexShaderConstantF(constant.first,&color.r,1))) {++failures;return;}
      if(!first)out<<',';first=false;
      out<<"{\"name\":\""<<constant.name<<"\",\"register\":"<<constant.first<<",\"rgba\":";
      Color(out,color);out<<'}';
    }
    out<<"]}";
  }
  out<<'}';const auto key=out.str();auto found=inputs.find(key);
  if(found==inputs.end()) {
    if(inputs.size()>=1024) {++rejected;return;}
    const auto id=static_cast<unsigned>(inputs.size()+1);found=inputs.emplace(key,id).first;
    fprintf(output,"{\"event\":\"input\",\"id\":%u,\"frame\":%u,\"draw\":%u,\"call\":\"%s\",\"data\":%s}\n",id,frameId,drawId,call,key.c_str());
  }
  fprintf(output,"{\"event\":\"sample\",\"frame\":%u,\"draw\":%u,\"input\":%u,\"call\":\"%s\"}\n",frameId,drawId,found->second,call);
}
static void EndFrame() {
  if(!output || (!samples && !failures && !rejected) || _ftelli64(output)>=16*1024*1024) return;
  fprintf(output,"{\"event\":\"frame\",\"frame\":%u,\"samples\":%u,\"failures\":%u,\"rejected\":%u,\"inputs\":%zu,\"shaders\":%zu}\n",frameId,samples,failures,rejected,inputs.size(),shaders.size());
  fflush(output);samples=failures=rejected=0;
}
} // namespace material_audit
