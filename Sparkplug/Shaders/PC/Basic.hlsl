// Semantic reconstruction of BasicShader.vsh (original VS 1.1).
// Names and HLSL entry point are reconstructed. This is not extracted asset text,
// an instruction-identical decompilation, or an installed rendering backend.
// The four uploaded registers are vectors, not a guessed matrix orientation.
float4 ViewProjectionRegisters[4] : register(c0);

float4 BasicVS(float4 position : POSITION0) : POSITION0
{
    float4 clip = position.x * ViewProjectionRegisters[0];
    clip = position.y * ViewProjectionRegisters[1] + clip;
    clip = position.z * ViewProjectionRegisters[2] + clip;
    return position.w * ViewProjectionRegisters[3] + clip;
}
