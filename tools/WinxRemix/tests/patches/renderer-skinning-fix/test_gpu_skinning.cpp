// Own bounded verification of the frozen Remix compute shader.
#define main CpuSkinStrideMain
#include "test_skin_strides.cpp"
#undef main
#include "vulkan_skin_dispatch.h"
using skin_gpu::Buffer;
using skin_gpu::Compute;

int main(int argc,char** argv){try{
  if(argc!=2)throw std::runtime_error("usage: test_gpu_skinning frozen-gpu_skinning.spv");
  unsigned dispatches=0,verticesChecked=0,stockWrong=0,indexOnlyWrong=0;float maxError=0,maxOriginalError=0;
  {Compute gpu(argv[1]);
    std::printf("{\"event\":\"device\",\"vendor\":%u,\"device\":%u,\"apiVersion\":%u,\"driverVersion\":%u,\"validationLayer\":false}\n",gpu.properties.vendorID,gpu.properties.deviceID,gpu.properties.apiVersion,gpu.properties.driverVersion);std::fflush(stdout);
    for(unsigned n:{3u,325u})for(unsigned b:{1u,2u,3u,4u,5u,8u}){
      Run run(b,n);
      for(unsigned v=0;v<n;++v){run.normals[v*3]=float(v%3)*.25f;run.normals[v*3+1]=.5f;}
      for(unsigned bone=0;bone<16;++bone){dxvk::Matrix4 matrix;memcpy(&matrix,&run.args.bones[bone],sizeof(matrix));matrix[0][0]=1+bone*.125f;matrix[1][1]=.75f+bone*.03125f;matrix[2][2]=.5f+bone*.0625f;matrix[1][0]=bone*.015625f;memcpy(&run.args.bones[bone],&matrix,sizeof(matrix));}
      run.args.dstPositionOffset=16;run.args.dstPositionStride=20;run.args.dstNormalOffset=8;run.args.dstNormalStride=16;
      const unsigned padded=((n+127)/128)*128;const float sentinel=-1234567.25f;
      std::vector<float> referencePosition(padded*5+16,sentinel),referenceNormal(padded*4+16,sentinel);
      auto cpu=[&](unsigned weightStride,unsigned indexStride,std::vector<float>& p,std::vector<float>& normal){run.args.blendWeightStride=weightStride;run.args.blendIndicesStride=indexStride;for(unsigned v=0;v<n;++v)dxvk::skinning(v,p.data(),normal.data(),run.positions.data(),run.weights.data(),reinterpret_cast<const uint8_t*>(run.packed.data()),run.normals.data(),run.args);};
      cpu(FixedWeightStride(run.source),FixedIndicesStride(run.source),referencePosition,referenceNormal);
      for(unsigned variant=0;variant<3;++variant){
        const unsigned ws=variant==0?StockWeightStride(run.source):FixedWeightStride(run.source),is=variant==2?FixedIndicesStride(run.source):StockIndicesStride(run.source);
        std::vector<float> expectedPosition(padded*5+16,sentinel),expectedNormal(padded*4+16,sentinel);cpu(ws,is,expectedPosition,expectedNormal);
        std::vector<float> initialPosition(expectedPosition.size(),sentinel),initialNormal(expectedNormal.size(),sentinel);std::array<Buffer,7> buffers;
        gpu.Allocate(buffers[0],&run.args,sizeof(run.args),0);gpu.Allocate(buffers[1],initialPosition.data(),initialPosition.size()*4,1);gpu.Allocate(buffers[2],run.positions.data(),run.positions.size()*4,2);
        gpu.Allocate(buffers[3],run.weights.data(),run.weights.size()*4,3);gpu.Allocate(buffers[4],run.packed.data(),run.packed.size()*4,4);gpu.Allocate(buffers[5],initialNormal.data(),initialNormal.size()*4,5);gpu.Allocate(buffers[6],run.normals.data(),run.normals.size()*4,6);
        gpu.Execute(buffers,n);unsigned mismatches=0;
        for(unsigned channel:{1u,5u}){const auto& expected=channel==1?expectedPosition:expectedNormal;const auto* actual=static_cast<const float*>(buffers[channel].mapped);
          for(size_t i=0;i<expected.size();++i){if(expected[i]==sentinel)Check(actual[i]==sentinel,"GPU preserves output offsets, padding and excess workgroup lanes");else{Check(std::isfinite(actual[i]),"finite GPU output");const float error=std::fabs(actual[i]-expected[i]);maxError=std::max(maxError,error);Check(error<0.0001f,"GPU matches actual shared CPU skinning header");}}}
        const auto* actual=static_cast<const float*>(buffers[1].mapped);const auto* normal=static_cast<const float*>(buffers[5].mapped);
        for(unsigned v=0;v<n;++v){bool different=false;for(unsigned c=0;c<3;++c)different|=std::fabs(actual[4+v*5+c]-referencePosition[4+v*5+c])>.0001f;if(different)++mismatches;
          if(variant==2&&b<=4){const auto original=Original(run,v);for(unsigned c=0;c<3;++c){const float error=std::max(std::fabs(actual[4+v*5+c]-original.position[c]),std::fabs(normal[2+v*4+c]-original.normal[c]));maxOriginalError=std::max(maxOriginalError,error);Check(error<.0001f,"GPU fixed B1-B4 agrees with recovered original Fixed expressions");}}}
        if(variant==0){Check((b==1)==(mismatches==0),"GPU stock control detects multiweight defect");stockWrong+=mismatches;}
        if(variant==1){Check((b<=4)==(mismatches==0),"GPU weight-only control detects packed index stride defect");indexOnlyWrong+=mismatches;}
        if(variant==2)Check(mismatches==0,"GPU v2 strides match fixed reference");
        ++dispatches;verticesChecked+=n;std::printf("{\"event\":\"case\",\"vertices\":%u,\"bones\":%u,\"variant\":%u,\"weightStride\":%u,\"indexStride\":%u,\"wrongVsFixed\":%u}\n",n,b,variant,ws,is,mismatches);std::fflush(stdout);
      }
    }
  }
  std::printf("{\"status\":\"PASS\",\"gpu\":true,\"dispatches\":%u,\"verticesChecked\":%u,\"checks\":%u,\"stockWrongVertices\":%u,\"weightOnlyWrongVertices\":%u,\"maxCpuGpuError\":%.9g,\"maxOriginalError\":%.9g,\"fullRenderer\":false,\"resourcesDestroyed\":true}\n",dispatches,verticesChecked,checks,stockWrong,indexOnlyWrong,maxError,maxOriginalError);return 0;
}catch(const std::exception& error){std::fprintf(stderr,"FAIL %s\n",error.what());return 1;}}
