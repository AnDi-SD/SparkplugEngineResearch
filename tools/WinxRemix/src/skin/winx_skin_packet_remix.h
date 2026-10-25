#pragma once
// Own preparation for Remix. The recovered Fixed expression stays in Sparkplug.
// This file owns no native/COM/API handles and performs no renderer submission.
#include "winx_skin_packet.h"

namespace winx_remix::skin_packet {
struct WeightCompatibility {
  uint32_t changedVertices=0,negativeVertices=0,nonfiniteRemainders=0;
  double maximumLastWeightDelta=0;
  bool Exact()const{return !changedVertices&&!negativeVertices&&!nonfiniteRemainders;}
};
// Pinned Remix skinning.h subtracts B-1 weights in sequence and skips <=0.
// Equality here describes weights only, not bit-identical GPU arithmetic or
// selected shader/material compatibility. No weight tolerance is a draw gate.
inline bool InspectRemixWeights(const Packet& packet,WeightCompatibility& output,Error* error=nullptr){
  if(!Validate(packet,error))return false;WeightCompatibility result;
  for(const auto& vertex:packet.vertices){float remaining=1;bool negative=false;
    for(unsigned i=0;i<packet.influences;++i){negative|=vertex.skin.weights[i]<0;
      if(i+1<packet.influences)remaining-=vertex.skin.weights[i];}
    result.negativeVertices+=negative;
    const float authored=vertex.skin.weights[packet.influences-1];
    if(!Finite(remaining))++result.nonfiniteRemainders;
    else result.maximumLastWeightDelta=(std::max)(result.maximumLastWeightDelta,std::fabs(double(remaining)-authored));
    result.changedVertices+=!Finite(remaining)||remaining!=authored;
  }
  output=result;return true;
}
struct BakedVertex {
  std::array<float,3> position{},normal{};
  std::array<float,2> uv{};
  uint32_t color=0xffffffff;
};
struct BakedMesh {
  std::vector<BakedVertex> vertices;
  std::vector<uint32_t> indices;
  uint32_t attributes=0;
};
// World positions use every authored weight. Normal direction is normalized
// from the recovered float4 result; the original float4 is not overwritten.
// A consumer must use identity instance transform and disable skinning to avoid
// applying bones twice. Material, camera and owner lifetime are separate gates.
inline bool Bake(const Packet& packet,BakedMesh& output,Error* error=nullptr){
  std::vector<pc::FixedSkinDeformationForAnalysis> deformed;
  if(!Deform(packet,deformed,error))return false;
  try {BakedMesh result;result.attributes=packet.attributes;result.indices=packet.indices;
    result.vertices.resize(deformed.size());
    for(size_t i=0;i<deformed.size();++i){const auto& value=deformed[i];auto& vertex=result.vertices[i];
      const double length=std::sqrt(double(value.normal[0])*value.normal[0]+double(value.normal[1])*value.normal[1]+double(value.normal[2])*value.normal[2]);
      if(!(length>0)||!std::isfinite(length))return Fail(error,Error::Deformation);
      for(unsigned c=0;c<3;++c){vertex.position[c]=value.position[c];vertex.normal[c]=float(value.normal[c]/length);}
      vertex.uv=packet.vertices[i].uv;vertex.color=packet.vertices[i].color;
    }
    output=std::move(result);return true;
  }catch(const std::bad_alloc&){return Fail(error,Error::Range);}
}
} // namespace winx_remix::skin_packet
