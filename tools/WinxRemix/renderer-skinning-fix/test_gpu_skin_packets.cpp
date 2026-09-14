// Own packet -> actual Remix CPU/GPU kernel comparison. No game or bridge.
#include "cpu_skinning_reference.h"
#include "vulkan_skin_dispatch.h"
#include "tools/WinxRemix/winx_skin_packet_remix.h"
#include "tools/WinxRemix/winx_skin_packet_gpu.h"
#include "public/include/remix/remix_c.h"
#include <filesystem>
namespace packet=winx_remix::skin_packet;
using skin_gpu::Buffer;
using skin_gpu::Compute;
static_assert(sizeof(remixapi_HardcodedVertex)==64&&offsetof(remixapi_HardcodedVertex,normal)==12&&offsetof(remixapi_HardcodedVertex,color)==32);
static uint64_t checks;
static unsigned dispatches;
static void Check(bool okay,const char* why){++checks;if(!okay)throw std::runtime_error(why);}
constexpr unsigned ChunkVertices=4096;
constexpr float Sentinel=-1234567.25f;
struct Metrics {double cpuPosition=0,cpuNormal=0,originalPosition=0,originalNormal=0;uint64_t differentVertices=0;};
static double Difference(float a,float b){return std::fabs(double(a)-b);}
struct Input {
  SkinningArgs args{};ApiSource source{};
  std::vector<float> stream,weights;
  std::vector<uint32_t> indices,packed;
  Input(const packet::Packet& p,const packet::BakedMesh& baked,unsigned begin,unsigned count,unsigned variant):
      stream(size_t(count)*16),weights(size_t(count)*(variant==2?1:p.influences+(variant==3))),indices(weights.size()){
    const bool alreadyBaked=variant==2;const unsigned b=alreadyBaked?1:p.influences+(variant==3);source={count,{b,indices.data()}};
    packet::SignedSkin signedSkin;
    if(variant==3)Check(packet::EncodeSignedSkin(p,signedSkin)&&signedSkin.influences==b,"signed palette encoding keeps all authored weights explicit");
    for(unsigned v=0;v<count;++v){const auto& original=p.vertices[begin+v];const auto& ready=baked.vertices[begin+v];remixapi_HardcodedVertex vertex{};
      for(unsigned c=0;c<3;++c){vertex.position[c]=alreadyBaked?ready.position[c]:original.skin.position[c];vertex.normal[c]=alreadyBaked?ready.normal[c]:original.skin.normal[c];}
      for(unsigned c=0;c<2;++c)vertex.texcoord[c]=original.uv[c];vertex.color=original.color;
      memcpy(stream.data()+size_t(v)*16,&vertex,sizeof(vertex));
      for(unsigned i=0;i<b;++i){weights[size_t(v)*b+i]=alreadyBaked?1:variant==3?signedSkin.weights[size_t(begin+v)*b+i]:original.skin.weights[i];
        indices[size_t(v)*b+i]=alreadyBaked?0:variant==3?signedSkin.indices[size_t(begin+v)*b+i]:original.skin.indices[i];}
    }
    packed=Pack(source);args.numVertices=count;args.useIndices=1;args.numBones=b;
    args.srcPositionStride=args.srcNormalStride=sizeof(remixapi_HardcodedVertex);args.srcNormalOffset=offsetof(remixapi_HardcodedVertex,normal);
    args.dstPositionOffset=16;args.dstPositionStride=20;args.dstNormalOffset=8;args.dstNormalStride=16;
    const auto& palette=variant==3?signedSkin.palette:p.palette;
    for(unsigned bone=0;bone<(alreadyBaked?1:palette.size());++bone){dxvk::Matrix4 matrix;
      if(!alreadyBaked)for(unsigned row=0;row<3;++row)for(unsigned col=0;col<4;++col)matrix[col][row]=palette[bone][row][col];
      memcpy(&args.bones[bone],&matrix,sizeof(matrix));}
  }
};
static void Dispatch(Compute& gpu,Input& input,const packet::BakedMesh& baked,unsigned begin,unsigned count,unsigned variant,Metrics& metrics){
  input.args.blendWeightStride=variant==0?StockWeightStride(input.source):FixedWeightStride(input.source);
  input.args.blendIndicesStride=variant==0?StockIndicesStride(input.source):FixedIndicesStride(input.source);
  const unsigned padded=((count+127)/128)*128;
  std::vector<float> expectedPosition(4+size_t(padded)*5+8,Sentinel),expectedNormal(2+size_t(padded)*4+8,Sentinel);
  for(unsigned v=0;v<count;++v)dxvk::skinning(v,expectedPosition.data(),expectedNormal.data(),input.stream.data(),input.weights.data(),
    reinterpret_cast<const uint8_t*>(input.packed.data()),input.stream.data(),input.args);
  std::vector<float> initialPosition(expectedPosition.size(),Sentinel),initialNormal(expectedNormal.size(),Sentinel);
  std::array<Buffer,7> buffers;
  gpu.Allocate(buffers[0],&input.args,sizeof(input.args),0);
  gpu.Allocate(buffers[1],initialPosition.data(),initialPosition.size()*4,1);
  gpu.Allocate(buffers[2],input.stream.data(),input.stream.size()*4,2);
  gpu.Allocate(buffers[3],input.weights.data(),input.weights.size()*4,3);
  gpu.Allocate(buffers[4],input.packed.data(),input.packed.size()*4,4);
  gpu.Allocate(buffers[5],initialNormal.data(),initialNormal.size()*4,5);
  gpu.Allocate(buffers[6],input.stream.data(),input.stream.size()*4,6);
  gpu.Execute(buffers,count);
  Check(!memcmp(buffers[0].mapped,&input.args,sizeof(input.args))&&
    !memcmp(buffers[2].mapped,input.stream.data(),input.stream.size()*4)&&!memcmp(buffers[6].mapped,input.stream.data(),input.stream.size()*4)&&
    !memcmp(buffers[3].mapped,input.weights.data(),input.weights.size()*4)&&!memcmp(buffers[4].mapped,input.packed.data(),input.packed.size()*4),
    "GPU preserves constants, positions, normals, weights, packed indices, UV/color and API padding");
  for(unsigned channel:{1u,5u}){const auto& expected=channel==1?expectedPosition:expectedNormal;const auto* actual=static_cast<const float*>(buffers[channel].mapped);
    for(size_t i=0;i<expected.size();++i){Check(std::isfinite(expected[i])&&std::isfinite(actual[i]),"finite actual CPU/GPU outputs");
      if(expected[i]==Sentinel)Check(actual[i]==Sentinel,"output offsets, padding and excess GPU lanes remain untouched");
      else {const double error=Difference(actual[i],expected[i]);auto& maximum=channel==1?metrics.cpuPosition:metrics.cpuNormal;maximum=(std::max)(maximum,error);
        Check(error<=(channel==1?1e-4+2e-6*std::fabs(double(expected[i])):2e-5),"GPU agrees with actual pinned CPU kernel within documented arithmetic tolerance");}}
  }
  const auto* position=static_cast<const float*>(buffers[1].mapped);const auto* normal=static_cast<const float*>(buffers[5].mapped);
  for(unsigned v=0;v<count;++v){const auto& expected=baked.vertices[begin+v];bool different=false;
    for(unsigned c=0;c<3;++c){const double pe=Difference(position[4+v*5+c],expected.position[c]),ne=Difference(normal[2+v*4+c],expected.normal[c]);
      metrics.originalPosition=(std::max)(metrics.originalPosition,pe);metrics.originalNormal=(std::max)(metrics.originalNormal,ne);different|=pe>1e-4||ne>2e-5;
      if(variant==2)Check(pe<=1e-5+2e-6*std::fabs(double(expected.position[c]))&&ne<=2e-5,"baked identity control preserves shared original world position and normal direction");
      if(variant==3)Check(pe<=1e-4+2e-6*std::fabs(double(expected.position[c]))&&ne<=2e-5,"signed palette with zero remainder reproduces authored Fixed geometry without CPU baking");
    }metrics.differentVertices+=different;
  }
  ++dispatches;
}
static void PrintMetrics(const Metrics& m){printf("{\"cpuPositionError\":%.17g,\"cpuNormalError\":%.17g,\"originalPositionError\":%.17g,\"originalNormalError\":%.17g,\"differentVertices\":%llu}",m.cpuPosition,m.cpuNormal,m.originalPosition,m.originalNormal,static_cast<unsigned long long>(m.differentVertices));}
static void Accumulate(Metrics& out,const Metrics& value){out.cpuPosition=(std::max)(out.cpuPosition,value.cpuPosition);out.cpuNormal=(std::max)(out.cpuNormal,value.cpuNormal);
  out.originalPosition=(std::max)(out.originalPosition,value.originalPosition);out.originalNormal=(std::max)(out.originalNormal,value.originalNormal);out.differentVertices+=value.differentVertices;}
