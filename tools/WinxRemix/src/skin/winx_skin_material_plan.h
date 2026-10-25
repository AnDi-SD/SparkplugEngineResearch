#pragma once
// Own initial RT material policy for a separately verified Fixed ColorMode 4.
// This is not reconstruction of game lighting and is not a shader/draw gate.
// Original ambient and directional terms belong to scene lighting; the authored
// additive vertex RGB is preserved as an emissive term under this RT policy.
#include "winx_skin_packet.h"
namespace winx_remix::skin_material {
// Caller must separately qualify ColorMode4, no specular/PS, a single texture
// multiplying RGB/alpha, UV0, supported sampler/alpha/blend and current owners.
// MatDiffuse RGB is our reflectance choice; this does not infer light intensity
// from LightMatDiff or ambient color from the already-multiplied AmbientCol.
inline bool ProjectColor4(const skin_packet::Packet& packet,
    const skin_packet::pc::FixedSkinVector& materialDiffuse,material_channels::Plan& output) {
  if(!skin_packet::Validate(packet)||packet.attributes!=3||materialDiffuse[3]!=1)return false;
  for(const auto component:materialDiffuse)if(!std::isfinite(component)||component<0||component>1)return false;
  const auto first=packet.vertices[0].color;bool uniform=true;
  for(const auto& vertex:packet.vertices)uniform&=(vertex.color&0xffffff)==(first&0xffffff);
  material_channels::Input input{};
  input.material.Diffuse={materialDiffuse[0],materialDiffuse[1],materialDiffuse[2],materialDiffuse[3]};
  input.diffuseSource=D3DMCS_MATERIAL;input.ambientSource=D3DMCS_MATERIAL;input.emissiveSource=D3DMCS_COLOR1;
  material_channels::Plan result;
  // Reuse the common one-vertex-factor decomposition, including its refusal of
  // variable additive RGB alongside constant nonzero albedo. Never flatten it.
  if(!material_channels::Factor(input,uniform,first,result))return false;
  output=result;return true;
}
} // namespace winx_remix::skin_material
