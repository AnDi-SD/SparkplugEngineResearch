// Own one-frame CPU geometry observation for light visibility diagnostics.
// Does not use USD capture, modify the game, or change any draw result.
#pragma once
static FILE* geometryProbeFile;
static bool geometryProbeAttempted;
static unsigned geometryProbeDraws,geometryProbeTriangles;
static size_t geometryProbeBytes;
static bool geometryProbeLimited;
static bool GeometryProbeRequested() {
  if(geometryProbeAttempted)return geometryProbeFile!=nullptr;
  static const bool requested=[](){wchar_t path[MAX_PATH]{};return GetEnvironmentVariableW(L"WINX_REMIX_GEOMETRY_PROBE",path,MAX_PATH)!=0;}();
  return requested;
}
static void RecordGeometryProbe(const std::vector<remixapi_HardcodedVertex>& vertices,const D3DMATRIX& world,uint64_t hash,DWORD cull) {
  if(!geometryProbeAttempted) {
    geometryProbeAttempted=true;wchar_t path[MAX_PATH]{};
    const auto length=GetEnvironmentVariableW(L"WINX_REMIX_GEOMETRY_PROBE",path,MAX_PATH);
    if(!length||length>=MAX_PATH)return;
    if(GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES)return;
    if(_wfopen_s(&geometryProbeFile,path,L"wb"))return;
    const uint32_t header[]={0x57475031,frameId};
    if(fwrite(header,sizeof(header),1,geometryProbeFile)!=1)geometryProbeLimited=true;
    geometryProbeBytes=sizeof(header);
  }
  if(!geometryProbeFile||geometryProbeLimited)return;
  const size_t bytes=16+8+sizeof(world)+vertices.size()*3*sizeof(float);
  if(vertices.size()%3||geometryProbeDraws>=4096||geometryProbeBytes+bytes>64*1024*1024){geometryProbeLimited=true;return;}
  const uint32_t header[]={1,drawId,static_cast<uint32_t>(vertices.size()/3),cull};
  bool ok=fwrite(header,sizeof(header),1,geometryProbeFile)==1&&fwrite(&hash,sizeof(hash),1,geometryProbeFile)==1&&
    fwrite(&world,sizeof(world),1,geometryProbeFile)==1;
  for(const auto& v:vertices)if(ok)ok=fwrite(v.position,12,1,geometryProbeFile)==1;
  if(!ok){geometryProbeLimited=true;return;}
  geometryProbeBytes+=bytes;++geometryProbeDraws;geometryProbeTriangles+=static_cast<unsigned>(vertices.size()/3);
}
static void EndGeometryProbe() {
  if(!geometryProbeFile)return;
  const uint32_t footer[]={0,geometryProbeDraws,geometryProbeTriangles,geometryProbeLimited?1u:0u};
  fwrite(footer,sizeof(footer),1,geometryProbeFile);fclose(geometryProbeFile);geometryProbeFile=nullptr;
}
