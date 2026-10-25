// Semantic reconstruction of the effective Bumpmap.rfx VS 1.1 / PS 1.1 pair.
// The source defaults BlendWeightCount to zero before its insertion marker.
// Reconstructed switches expose its 0..4-weight specialization; automatic game
// selection of these Bumpmap variants is not established. Bindings are analytic.
// Compile this readable form as VS/PS 3.0; no original instruction identity,
// live material use, or automatic Remix compatibility is asserted.
#if defined(SPARKPLUG_BUMPMAP_VERTEX)
#ifndef SPARKPLUG_BUMPMAP_WEIGHTS
#define SPARKPLUG_BUMPMAP_WEIGHTS 0
#endif
#if SPARKPLUG_BUMPMAP_WEIGHTS < 0 || SPARKPLUG_BUMPMAP_WEIGHTS > 4
#error Unsupported authored weight count
#endif
float4x4 ObjectViewProjection;
float4x4 WorldViewProjection;
float4x4 InverseView;
float4 LightPositionInView;
float4x3 BonePalette[16];

struct BumpmapVertexInput
{
    float4 position : POSITION0;
    float4 weights : BLENDWEIGHT0;
    float4 indices : BLENDINDICES0;
    float3 normal : NORMAL0;
    float3 tangent : TANGENT0;
    float3 binormal : BINORMAL0;
    float2 uv : TEXCOORD0;
};
struct BumpmapVertexOutput
{
    float4 position : POSITION0;
    float3 light : COLOR0;
    float3 halfVector : COLOR1;
    float2 uv0 : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
};

BumpmapVertexOutput BumpmapVS(BumpmapVertexInput input)
{
    BumpmapVertexOutput result;
    float4 position = input.position;
    float3 normal = input.normal, tangent = input.tangent, binormal = input.binormal;
#if SPARKPLUG_BUMPMAP_WEIGHTS > 0
    float3 weightedPosition = 0, weightedNormal = 0, weightedTangent = 0, weightedBinormal = 0;
    [unroll] for (int influence = 0; influence < SPARKPLUG_BUMPMAP_WEIGHTS; ++influence)
    {
        int bone = (int)input.indices[influence];
        float weight = input.weights[influence];
        weightedPosition += mul(input.position, BonePalette[bone]) * weight;
        weightedNormal += mul(input.normal, (float3x3)BonePalette[bone]) * weight;
        weightedTangent += mul(input.tangent, (float3x3)BonePalette[bone]) * weight;
        weightedBinormal += mul(input.binormal, (float3x3)BonePalette[bone]) * weight;
    }
    position.xyz = weightedPosition;
    normal = weightedNormal;
    tangent = weightedTangent;
    binormal = weightedBinormal; // Original does not normalize the deformed frame.
    result.position = mul(WorldViewProjection, position);
#else
    result.position = mul(ObjectViewProjection, position);
#endif
    float4 worldLight = mul(InverseView, LightPositionInView);
    float3 toLight = normalize(worldLight.xyz - position.xyz);
    // The camera-origin float4 is normalized before the xyz truncation.
    float3 viewDirection = normalize(mul(InverseView, float4(0, 0, 0, 1))).xyz;
    float3 halfVector = normalize(viewDirection + toLight);
    result.light = float3(dot(toLight, tangent), dot(toLight, binormal),
                          dot(toLight, normal)) * 0.5 + 0.5;
    result.halfVector = float3(dot(halfVector, tangent), dot(halfVector, binormal),
                               dot(halfVector, normal)) * 0.5 + 0.5;
    result.uv0 = input.uv;
    result.uv1 = input.uv;
    return result;
}
#else
sampler2D BaseTexture : register(s0);
sampler2D NormalTexture : register(s1);
float4 AmbientColor : register(c0);
float4 LightMaterialDiffuse : register(c1);
float4 LightMaterialSpecular : register(c2);

float4 BumpmapPS(float3 encodedLight : COLOR0, float3 encodedHalf : COLOR1,
                float2 baseUv : TEXCOORD0, float2 normalUv : TEXCOORD1) : COLOR0
{
    float4 base = tex2D(BaseTexture, baseUv);
    float3 normal = tex2D(NormalTexture, normalUv).xyz * 2 - 1;
    float3 light = encodedLight * 2 - 1;
    float3 halfVector = encodedHalf * 2 - 1;
    float specular = dot(normal, halfVector);
    specular *= specular;
    specular *= specular;
    specular *= specular; // Power 8, despite the source comment saying 32.
    float4 diffuse = dot(normal, light) * LightMaterialDiffuse + AmbientColor;
    // Original final PS arithmetic covers RGBA, including the specular alpha.
    return base * diffuse + specular * LightMaterialSpecular;
}
#endif
