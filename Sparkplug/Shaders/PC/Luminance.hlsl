// Semantic reconstruction of the standalone PC luminance pixel program.
// Names and HLSL structure are reconstructed; this is not original HLSL,
// a bit-exact instruction listing, a backend, or a runtime integration.
// Compilation to ps_3_0 is separate from the original ps_1_1 instruction format.

sampler2D Image : register(s0);
// Reconstructed name: c0.rgb supplies weights; the program does not use c0.w.
float4 LuminanceWeights : register(c0);

struct LuminancePixelInput
{
    float2 TexCoord : TEXCOORD0;
};

struct LuminancePixelOutput
{
    float4 Color : COLOR0;
};

LuminancePixelOutput LuminancePS(LuminancePixelInput input)
{
    LuminancePixelOutput output;
    float4 texel = tex2D(Image, input.TexCoord);
    float intensity = dot(texel.rgb, LuminanceWeights.rgb);
    // Four squarings reproduce the sixteenth power without a pow-domain change.
    intensity *= intensity;
    intensity *= intensity;
    intensity *= intensity;
    intensity *= intensity;
    output.Color = float4(texel.rgb * intensity, texel.a);
    return output;
}
