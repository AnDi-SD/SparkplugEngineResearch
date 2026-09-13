using OpenTK.Graphics.OpenGL4;
using SmoViewer.Core;
namespace SmoViewer.Rendering.Wpf;

public sealed partial class SmoGpuSceneRenderer
{
    private readonly Dictionary<SmoRenderObjectKey, string> _fogIssues = [];
    private int _fogModeLocation, _fogColorLocation, _fogStartEndLocation, _fogDensityLocation, _fogEyeDepthLocation;
    public bool ShowFog { get; set; } = true;
    public int LastFogPlacementCount { get; private set; }

    private void InitializeFogUniforms()
    {
        _fogModeLocation = GL.GetUniformLocation(_program, "uFogMode");
        _fogColorLocation = GL.GetUniformLocation(_program, "uFogColor");
        _fogStartEndLocation = GL.GetUniformLocation(_program, "uFogStartEnd");
        _fogDensityLocation = GL.GetUniformLocation(_program, "uFogDensity");
        _fogEyeDepthLocation = GL.GetUniformLocation(_program, "uFogEyeDepth");
    }

    private void BindFog(GpuRenderItem item)
    {
        GL.Uniform1(_fogModeLocation, 0);
        _fogIssues.Remove(item.Key);
        if (!ShowFog || item.IsSky || RenderMode == SmoSceneRenderMode.Wireframe) return;
        var fog = item.FogDraw;
        if (fog is null) return; // Host-created overlays have no game Fog.
        if (fog.Issue is not null || (fog.KnownMask & 1) == 0 || fog.Enabled > 1)
        { _fogIssues[item.Key] = $"FOG_GPU_UNAVAILABLE: {item.Key}: {fog.Issue ?? "Unknown enable state"}."; return; }
        if (fog.Enabled == 0) return;
        if (item.BoneMatrices.Length != 0)
        {
            _fogIssues[item.Key] = $"FOG_SHADER_UNVERIFIED: {item.Key}: the shipped weighted shader has no oFog producer.";
            return;
        }
        uint required = fog.Mode == 3 ? 31u : 39u;
        if (fog.Mode is < 1 or > 3 || (fog.KnownMask & required) != required ||
            (fog.Mode == 3 && (!float.IsFinite(fog.Start) || !float.IsFinite(fog.End) || fog.End <= fog.Start)) ||
            (fog.Mode != 3 && (!float.IsFinite(fog.Density) || fog.Density is < 0 or > 1)))
        { _fogIssues[item.Key] = $"FOG_GPU_UNAVAILABLE: {item.Key}: unsupported consumed device parameters."; return; }
        GL.Uniform3(_fogColorLocation, ((fog.Color >> 16) & 255) / 255f,
            ((fog.Color >> 8) & 255) / 255f, (fog.Color & 255) / 255f);
        if (fog.Mode == 3) GL.Uniform2(_fogStartEndLocation, fog.Start, fog.End);
        else GL.Uniform1(_fogDensityLocation, fog.Density);
        GL.Uniform1(_fogModeLocation, (int)fog.Mode);
        ++LastFogPlacementCount;
    }

    // Modern device implementation of D3D9 pixel fog: eye-relative W for
    // perspective, device Z for affine projection; neither uses radial range.
    // Fog changes RGB after texture stages and alpha test, before framebuffer
    // blending. Its packed alpha is ignored. No legacy graphics API is loaded.
    // https://learn.microsoft.com/en-us/windows/win32/direct3d9/pixel-fog
    // https://learn.microsoft.com/en-us/windows/win32/direct3d9/fog-formulas
    // https://learn.microsoft.com/en-us/windows/win32/direct3d9/fog-color
    private const string FogFragmentSource = """
        uniform int uFogMode;
        uniform int uFogEyeDepth;
        uniform vec3 uFogColor;
        uniform vec2 uFogStartEnd;
        uniform float uFogDensity;
        vec4 ApplyPixelFog(vec4 color)
        {
            if (uFogMode == 0) return color;
            float depth = uFogEyeDepth != 0 ? abs(1.0 / gl_FragCoord.w) : gl_FragCoord.z;
            float factor;
            if (uFogMode == 3) factor = (uFogStartEnd.y - depth) / (uFogStartEnd.y - uFogStartEnd.x);
            else if (uFogMode == 1) factor = exp(-uFogDensity * depth);
            else { float product = uFogDensity * depth; factor = exp(-product * product); }
            return vec4(mix(uFogColor, color.rgb, clamp(factor, 0.0, 1.0)), color.a);
        }
        """;
}
