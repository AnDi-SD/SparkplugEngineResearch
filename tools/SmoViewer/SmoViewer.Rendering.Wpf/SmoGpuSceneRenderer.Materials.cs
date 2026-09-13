using System.Diagnostics;
using System.IO;
using System.Numerics;
using OpenTK.Graphics.OpenGL4;
using SmoViewer.Core;
using SmoViewer.Scene;

namespace SmoViewer.Rendering.Wpf;

public sealed partial class SmoGpuSceneRenderer
{
    private int _materialEnabledLocation, _materialColorModeLocation, _materialDiffuseLocation,
        _materialEmissiveLocation, _materialAlphaTestLocation, _materialAlphaReferenceLocation,
        _materialAlphaFunctionLocation, _materialStageCountLocation;
    private readonly int[] _stageColorLocations = new int[8], _stageAlphaLocations = new int[8],
        _stagePresentLocations = new int[8], _stageCoordinateLocations = new int[8],
        _stageTransformLocations = new int[8], _stageUVLocations = new int[8];
    private readonly Dictionary<MaterialSamplerKey, int> _materialSamplers = [];
    private readonly Dictionary<int, MaterialRuntimeBinding> _materialRuntimes = [];
    private readonly Dictionary<SmoRenderObjectKey, string> _materialIssues = [];
    private uint _materialFrame;
    private long _materialTimestamp;
    public IReadOnlyCollection<string> MaterialIssues => _materialIssues.Values;

    private sealed record MaterialRuntimeBinding(SparkplugMaterialRuntime Runtime)
    {
        public HashSet<int> Materials { get; } = [];
        public HashSet<int> Controllers { get; } = [];
        public HashSet<int> DynamicMaterials { get; } = [];
        public void Register(SmoLoadedMaterial material)
        {
            if (!Materials.Add(material.ObjectIndex)) return;
            var controllers = Runtime.ReferencedControllers(material.ObjectIndex);
            if (controllers.Count > 0) DynamicMaterials.Add(material.ObjectIndex);
            Controllers.UnionWith(controllers);
        }
    }

    /// <summary>Borrow the application's actual graph. The host owns its lifetime.
    /// Only materials with controllers are evaluated again during rendering.</summary>
    public void SetMaterialRuntime(int fileIndex, SparkplugMaterialRuntime runtime)
    {
        ArgumentNullException.ThrowIfNull(runtime);
        var binding = new MaterialRuntimeBinding(runtime);
        foreach (var item in _items.Where(item => item.Key.FileIndex == fileIndex && item.Material is not null))
            binding.Register(item.Material!);
        _materialRuntimes[fileIndex] = binding;
    }

    private void RegisterMaterialTextures(int fileIndex, SmoSceneMesh mesh)
    {
        if (mesh.MaterialDraw is not null)
            foreach (var stage in mesh.MaterialDraw.Passes.SelectMany(pass => pass.Stages))
                if (stage.Texture?.Texture is SmoTexture texture) RegisterTexture(fileIndex, texture);
        if (mesh.LoadedMaterial is null) return;
        foreach (var layer in mesh.LoadedMaterial.Passes.SelectMany(pass => pass.Layers))
        {
            if (layer.Texture?.Texture is SmoTexture texture) RegisterTexture(fileIndex, texture);
            if (layer.Animation is not null)
                foreach (var key in layer.Animation.Keys)
                    if (key.Texture?.Texture is SmoTexture frame) RegisterTexture(fileIndex, frame);
        }
        if (_materialRuntimes.TryGetValue(fileIndex, out var binding)) binding.Register(mesh.LoadedMaterial);
    }

    private void ClearMaterialBindings()
    {
        _materialRuntimes.Clear(); _materialIssues.Clear(); _materialTimestamp = 0; _materialFrame = 0;
        _lightingRuntimes.Clear();
        _shaderLightingCaptures.Clear();
        _alphaInputs.Clear();_lastAlphaOrder.Clear();AlphaIssue=null;
        _skyIssues.Clear();_lastSkyAlphaOrder.Clear();LastSkyPlacementCount=0;
        _fogIssues.Clear();LastFogPlacementCount=0;
    }

