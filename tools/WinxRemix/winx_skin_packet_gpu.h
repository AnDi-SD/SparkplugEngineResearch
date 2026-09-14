#pragma once
// Own encoding for the pinned Remix kernel's positive explicit weights and
// implicit final weight. The recovered Fixed expression is unchanged.
#include "winx_skin_packet.h"

namespace winx_remix::skin_packet {
struct SignedSkin {
  unsigned influences=0;
  std::vector<float> weights;
  std::vector<uint32_t> indices;
  std::vector<pc::FixedSkinMatrix> palette;
};
// Encode each authored contribution as abs(weight) times a signed matrix.
// The extra final influence points to a zero affine matrix, so the renderer's
// implicit remainder contributes zero whether positive or skipped. Every
// original influence remains explicit: B1..B4 become B2..B5, at most 33 bones.
// This is an adapter representation, not normalization of the game's weights.
inline bool EncodeSignedSkin(const Packet& packet,SignedSkin& output,Error* error=nullptr) {
  if(!Validate(packet,error))return false;
  try {
    SignedSkin result;result.influences=packet.influences+1;
    const auto bones=unsigned(packet.palette.size());
    result.palette.resize(bones*2+1); // Value-initialized final zero matrix.
    for(unsigned bone=0;bone<bones;++bone)for(unsigned row=0;row<3;++row)for(unsigned column=0;column<4;++column) {
      const float value=packet.palette[bone][row][column];
      result.palette[bone][row][column]=value;
      result.palette[bones+bone][row][column]=-value;
    }
    result.weights.resize(packet.vertices.size()*result.influences);
    result.indices.resize(result.weights.size());
    for(size_t vertex=0;vertex<packet.vertices.size();++vertex) {
      const auto& input=packet.vertices[vertex].skin;const auto offset=vertex*result.influences;float remainder=1;
      for(unsigned influence=0;influence<packet.influences;++influence) {
        const float weight=input.weights[influence],magnitude=std::fabs(weight);
        result.weights[offset+influence]=magnitude;
        result.indices[offset+influence]=input.indices[influence]+(weight<0?bones:0);
        remainder-=magnitude;
      }
      if(!Finite(remainder))return Fail(error,Error::Deformation);
      result.indices[offset+packet.influences]=bones*2;
      // Stored final weight is canonical zero; the pinned kernel ignores it.
    }
    output=std::move(result);return true;
  }catch(const std::bad_alloc&){return Fail(error,Error::Range);}
}
} // namespace winx_remix::skin_packet
