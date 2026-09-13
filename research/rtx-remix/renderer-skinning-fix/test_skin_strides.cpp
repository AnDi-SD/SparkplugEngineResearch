// Own bounded regression using the unchanged renderer's actual CPU/GPU header.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <array>
#include <stdexcept>
#include <cmath>
#include "rtx/pass/gpu_skinning_binding_indices.h"
#include "rtx/pass/skinning.h"
#include "source_expressions.h"
// Own test logging sink. Every production math diagnostic fails the fixture;
// no algorithm or success path is replaced, and no renderer log backend links.
void dxvk::Logger::err(const std::string& message){throw std::runtime_error("renderer math diagnostic: "+message);}
static unsigned checks,stockFailures,fixedCases,weightOnlyIndexFailures;
static void Check(bool okay,const char* name){++checks;if(!okay)throw std::runtime_error(name);}
struct Run {
  unsigned b;
  std::vector<float> weights,positions,normals;
  std::vector<uint32_t> indices,packed;
  ApiSource source{};SkinningArgs args{};
  explicit Run(unsigned count):b(count),weights(3*b),positions{0,1,5,2,3,5,4,5,5},normals(9,0),indices(3*b){
    for(unsigned v=0;v<3;++v){float left=1;
      for(unsigned j=0;j<b;++j){float w=j+1==b?left:float(j+v+1)/128.f;weights[v*b+j]=w;left-=w;indices[v*b+j]=(v*3+j)%16;}
      normals[v*3+2]=-2;}
    source={3,{b,indices.data()}};packed=Pack(source);
    args.srcPositionStride=args.srcNormalStride=12;args.numVertices=3;args.useIndices=1;args.numBones=b;
    for(unsigned bone=0;bone<256;++bone){dxvk::Matrix4 matrix;matrix[3][0]=float(bone)*4;matrix[3][1]=-float(bone)*2;
      static_assert(sizeof(matrix)==sizeof(args.bones[0]));memcpy(&args.bones[bone],&matrix,sizeof(matrix));}
  }
  std::array<float,3> Execute(unsigned v,unsigned weightStride,unsigned indexStride){
    args.blendWeightStride=weightStride;args.blendIndicesStride=indexStride;
    std::array<float,3> output{},normal{};
    dxvk::skinning(v,output.data(),normal.data(),positions.data(),weights.data(),reinterpret_cast<const uint8_t*>(packed.data()),normals.data(),args);
    Check(std::fabs(normal[0])<1e-6&&std::fabs(normal[1])<1e-6&&std::fabs(normal[2]+1)<1e-6,"actual common skinning normal normalization");
    return output;
  }
  std::array<float,3> Expected(unsigned v){
    // Closed-form translation oracle; does not duplicate the skinning loop.
    float weightedIndex=0;for(unsigned j=0;j<b;++j)weightedIndex+=weights[v*b+j]*float(indices[v*b+j]);
    return {positions[v*3]+4*weightedIndex,positions[v*3+1]-2*weightedIndex,5};
  }
};
static bool Same(const std::array<float,3>& a,const std::array<float,3>& b){for(unsigned i=0;i<3;++i)if(std::fabs(a[i]-b[i])>1e-5)return false;return true;}
int main(){try{
  for(unsigned b:{1u,2u,3u,4u,5u,8u}){Run run(b);
    Check(FixedWeightStride(run.source)==4*b,"weight RasterBuffer expression spans complete API tuple");
    Check(FixedIndicesStride(run.source)==4*((b+3)/4),"index RasterBuffer expression spans all compressed words");
    unsigned stockMismatch=0,weightOnlyMismatch=0;
    for(unsigned v=0;v<3;++v){const auto expected=run.Expected(v);
      const auto fixed=run.Execute(v,FixedWeightStride(run.source),FixedIndicesStride(run.source));
      Check(Same(fixed,expected),"fixed source expressions plus actual skinning match authored unit-sum result");++fixedCases;
      if(!Same(run.Execute(v,StockWeightStride(run.source),StockIndicesStride(run.source)),expected))++stockMismatch;
      if(!Same(run.Execute(v,FixedWeightStride(run.source),StockIndicesStride(run.source)),expected))++weightOnlyMismatch;
    }
    Check((b==1)==(stockMismatch==0),"stock stride control fails every multi-influence case and preserves B1");stockFailures+=stockMismatch;
    Check((b<=4)==(weightOnlyMismatch==0),"weight-only fix leaves independently reproduced B5/B8 index-stride defect");weightOnlyIndexFailures+=weightOnlyMismatch;
  }
  {Run run(1);run.weights[0]=2;auto result=run.Execute(0,FixedWeightStride(run.source),FixedIndicesStride(run.source));
    Check(result[2]==5,"existing implicit-last rule ignores stored B1 weight2");}
  {Run run(2);run.weights[0]=.25f;run.weights[1]=.25f;auto result=run.Execute(0,FixedWeightStride(run.source),FixedIndicesStride(run.source));
    Check(result[0]==3&&result[1]==-.5f&&result[2]==5,"existing implicit last is0.75, not authored0.25; stride patch does not change policy");
    run.weights[0]=1.25f;run.weights[1]=-.25f;result=run.Execute(0,FixedWeightStride(run.source),FixedIndicesStride(run.source));
    Check(result[2]==6.25f,"negative implicit remainder is skipped without position renormalization");}
  {uint32_t indices[]={255,254,253,252,251,250,249,248};ApiSource src{1,{8,indices}};const auto packed=Pack(src);
    const auto* bytes=reinterpret_cast<const uint8_t*>(packed.data());bool exact=true;for(unsigned i=0;i<8;++i)exact&=bytes[i]==indices[i];
    Check(exact&&packed.size()==2,"actual packing preserves all8 legal byte-valued indices");
    indices[0]=256;const auto invalid=Pack(src);const auto* bad=reinterpret_cast<const uint8_t*>(invalid.data());
    Check(bad[0]==0&&bad[1]==255,"known unsupported uint32 index256 contaminates adjacent packed lane; caller guard still required");}
  printf("{\"status\":\"PASS\",\"checks\":%u,\"fixedVertices\":%u,\"stockWrongVertices\":%u,\"weightOnlyWrongIndexVertices\":%u,\"actualCommonSkinningHeader\":true,\"rendererBinaryBuilt\":false,\"gpu\":false}\n",checks,fixedCases,stockFailures,weightOnlyIndexFailures);return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL %s\n",error.what());return 1;}}