    private void BeginMaterialFrame()
    {
        _fogIssues.Clear();LastFogPlacementCount=0;
        UpdateTextWorlds();
        _shaderLightingCaptures.Clear();
        long now = Stopwatch.GetTimestamp();
        float elapsed = _materialTimestamp == 0 ? 0 : (float)((now - _materialTimestamp) / (double)Stopwatch.Frequency);
        _materialTimestamp = now; ++_materialFrame;
        // Explicit asset-preview clock. This does not reconstruct the game's
        // visibility traversal or AnimationManager schedule. No uniform texture timer.
        foreach (var binding in _materialRuntimes.Values)
            if (binding.Controllers.Count > 0) binding.Runtime.ApplyControllers(binding.Controllers, elapsed);
    }

    private void InitializeMaterialUniforms()
    {
        int Location(string name) => GL.GetUniformLocation(_program, name);
        _materialEnabledLocation = Location("uMaterialEnabled");
        _materialColorModeLocation = Location("uMaterialColorMode");
        _materialDiffuseLocation = Location("uMaterialDiffuse");
        _materialEmissiveLocation = Location("uMaterialEmissive");
        _materialAlphaTestLocation = Location("uMaterialAlphaTest");
        _materialAlphaReferenceLocation = Location("uMaterialAlphaReference");
        _materialAlphaFunctionLocation = Location("uMaterialAlphaFunction");
        _materialStageCountLocation = Location("uMaterialStageCount");
        for (int s = 0; s < 8; ++s)
        {
            GL.Uniform1(Location($"uStageTexture{s}"), s);
            _stageColorLocations[s] = Location($"uStageColor[{s}]");
            _stageAlphaLocations[s] = Location($"uStageAlpha[{s}]");
            _stagePresentLocations[s] = Location($"uStagePresent[{s}]");
            _stageCoordinateLocations[s] = Location($"uStageCoordinates[{s}]");
            _stageTransformLocations[s] = Location($"uStageTransformFlags[{s}]");
            _stageUVLocations[s] = Location($"uStageUV[{s}]");
        }
    }

    private void DrawMaterial(GpuRenderItem item, GpuGeometry geometry, GpuAppearance appearance)
    {
        try
        {
            var draw = item.MaterialDraw;
            if(item.TextObjectIndex is int text&&_materialRuntimes.TryGetValue(item.Key.FileIndex,out var textRuntime))
                draw=textRuntime.Runtime.CaptureTextDraw(text,_materialFrame);
            else if (item.Material is not null && _materialRuntimes.TryGetValue(item.Key.FileIndex, out var runtime) &&
                runtime.DynamicMaterials.Contains(item.Material.ObjectIndex))
                draw = runtime.Runtime.CaptureDraw(item.Material.ObjectIndex, _materialFrame);
            if (draw is null) throw new InvalidDataException("No common material draw snapshot.");
            foreach (var pass in draw.Passes) ValidateMaterialPass(item, pass);
            bool originalLighting = BindShaderLighting(item);
            GL.Uniform1(_materialEnabledLocation, 1);
            GL.Uniform1(_materialColorModeLocation, (int)(item.Material?.RenderStates[8] ?? 2));
            GL.BindVertexArray(geometry.VertexArray);
            foreach (var pass in draw.Passes)
            {
                ApplyMaterialRaster(pass.Raster, appearance.Opacity);
                GL.Uniform4(_materialDiffuseLocation, pass.Diffuse.X, pass.Diffuse.Y, pass.Diffuse.Z, pass.Diffuse.W);
                GL.Uniform4(_materialEmissiveLocation, pass.Emissive.X, pass.Emissive.Y, pass.Emissive.Z, pass.Emissive.W);
                int activeStages = 0;
                while (activeStages < 8 && pass.Stages[activeStages].ColorOperation != 1 && pass.Stages[activeStages].Texture is not null) ++activeStages;
                GL.Uniform1(_materialStageCountLocation, activeStages);
                for (int s = 0; s < activeStages; ++s)
                {
                    var stage = pass.Stages[s];
                    int texture = _whiteTexture;
                    bool present = stage.Texture?.Texture is not null &&
                        _textures.TryGetValue(new GpuTextureKey(item.Key.FileIndex, stage.Texture.ObjectIndex), out texture);
                    GL.ActiveTexture(TextureUnit.Texture0 + s);
                    GL.BindTexture(TextureTarget.Texture2D, present ? texture : _whiteTexture);
                    GL.BindSampler(s, MaterialSampler(stage));
                    GL.Uniform1(_stageColorLocations[s], (int)stage.ColorOperation);
                    GL.Uniform1(_stageAlphaLocations[s], (int)stage.AlphaOperation);
                    GL.Uniform1(_stagePresentLocations[s], present ? 1 : 0);
                    GL.Uniform1(_stageCoordinateLocations[s], (int)stage.Coordinates);
                    GL.Uniform1(_stageTransformLocations[s], (int)stage.TransformFlags);
                    if (stage.UVTransform is Matrix4x4 uv) SetMatrix(_stageUVLocations[s], uv);
                }
                GL.DrawElements(PrimitiveType.Triangles, geometry.IndexCount, DrawElementsType.UnsignedInt, 0);
            }
            _materialIssues.Remove(item.Key);
            if (!originalLighting)
                _materialIssues[item.Key] = $"SHADER_LIGHTING_PREVIEW: {item.Key}: weighted material has no bound scene light cache.";
        }
        catch (Exception error) when (error is InvalidDataException or ArgumentOutOfRangeException or ObjectDisposedException)
        {
            _materialIssues[item.Key] = $"MATERIAL_GPU_UNAVAILABLE: {item.Key}: {error.Message}";
        }
        finally { GL.ActiveTexture(TextureUnit.Texture0); }
    }

