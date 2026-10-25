// Semantic reconstruction of the Fixed.rfx vertex/pixel family.
// Analytic names and compile-time switches replace the EXE's source insertion.
// Original profiles: VS/PS 1.1. Check this readable form with VS/PS 3.0.
// Matrix declarations retain HLSL mul conventions. Reflection/register allocation
// is per specialization; it is not an ABI-compatible replacement shader.
#ifndef SPARKPLUG_FIXED_WEIGHTS
#define SPARKPLUG_FIXED_WEIGHTS 4
#endif
#ifndef SPARKPLUG_FIXED_COLOR_MODE
#define SPARKPLUG_FIXED_COLOR_MODE 4
#endif
#ifndef SPARKPLUG_FIXED_LIGHT_COUNT
#define SPARKPLUG_FIXED_LIGHT_COUNT 1
#endif
#ifndef SPARKPLUG_FIXED_SPECULAR
#define SPARKPLUG_FIXED_SPECULAR 0
#endif
#ifndef SPARKPLUG_FIXED_TEXCOORDS
#define SPARKPLUG_FIXED_TEXCOORDS 1
#endif
#ifndef SPARKPLUG_FIXED_UV_MASK
#define SPARKPLUG_FIXED_UV_MASK 0
#endif
#ifndef SPARKPLUG_FIXED_LIGHT0
#define SPARKPLUG_FIXED_LIGHT0 0
#endif
#ifndef SPARKPLUG_FIXED_LIGHT1
#define SPARKPLUG_FIXED_LIGHT1 0
#endif
#ifndef SPARKPLUG_FIXED_LIGHT2
#define SPARKPLUG_FIXED_LIGHT2 0
#endif
#ifndef SPARKPLUG_FIXED_LIGHT3
#define SPARKPLUG_FIXED_LIGHT3 0
#endif
#ifndef SPARKPLUG_FIXED_LIGHT4
#define SPARKPLUG_FIXED_LIGHT4 0
#endif
#ifndef SPARKPLUG_FIXED_LIGHT5
#define SPARKPLUG_FIXED_LIGHT5 0
#endif
#ifndef SPARKPLUG_FIXED_LIGHT6
#define SPARKPLUG_FIXED_LIGHT6 0
#endif
#ifndef SPARKPLUG_FIXED_LIGHT7
#define SPARKPLUG_FIXED_LIGHT7 0
#endif
#if SPARKPLUG_FIXED_WEIGHTS < 0 || SPARKPLUG_FIXED_WEIGHTS > 4
#error Unsupported authored weight count
#endif
#if SPARKPLUG_FIXED_LIGHT_COUNT < 0 || SPARKPLUG_FIXED_LIGHT_COUNT > 8
#error Unsupported light count
#endif
#if SPARKPLUG_FIXED_TEXCOORDS < 0 || SPARKPLUG_FIXED_TEXCOORDS > 8
#error Unsupported texture coordinate count
#endif

float4x4 ObjectViewProjection, WorldViewProjection, View;
float4x3 BonePalette[16];
float3x3 UvTransforms[8];
float4 AmbientMaterial, DiffuseMaterial, ConstantColor;
float SpecularPower;
float4 LightDiffuse[8], LightSpecular[8], LightDirection[8], LightPosition[8];
float4 LightAttenuation[8];
float LightInner[8], LightOuter[8];
static const int LightKinds[8] = {
    SPARKPLUG_FIXED_LIGHT0, SPARKPLUG_FIXED_LIGHT1,
    SPARKPLUG_FIXED_LIGHT2, SPARKPLUG_FIXED_LIGHT3,
    SPARKPLUG_FIXED_LIGHT4, SPARKPLUG_FIXED_LIGHT5,
    SPARKPLUG_FIXED_LIGHT6, SPARKPLUG_FIXED_LIGHT7
};

