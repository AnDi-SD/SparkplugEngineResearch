using System.Numerics;

namespace SmoViewer.Sparkplug;

/// <summary>Only marked fields were produced by the original PC shader constants.
/// Known bits: diffuse 1, specular 2, position 4, direction 8, attenuation 16,
/// inner 32, outer 64. Unmarked values are transport padding.</summary>
public sealed record SparkplugShaderLight(uint Type, uint KnownMask, Vector4 Diffuse,
    Vector4 Specular, Vector4 Position, Vector4 Direction, Vector4 Attenuation, float Inner, float Outer);
public sealed record SparkplugShaderLighting(uint ColorMode, bool UseSpecular, uint KnownMask,
    Vector4 Ambient, Vector4 Diffuse, Vector4 ConstantColor, float Power, IReadOnlyList<SparkplugShaderLight> Lights);

public sealed partial class SparkplugSceneRuntime
{
    /// <summary>Project the actual selected RenderNode cache through common PC
    /// constant producers. The host supplies the game-world to shader-view matrix
    /// and renderer constant ARGB; no light selection or color math is repeated here.</summary>
    public unsafe SparkplugShaderLighting CaptureShaderLighting(int renderNodeObjectIndex,
        int materialObjectIndex, Matrix4x4 gameWorldToView, uint constantColor = 0)
    {
        ObjectDisposedException.ThrowIf(_handle.IsClosed, this);
        if ((uint)renderNodeObjectIndex >= (uint)_documentObjectIds.Length)
            throw new ArgumentOutOfRangeException(nameof(renderNodeObjectIndex));
        if ((uint)materialObjectIndex >= (uint)_documentObjectIds.Length)
            throw new ArgumentOutOfRangeException(nameof(materialObjectIndex));
        if (!_lightingConfigured) throw new InvalidDataException("Scene lighting is not configured.");
        NativeMethods.Check(NativeMethods.spv_scene_shader_lighting(_handle,
            _documentObjectIds[renderNodeObjectIndex], _documentObjectIds[materialObjectIndex],
            (float*)&gameWorldToView, constantColor, out var row));
        if (row.Count > 8) throw new InvalidDataException("Shader light count exceeds ABI capacity.");
        static Vector4 Vector(float* p) => new(p[0],p[1],p[2],p[3]);
        var lights = new SparkplugShaderLight[row.Count];
        var rows = (NativeMethods.ShaderLight*)row.Lights;
        for (int i = 0; i < lights.Length; ++i)
        {
            var light = rows[i];
            lights[i] = new(light.Type,light.Known,Vector(light.Diffuse),Vector(light.Specular),
                Vector(light.Position),Vector(light.Direction),Vector(light.Attenuation),light.Inner,light.Outer);
        }
        return new(row.ColorMode,row.Specular != 0,row.Known,Vector(row.Ambient),Vector(row.Diffuse),
            Vector(row.ConstantColor),row.Power,Array.AsReadOnly(lights));
    }
}