    private static void ValidateMaterialPass(GpuRenderItem item, SmoMaterialDrawPass pass)
    {
        var r = pass.Raster;
        if (r.KnownMask != 0xffff || r.DepthEnable > 1 || r.DepthWrite > 1 ||
            r.FillMode is < 1 or > 3 || r.ShadeMode != 2 || r.CullMode is < 1 or > 3 ||
            r.DepthFunction is < 1 or > 8 || r.AlphaFunction is < 1 or > 8 || r.AlphaReference > 255)
            throw new InvalidDataException("Unknown or unsupported raster state.");
        _ = BlendFactor(r.SourceBlend); _ = BlendFactor(r.DestinationBlend);
        if (pass.Stages.Count != 8) throw new InvalidDataException("Expected eight submitted texture stages.");
        foreach (var s in pass.Stages)
        {
            if (s.KnownMask != 1023) throw new InvalidDataException("Texture stage contains untouched state.");
            if (s.ColorOperation == 1 || s.Texture is null) break;
            if (s.Texture.Texture is null) throw new InvalidDataException(s.Texture.Issue ?? "Texture upload missing.");
            if (s.ColorOperation is not (2 or 3 or 4 or 5 or 6 or 7 or 10 or 12 or 13 or 15 or 16 or 18 or 19 or 20 or 21) ||
                s.AlphaOperation is not (2 or 3 or 4 or 5 or 6 or 7 or 10 or 12 or 13 or 15 or 16))
                throw new InvalidDataException("Unsupported or undefined texture operation.");
            if (s.Coordinates > 1 || (s.Coordinates == 0 && !item.HasUv0) || (s.Coordinates == 1 && !item.HasUv1 && item.BoneMatrices.Length == 0))
                throw new InvalidDataException("Requested texture coordinates are unavailable.");
            if (s.TransformFlags != 0 && (s.UVTransform is null || (s.TransformFlags & ~0x107u) != 0 || (s.TransformFlags & 7) is < 2 or > 4))
                throw new InvalidDataException("UV transform is missing or unsupported.");
        }
    }