struct FixedVertexInput
{
    float4 position : POSITION0;
    float4 weights : BLENDWEIGHT0;
    float4 indices : BLENDINDICES0;
    float4 normal : NORMAL0;
    float4 color : COLOR0;
    float3 uv : TEXCOORD0;
};
struct FixedVertexOutput
{
    float4 position : POSITION0;
    float4 color : COLOR0;
#if SPARKPLUG_FIXED_TEXCOORDS > 0
    float3 uv0 : TEXCOORD0;
#endif
#if SPARKPLUG_FIXED_TEXCOORDS > 1
    float3 uv1 : TEXCOORD1;
#endif
#if SPARKPLUG_FIXED_TEXCOORDS > 2
    float3 uv2 : TEXCOORD2;
#endif
#if SPARKPLUG_FIXED_TEXCOORDS > 3
    float3 uv3 : TEXCOORD3;
#endif
#if SPARKPLUG_FIXED_TEXCOORDS > 4
    float3 uv4 : TEXCOORD4;
#endif
#if SPARKPLUG_FIXED_TEXCOORDS > 5
    float3 uv5 : TEXCOORD5;
#endif
#if SPARKPLUG_FIXED_TEXCOORDS > 6
    float3 uv6 : TEXCOORD6;
#endif
#if SPARKPLUG_FIXED_TEXCOORDS > 7
    float3 uv7 : TEXCOORD7;
#endif
};

