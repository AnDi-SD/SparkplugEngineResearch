// Semantic reconstruction of three standalone PC blur stages. HLSL names and
// structures are reconstructed, not original source or bit-exact instructions.
// This file supplies no backend or runtime wiring. Shader-model 3 compilation
// checks the reconstruction independently of the original shader-model 1 ISA.
// Define SPARKPLUG_BLUR_VERTEX for BlurVS; pixel entries use the default branch.

#if defined(SPARKPLUG_BLUR_VERTEX)

float4 TextureSize : register(c0);
float4 Distance : register(c1);

struct BlurVertexInput
{
    float4 Position : POSITION0;
    float2 TexCoord : TEXCOORD0;
};

struct BlurVertexOutput
{
    float4 Position : POSITION0;
    float2 TexCoord0 : TEXCOORD0;
    float2 TexCoord1 : TEXCOORD1;
    float2 TexCoord2 : TEXCOORD2;
    float2 TexCoord3 : TEXCOORD3;
};

BlurVertexOutput BlurVS(BlurVertexInput input)
{
    BlurVertexOutput output;
    float2 positive = float2(input.Position.x > 0.0f ? 1.0f : 0.0f,
        input.Position.y > 0.0f ? 1.0f : 0.0f);
    float2 negative = float2(input.Position.x < 0.0f ? 1.0f : 0.0f,
        input.Position.y < 0.0f ? 1.0f : 0.0f);
    output.Position = float4(positive - negative, 0.0f, 1.0f);

    // Both axes use TextureSize.x, including the half-texel displacement.
    float inverseSize = rcp(TextureSize.x);
    float radius = inverseSize * Distance.x;
    float halfTexel = inverseSize * 0.5f;
    output.TexCoord0 = input.TexCoord + float2(halfTexel - radius, halfTexel);
    output.TexCoord1 = input.TexCoord + float2(halfTexel + radius, halfTexel);
    output.TexCoord2 = input.TexCoord + float2(halfTexel, halfTexel + radius);
    output.TexCoord3 = input.TexCoord + float2(halfTexel, halfTexel - radius);
    return output;
}

#else

sampler2D Image0 : register(s0);
sampler2D Image1 : register(s1);
sampler2D Image2 : register(s2);
sampler2D Image3 : register(s3);
// BlurLuminance's pixel c1 is distinct from BlurVS's Distance at vertex c1.
float4 Luminance : register(c1);

struct BlurPixelInput
{
    float2 TexCoord0 : TEXCOORD0;
    float2 TexCoord1 : TEXCOORD1;
    float2 TexCoord2 : TEXCOORD2;
    float2 TexCoord3 : TEXCOORD3;
};

struct BlurPixelOutput
{
    float4 Color : COLOR0;
};

float4 AverageBlurSamples(BlurPixelInput input)
{
    float4 average = tex2D(Image0, input.TexCoord0) * 0.25f;
    average += tex2D(Image1, input.TexCoord1) * 0.25f;
    average += tex2D(Image2, input.TexCoord2) * 0.25f;
    average += tex2D(Image3, input.TexCoord3) * 0.25f;
    return average;
}

BlurPixelOutput BlurPS(BlurPixelInput input)
{
    BlurPixelOutput output;
    output.Color = AverageBlurSamples(input);
    return output;
}

BlurPixelOutput BlurLuminancePS(BlurPixelInput input)
{
    BlurPixelOutput output;
    float3 average = AverageBlurSamples(input).rgb;
    float luminance = dot(average, Luminance.rgb);
    output.Color = float4(average * luminance, 1.0f);
    return output;
}

#endif
