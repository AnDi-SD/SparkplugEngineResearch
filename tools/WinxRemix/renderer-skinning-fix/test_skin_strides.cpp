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
#include "Sparkplug/Analysis/PC/spFixedShaderSkinning.h"
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
  explicit Run(unsigned count,unsigned vertices=3):b(count),weights(vertices*b),positions(vertices*3),normals(vertices*3,0),indices(vertices*b){
    for(unsigned v=0;v<vertices;++v){float left=1;
      positions[v*3]=float((v%3)*2);positions[v*3+1]=float((v%3)*2+1);positions[v*3+2]=5;
      for(unsigned j=0;j<b;++j){float w=j+1==b?left:float(j+v%3+1)/128.f;weights[v*b+j]=w;left-=w;indices[v*b+j]=(v*3+j)%16;}
      normals[v*3+2]=-2;}
    source={vertices,{b,indices.data()}};packed=Pack(source);
    args.srcPositionStride=args.srcNormalStride=12;args.numVertices=vertices;args.useIndices=1;args.numBones=b;
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
static sparkplug::evidence::pc::FixedSkinDeformationForAnalysis Original(const Run& run,unsigned v){
  using namespace sparkplug::evidence::pc;
  FixedSkinVertexForAnalysis input{};input.position[3]=1;
  for(unsigned c=0;c<3;++c){input.position[c]=run.positions[v*3+c];input.normal[c]=run.normals[v*3+c];}
  for(unsigned j=0;j<run.b;++j){input.weights[j]=run.weights[v*run.b+j];input.indices[j]=run.indices[v*run.b+j];}
  std::array<FixedSkinMatrix,16> palette{};
  for(unsigned bone=0;bone<16;++bone){dxvk::Matrix4 matrix;memcpy(&matrix,&run.args.bones[bone],sizeof(matrix));
    for(unsigned coordinate=0;coordinate<3;++coordinate)for(unsigned component=0;component<4;++component)
      palette[bone][coordinate][component]=matrix[component][coordinate];}
  FixedSkinDeformationForAnalysis result{};
  Check(DeformFixedSkinVertexForAnalysis(input,run.b,palette.data(),palette.size(),result),"shared original shader semantic boundary");
  return result;
}
int main(){try{
  for(unsigned b:{1u,2u,3u,4u,5u,8u}){Run run(b);
    Check(FixedWeightStride(run.source)==4*b,"weight RasterBuffer expression spans complete API tuple");
    Check(FixedIndicesStride(run.source)==4*((b+3)/4),"index RasterBuffer expression spans all compressed words");
    unsigned stockMismatch=0,weightOnlyMismatch=0;
    for(unsigned v=0;v<3;++v){const auto expected=run.Expected(v);
      const auto fixed=run.Execute(v,FixedWeightStride(run.source),FixedIndicesStride(run.source));
      Check(Same(fixed,expected),"fixed source expressions plus actual skinning match authored unit-sum result");++fixedCases;
      if(b<=4){const auto original=Original(run,v);
        Check(Same(fixed,{original.position[0],original.position[1],original.position[2]}),"unit-sum B1-B4 also match recovered Fixed shader");}
      if(!Same(run.Execute(v,StockWeightStride(run.source),StockIndicesStride(run.source)),expected))++stockMismatch;
      if(!Same(run.Execute(v,FixedWeightStride(run.source),StockIndicesStride(run.source)),expected))++weightOnlyMismatch;
    }
    Check((b==1)==(stockMismatch==0),"stock stride control fails every multi-influence case and preserves B1");stockFailures+=stockMismatch;
    Check((b<=4)==(weightOnlyMismatch==0),"weight-only fix leaves independently reproduced B5/B8 index-stride defect");weightOnlyIndexFailures+=weightOnlyMismatch;
  }
  {Run run(1);run.weights[0]=2;auto result=run.Execute(0,FixedWeightStride(run.source),FixedIndicesStride(run.source));
    Check(result[2]==5,"existing implicit-last rule ignores stored B1 weight2");
    Check(Original(run,0).position[2]==10,"original B1 retains the explicit nonunit weight");}
  {Run run(2);run.weights[0]=.25f;run.weights[1]=.25f;auto result=run.Execute(0,FixedWeightStride(run.source),FixedIndicesStride(run.source));
    Check(result[0]==3&&result[1]==-.5f&&result[2]==5,"existing implicit last is0.75, not authored0.25; stride patch does not change policy");
    Check(Original(run,0).position[2]==2.5f,"original nonunit B2 differs despite correct strides");
    run.weights[0]=1.25f;run.weights[1]=-.25f;result=run.Execute(0,FixedWeightStride(run.source),FixedIndicesStride(run.source));
    Check(result[2]==6.25f,"negative implicit remainder is skipped without position renormalization");}
  float nearUnitPositionError=0;
  {Run run(2);run.weights[0]=.5f;run.weights[1]=.5f+0x1p-23f;
    dxvk::Matrix4 matrix;matrix[3][0]=1000000.f;memcpy(&run.args.bones[1],&matrix,sizeof(matrix));
    const auto original=Original(run,0);
    const auto actual=run.Execute(0,FixedWeightStride(run.source),FixedIndicesStride(run.source));
    const double sumError=std::fabs(double(run.weights[0])+run.weights[1]-1.0);
    nearUnitPositionError=std::fabs(original.position[0]-actual[0]);
    Check(sumError<1e-5&&nearUnitPositionError>=.1f,"near-unit sum alone does not bound world-position error");}
  {uint32_t indices[]={255,254,253,252,251,250,249,248};ApiSource src{1,{8,indices}};const auto packed=Pack(src);
    const auto* bytes=reinterpret_cast<const uint8_t*>(packed.data());bool exact=true;for(unsigned i=0;i<8;++i)exact&=bytes[i]==indices[i];
    Check(exact&&packed.size()==2,"actual packing preserves all8 legal byte-valued indices");
    indices[0]=256;const auto invalid=Pack(src);const auto* bad=reinterpret_cast<const uint8_t*>(invalid.data());
    Check(bad[0]==0&&bad[1]==255,"known unsupported uint32 index256 contaminates adjacent packed lane; caller guard still required");}
  printf("{\"status\":\"PASS\",\"checks\":%u,\"fixedVertices\":%u,\"stockWrongVertices\":%u,\"weightOnlyWrongIndexVertices\":%u,\"nearUnitPositionError\":%.9g,\"actualCommonSkinningHeader\":true,\"sharedOriginalShaderExpressions\":true,\"rendererBinaryBuilt\":false,\"gpu\":false}\n",checks,fixedCases,stockFailures,weightOnlyIndexFailures,nearUnitPositionError);return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL %s\n",error.what());return 1;}}