static std::array<Metrics,4> Exercise(Compute& gpu,const packet::Packet& p,const char* kind,unsigned ordinal,packet::WeightCompatibility& compatibility){
  packet::BakedMesh baked;Check(packet::InspectRemixWeights(p,compatibility)&&packet::Bake(p,baked),"packet compatibility and complete shared bake");
  Check(baked.indices==p.indices&&baked.attributes==p.attributes,"baked geometry topology and attribute flags preserved");
  std::array<Metrics,4> metrics{};
  for(unsigned begin=0;begin<p.vertices.size();begin+=ChunkVertices){const unsigned count=(std::min)(ChunkVertices,unsigned(p.vertices.size()-begin));
    for(unsigned variant=0;variant<4;++variant){Input input(p,baked,begin,count,variant);Dispatch(gpu,input,baked,begin,count,variant,metrics[variant]);}}
  printf("{\"event\":\"packet\",\"kind\":\"%s\",\"ordinal\":%u,\"vertices\":%zu,\"triangles\":%zu,\"influences\":%u,\"bones\":%zu,\"exactWeightContract\":%s,\"changedWeightVertices\":%u,\"negativeWeightVertices\":%u,\"maxLastWeightDelta\":%.17g,\"variants\":[",kind,ordinal,p.vertices.size(),p.indices.size()/3,p.influences,p.palette.size(),compatibility.Exact()?"true":"false",compatibility.changedVertices,compatibility.negativeVertices,compatibility.maximumLastWeightDelta);
  for(unsigned variant=0;variant<4;++variant){if(variant)fputc(',',stdout);PrintMetrics(metrics[variant]);}fputs("]}\n",stdout);fflush(stdout);return metrics;
}
static packet::Packet Read(const std::filesystem::path& path){std::ifstream file(path,std::ios::binary|std::ios::ate);Check(bool(file),"packet open");const auto size=file.tellg();
  Check(size>=0&&uint64_t(size)<=packet::MaximumWireBytes,"packet byte bound");std::vector<uint8_t> bytes(size_t(size),0);file.seekg(0);file.read(reinterpret_cast<char*>(bytes.data()),size);
  packet::Packet p;Check(bool(file)&&packet::Decode(bytes,p),"complete bounded packet decode");return p;
}
static packet::Packet Synthetic(unsigned b,const std::array<float,4>& weights,unsigned count,bool amplified=false){
  packet::Packet p;p.influences=b;p.attributes=3;p.palette.resize(16);p.vertices.resize(count);p.indices.resize(count);
  for(unsigned bone=0;bone<16;++bone){auto& m=p.palette[bone];m[0]={1+bone*.03125f,bone*.015625f,0,float(bone)*2};m[1]={0,.75f+bone*.015625f,0,-float(bone)};m[2]={0,0,1+bone*.0078125f,float(bone)*3};}
  if(amplified)for(auto& matrix:p.palette)matrix[0][3]=1000000;
  for(unsigned v=0;v<count;++v){auto& vertex=p.vertices[v];vertex.skin.position={float(v%17)*.25f,-float(v%7)*.5f,2,1};vertex.skin.normal={float(v%3)*.25f,.5f,-2,1};vertex.skin.weights=weights;
    for(unsigned i=0;i<b;++i)vertex.skin.indices[i]=(v*3+i)%16;vertex.uv={float(v%7)*.125f,.5f};vertex.color=0xff336600u+(v%256);vertex.sourceIndex=v;p.indices[v]=v;}
  Check(packet::Validate(p),"bounded universal synthetic source");return p;
}
int main(int argc,char** argv){try{
  if(argc!=4)throw std::runtime_error("usage: test_gpu_skin_packets shader.spv packet-directory packet-count");
  size_t parsed=0;const auto count=std::stoul(argv[3],&parsed);Check(parsed==strlen(argv[3])&&count>0&&count<=128,"packet count bound");
  unsigned syntheticCases=0,capturedCases=0,needsBake=0;uint64_t capturedVertices=0,changedWeights=0;std::array<Metrics,4> totals{};
  {Compute gpu(argv[1]);printf("{\"event\":\"device\",\"vendor\":%u,\"device\":%u,\"apiVersion\":%u,\"driverVersion\":%u,\"validationLayer\":false,\"chunkVertices\":%u}\n",gpu.properties.vendorID,gpu.properties.deviceID,gpu.properties.apiVersion,gpu.properties.driverVersion,ChunkVertices);fflush(stdout);
    struct Spec{unsigned b;std::array<float,4> weights;unsigned vertices;bool amplified;};
    const Spec specs[]={{1,{1,0,0,0},129,false},{1,{2,0,0,0},129,false},{1,{-.5f,0,0,0},129,false},
      {2,{.25f,.75f,0,0},129,false},{2,{.25f,.25f,0,0},129,false},{2,{1.25f,-.25f,0,0},129,false},
      {2,{.5f,.5f+0x1p-23f,0,0},129,true},{3,{.25f,.25f,.5f,0},129,false},{4,{.125f,.25f,.125f,.5f},4101,false},
      {3,{-.25f,.125f,.5f,0},129,false},{4,{-.5f,.25f,1.25f,-.125f},129,false},
      {4,{0,0,0,-1},129,false},{4,{.5f,-.5f,.125f,0},129,false}};
    for(const auto& spec:specs){packet::WeightCompatibility compatibility;const auto metrics=Exercise(gpu,Synthetic(spec.b,spec.weights,spec.vertices,spec.amplified),"synthetic",++syntheticCases,compatibility);
      if(spec.amplified)Check(!compatibility.Exact()&&metrics[1].originalPosition>=.1,"small weight error produces material positional error and requires bake");
      if(compatibility.Exact())Check(metrics[1].originalPosition<=1e-4&&metrics[1].originalNormal<=2e-5,"compatible synthetic B1-B4 match shared Fixed with corrected strides");
      if(spec.b==4)Check(metrics[0].differentVertices>0,"stock stride control detects the defect across chunk boundaries");}
    for(unsigned i=1;i<=count;++i){char name[40]{};snprintf(name,sizeof(name),"packet-%04u.skp",i);const auto p=Read(std::filesystem::path(argv[2])/name);
      packet::WeightCompatibility compatibility;const auto metrics=Exercise(gpu,p,"captured",i,compatibility);
      ++capturedCases;capturedVertices+=p.vertices.size();changedWeights+=compatibility.changedVertices;needsBake+=!compatibility.Exact();
      for(unsigned variant=0;variant<4;++variant)Accumulate(totals[variant],metrics[variant]);}
  }
  printf("{\"status\":\"PASS\",\"gpu\":true,\"capturedPackets\":%u,\"capturedVertices\":%llu,\"changedWeightVertices\":%llu,\"packetsNeedingWeightAdaptation\":%u,\"syntheticCases\":%u,\"dispatches\":%u,\"checks\":%llu,\"fullRenderer\":false,\"bridge\":false,\"resourcesDestroyed\":true,\"capturedVariants\":[",capturedCases,static_cast<unsigned long long>(capturedVertices),static_cast<unsigned long long>(changedWeights),needsBake,syntheticCases,dispatches,static_cast<unsigned long long>(checks));
  for(unsigned variant=0;variant<4;++variant){if(variant)fputc(',',stdout);PrintMetrics(totals[variant]);}fputs("]}\n",stdout);return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL %s\n",error.what());return 1;}}