    private void ApplyMaterialRaster(SmoMaterialRasterState r, float opacity)
    {
        if (r.DepthEnable == 0) GL.Disable(EnableCap.DepthTest); else GL.Enable(EnableCap.DepthTest);
        GL.DepthMask(r.DepthWrite != 0 && opacity >= .999f);
        GL.DepthFunc(r.DepthFunction switch {
            1 => DepthFunction.Never, 2 => DepthFunction.Less, 3 => DepthFunction.Equal,
            4 => DepthFunction.Lequal, 5 => DepthFunction.Greater, 6 => DepthFunction.Notequal,
            7 => DepthFunction.Gequal, _ => DepthFunction.Always });
        if (r.CullMode == 1) GL.Disable(EnableCap.CullFace);
        else { GL.Enable(EnableCap.CullFace); GL.FrontFace(FrontFaceDirection.Ccw); GL.CullFace(r.CullMode == 2 ? TriangleFace.Front : TriangleFace.Back); }
        if (r.BlendEnable != 0 || opacity < .999f) GL.Enable(EnableCap.Blend); else GL.Disable(EnableCap.Blend);
        GL.BlendFunc(opacity < .999f ? BlendingFactor.SrcAlpha : BlendFactor(r.SourceBlend),
            opacity < .999f ? BlendingFactor.OneMinusSrcAlpha : BlendFactor(r.DestinationBlend));
        GL.PolygonMode(TriangleFace.FrontAndBack, RenderMode == SmoSceneRenderMode.Wireframe || r.FillMode == 2 ? PolygonMode.Line : r.FillMode == 1 ? PolygonMode.Point : PolygonMode.Fill);
        GL.Uniform1(_materialAlphaTestLocation, r.AlphaTest != 0 ? 1 : 0);
        GL.Uniform1(_materialAlphaReferenceLocation, r.AlphaReference / 255f);
        GL.Uniform1(_materialAlphaFunctionLocation, (int)r.AlphaFunction);
    }

    private static BlendingFactor BlendFactor(uint value) => value switch {
        1 => BlendingFactor.Zero, 2 => BlendingFactor.One, 3 => BlendingFactor.SrcColor,
        4 => BlendingFactor.OneMinusSrcColor, 5 => BlendingFactor.SrcAlpha, 6 => BlendingFactor.OneMinusSrcAlpha,
        7 => BlendingFactor.DstAlpha, 8 => BlendingFactor.OneMinusDstAlpha, 9 => BlendingFactor.DstColor,
        10 => BlendingFactor.OneMinusDstColor, 11 => BlendingFactor.SrcAlphaSaturate,
        _ => throw new InvalidDataException("Unsupported mapped blend factor.") };

    private readonly record struct MaterialSamplerKey(uint U, uint V, uint Border, uint Mag, uint Min, uint Mip);
    private int MaterialSampler(SmoMaterialTextureStage stage)
    {
        var key = new MaterialSamplerKey(stage.AddressU, stage.AddressV, stage.BorderColor,
            stage.Magnification, stage.Minification, stage.MipFilter);
        if (_materialSamplers.TryGetValue(key, out int handle)) return handle;
        static TextureWrapMode Wrap(uint value) => value switch { 1 => TextureWrapMode.Repeat,
            2 => TextureWrapMode.MirroredRepeat, 3 => TextureWrapMode.ClampToEdge,
            _ => throw new InvalidDataException("Unsupported mapped texture address mode.") };
        var u = Wrap(key.U); var v = Wrap(key.V);
        if (key.Mag is < 1 or > 3 || key.Min is < 1 or > 3 || key.Mip > 2)
            throw new InvalidDataException("Unsupported texture filter.");
        // Native filter3 requests anisotropy. With the explicitly unchanged
        // default maximum anisotropy1, linear is its modern backend equivalent.
        var min = key.Mip switch {
            0 => key.Min == 1 ? TextureMinFilter.Nearest : TextureMinFilter.Linear,
            1 => key.Min == 1 ? TextureMinFilter.NearestMipmapNearest : TextureMinFilter.LinearMipmapNearest,
            _ => key.Min == 1 ? TextureMinFilter.NearestMipmapLinear : TextureMinFilter.LinearMipmapLinear };
        handle = GL.GenSampler();
        GL.SamplerParameter(handle, SamplerParameterName.TextureWrapS, (int)u);
        GL.SamplerParameter(handle, SamplerParameterName.TextureWrapT, (int)v);
        GL.SamplerParameter(handle, SamplerParameterName.TextureMinFilter, (int)min);
        GL.SamplerParameter(handle, SamplerParameterName.TextureMagFilter, (int)(key.Mag == 1 ? TextureMagFilter.Nearest : TextureMagFilter.Linear));
        _materialSamplers.Add(key, handle); return handle;
    }
}
