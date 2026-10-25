// Semantic reconstruction of the standalone PC point-light shadow-volume VS.
// Names and HLSL structure are reconstructed, not original HLSL or bit-exact ISA.
// Shader-model 3 compilation checks this source independently of original vs_1_1;
// this file does not provide a backend or select a runtime shadow technique.

float4 view_proj_matrix[4] : register(c0);
float4 LightPos : register(c4);
float4 Range : register(c5);

struct ShadowVolumePointVertexInput
{
    // w blends the source vertex (1) with its radial extrusion (0).
    float4 Position : POSITION0;
};

struct ShadowVolumePointVertexOutput
{
    float4 Position : POSITION0;
    float4 Color : COLOR0;
};

ShadowVolumePointVertexOutput ShadowVolumePointVS(ShadowVolumePointVertexInput input)
{
    ShadowVolumePointVertexOutput output;
    float3 delta = input.Position.xyz - LightPos.xyz;
    float distanceSquared = dot(delta, delta);
    float inverseDistance = rsqrt(distanceSquared);
    float distance = inverseDistance * distanceSquared;
    float beyondRange = Range.x < distance ? 1.0f : 0.0f;
    float extrusionDistance = Range.x + (distance - Range.x) * beyondRange;
    float3 direction = delta * rcp(distance);
    float3 extruded = extrusionDistance * direction + LightPos.xyz;
    float3 position = (input.Position.xyz - extruded) * input.Position.w + extruded;

    output.Position = position.x * view_proj_matrix[0]
        + position.y * view_proj_matrix[1]
        + position.z * view_proj_matrix[2]
        + view_proj_matrix[3];
    output.Color = float4(1.0f, 0.0f, 1.0f, 1.0f);
    return output;
}