FixedVertexOutput FixedVS(FixedVertexInput input)
{
    FixedVertexOutput result;
    float4 worldPosition = input.position;
    float4 worldNormal = input.normal;
#if SPARKPLUG_FIXED_WEIGHTS > 0
    float3 weightedPosition = 0;
    float3 weightedNormal = 0;
    // Every stored weight participates, including the last/zero/negative one.
    // No remainder weight or sum normalization is introduced.
    [unroll] for (int influence = 0; influence < SPARKPLUG_FIXED_WEIGHTS; ++influence)
    {
        int bone = (int)input.indices[influence];
        weightedPosition += mul(input.position, BonePalette[bone]) * input.weights[influence];
        weightedNormal += mul(input.normal.xyz, (float3x3)BonePalette[bone]) * input.weights[influence];
    }
    worldPosition.xyz = weightedPosition;
    worldNormal.xyz = weightedNormal;
    worldNormal = normalize(worldNormal); // w is preserved and participates.
    result.position = mul(WorldViewProjection, worldPosition);
#else
    result.position = mul(ObjectViewProjection, worldPosition);
#endif
    float4 viewPosition = mul(View, worldPosition);
    float3 viewNormal = normalize(mul((float3x3)View, worldNormal.xyz));
    float3 lighting = AmbientMaterial.rgb;
    [unroll] for (int light = 0; light < SPARKPLUG_FIXED_LIGHT_COUNT; ++light)
    {
        int kind = LightKinds[light]; // directional=0, point=1, spot=2.
        if (kind < 0 || kind > 2) continue; // Original has no branch for other types.
        float3 toLight;
        if (kind == 0) toLight = -LightDirection[light].xyz;
        else toLight = normalize(LightPosition[light] - viewPosition).xyz;
        // Point/spot normalization above is float4 BEFORE selecting xyz.
        float normalDot = dot(viewNormal, toLight);
#if SPARKPLUG_FIXED_SPECULAR
        float3 viewDirection = kind == 0 ? float3(0, 0, -1) : normalize(-viewPosition).xyz;
        float3 halfVector = normalize(viewDirection + toLight);
        float4 coefficients = lit(normalDot, dot(viewNormal, halfVector), SpecularPower);
        float3 contribution = LightDiffuse[light].rgb * coefficients.y
                            + LightSpecular[light].rgb * coefficients.z;
        if (kind == 2)
        {
            float cone = clamp((dot(toLight, -LightDirection[light].xyz) - LightOuter[light])
                             / (LightInner[light] - LightOuter[light]), 0, 1);
            contribution *= cone;
        }
        lighting += contribution; // Specular point branch has no attenuation.
#else
        if (kind == 2)
        {
            float cone = clamp((dot(toLight, -LightDirection[light].xyz) - LightOuter[light])
                             / (LightInner[light] - LightOuter[light]), 0, 1);
            lighting += cone * (LightDiffuse[light].rgb * normalDot);
            // Original spot diffuse retains a negative dot; no range attenuation.
        }
        else if (normalDot > 0)
        {
            lighting += LightDiffuse[light].rgb * normalDot;
            if (kind == 1)
            {
                float distance = length(LightPosition[light].xyz - viewPosition.xyz);
                lighting *= 1.0 / (LightAttenuation[light].x + LightAttenuation[light].y * distance);
                // Scales ALL accumulated lighting, so light order matters.
            }
        }
#endif
    }
#if SPARKPLUG_FIXED_COLOR_MODE == 0
    result.color = 1;
#elif SPARKPLUG_FIXED_COLOR_MODE == 1
    result.color = DiffuseMaterial;
#elif SPARKPLUG_FIXED_COLOR_MODE == 2
    result.color = input.color;
#elif SPARKPLUG_FIXED_COLOR_MODE == 4
    result.color = float4(lighting + input.color.rgb, DiffuseMaterial.a);
#elif SPARKPLUG_FIXED_COLOR_MODE == 5
    result.color = float4(lighting, DiffuseMaterial.a) * input.color;
#elif SPARKPLUG_FIXED_COLOR_MODE == 6
    result.color = ConstantColor;
#else
    result.color = float4(lighting, DiffuseMaterial.a);
#endif
    // Each enabled output starts at UV0; the third lane is replaced by 1.
    float3 uv = float3(input.uv.xy, 1);
#if SPARKPLUG_FIXED_TEXCOORDS > 0
#if SPARKPLUG_FIXED_UV_MASK & 1
    result.uv0 = mul(UvTransforms[0], uv);
#else
    result.uv0 = uv;
#endif
#endif
#if SPARKPLUG_FIXED_TEXCOORDS > 1
#if SPARKPLUG_FIXED_UV_MASK & 2
    result.uv1 = mul(UvTransforms[1], uv);
#else
    result.uv1 = uv;
#endif
#endif
#if SPARKPLUG_FIXED_TEXCOORDS > 2
#if SPARKPLUG_FIXED_UV_MASK & 4
    result.uv2 = mul(UvTransforms[2], uv);
#else
    result.uv2 = uv;
#endif
#endif
#if SPARKPLUG_FIXED_TEXCOORDS > 3
#if SPARKPLUG_FIXED_UV_MASK & 8
    result.uv3 = mul(UvTransforms[3], uv);
#else
    result.uv3 = uv;
#endif
#endif
#if SPARKPLUG_FIXED_TEXCOORDS > 4
#if SPARKPLUG_FIXED_UV_MASK & 16
    result.uv4 = mul(UvTransforms[4], uv);
#else
    result.uv4 = uv;
#endif
#endif
#if SPARKPLUG_FIXED_TEXCOORDS > 5
#if SPARKPLUG_FIXED_UV_MASK & 32
    result.uv5 = mul(UvTransforms[5], uv);
#else
    result.uv5 = uv;
#endif
#endif
#if SPARKPLUG_FIXED_TEXCOORDS > 6
#if SPARKPLUG_FIXED_UV_MASK & 64
    result.uv6 = mul(UvTransforms[6], uv);
#else
    result.uv6 = uv;
#endif
#endif
#if SPARKPLUG_FIXED_TEXCOORDS > 7
#if SPARKPLUG_FIXED_UV_MASK & 128
    result.uv7 = mul(UvTransforms[7], uv);
#else
    result.uv7 = uv;
#endif
#endif
    return result; // COLOR packing/saturation and texture stages are external.
}

float4 FixedPS(float4 interpolatedColor : COLOR0) : COLOR0
{
    return interpolatedColor; // No texture sampling in this original PS.
}
