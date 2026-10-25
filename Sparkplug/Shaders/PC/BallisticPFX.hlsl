// Semantic reconstruction of the standalone PC BallisticPFX vertex program.
// Identifiers and HLSL structure are reconstructed; this is not original HLSL,
// a bit-exact instruction listing, a backend, or a runtime integration.
// See docs/engine/effects/ballistic-shader.md for the expression contract.
// Compilation to vs_3_0 validates HLSL independently of the original vs_1_1 ISA.

float4 view_proj_matrix[4] : register(c0);
float4 vAccelBegin : register(c4);
float4 vAccelEnd : register(c5);
float4 vCameraPos : register(c6);
// x: current time; y: loop parameter; z/w: initial/final point scale.
float4 Time_Loop_Scales : register(c7);
float4 fFrameBufWidth : register(c8);
float4 ColorBegin : register(c9);
float4 ColorEnd : register(c10);

struct BallisticVertexInput
{
    float4 Position : POSITION0;
    float3 Velocity : NORMAL0;
    float2 BirthTimeAndLifetime : TEXCOORD0;
};

struct BallisticVertexOutput
{
    float4 Position : POSITION0;
    float4 Color : COLOR0;
    float PointSize : PSIZE;
};

BallisticVertexOutput BallisticVS(BallisticVertexInput input)
{
    BallisticVertexOutput output;
    float lifetime = input.BirthTimeAndLifetime.y;
    float elapsed = Time_Loop_Scales.x - input.BirthTimeAndLifetime.x;
    float expired = elapsed >= lifetime ? 1.0f : 0.0f;
    float looping = Time_Loop_Scales.y >= 0.5f ? 1.0f : 0.0f;
    // The original expression subtracts one lifetime, even after several loops.
    float age = elapsed - (expired * looping) * lifetime;
    float ageSquared = age * age;
    float normalizedAge = age * rcp(lifetime);

    float3 position = age * input.Velocity + input.Position.xyz;
    position += (ageSquared * vAccelBegin.xyz) * 0.5f;
    float3 accelerationDelta = vAccelBegin.xyz - vAccelEnd.xyz;
    // This decimal is the program's constant, rather than an exact 1/6.
    position += ((ageSquared * accelerationDelta) * normalizedAge) * 0.166667f;

    // Constants are uploaded basis vectors. Projection uses homogeneous 1,
    // independently of the input Position.w used in point-size distance below.
    output.Position = position.x * view_proj_matrix[0]
        + position.y * view_proj_matrix[1]
        + position.z * view_proj_matrix[2]
        + view_proj_matrix[3];

    float4 originalCameraDelta = input.Position - vCameraPos;
    float inverseDistance = rsqrt(dot(originalCameraDelta, originalCameraDelta));
    float remaining = 1.0f - normalizedAge;
    float scale = normalizedAge * Time_Loop_Scales.w
        + remaining * Time_Loop_Scales.z;
    float lowerMask = normalizedAge >= 0.0f ? 1.0f : 0.0f;
    float upperMask = normalizedAge <= 1.0f ? 1.0f : 0.0f;
    output.PointSize = inverseDistance
        * (((scale * lowerMask) * upperMask) * fFrameBufWidth.x);
    output.Color = normalizedAge * ColorEnd + remaining * ColorBegin;
    return output;
}
