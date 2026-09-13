using OpenTK.Graphics.OpenGL4;
using SmoViewer.Core;
using SmoViewer.Scene;
using System.Diagnostics;
using System.Numerics;
using System.Runtime.InteropServices;
using System.Windows.Media;
using System.Windows.Media.Media3D;
using Color = System.Windows.Media.Color;

namespace SmoViewer.Rendering.Wpf;

/// <summary>A stable renderer identity independent from any viewer window.</summary>
public readonly record struct SmoRenderObjectKey(int FileIndex, int ObjectIndex,
    SmoRenderOccurrenceKey? OccurrenceKey = null);

public sealed record GpuUploadReport(
    int GeometryCount,
    int TextureCount,
    int PlacementCount,
    double ElapsedMilliseconds)
{
    public int RuntimeMipTextureCount {get;init;}
    public int GeneratedMipTextureCount {get;init;}
    public int UploadedMipCount {get;init;}
    public long CopiedTextureBytes {get;init;}
}

public sealed record GpuAntialiasingReport(
    int RequestedSamples,
    int ActualSamples);

public enum SmoSceneRenderMode
{
    Lit,
    Unlit,
    Wireframe
}

/// <summary>
/// Shared OpenGL 3.3 scene backend used by every WPF tool that previews SMO.
/// The caller owns the GL context, camera controls and editor interaction.
/// </summary>
public sealed partial class SmoGpuSceneRenderer
{
    // The confirmed PC corpus uses at most 16 palette slots per spSkin.
    // Keep headroom while staying below the OpenGL 3.3 minimum vertex
    // uniform budget together with model/view/projection matrices.
    private const int MaximumBoneCount = 32;
    private const int VertexFloatCount = 22;
    private readonly Dictionary<GpuGeometryKey, PendingGpuGeometry>
        _pendingGeometry = new();
    private readonly Dictionary<GpuTextureKey, SmoTexture> _pendingTextures = new();
    private readonly Dictionary<GpuGeometryKey, GpuGeometry> _geometry = new();
    private readonly Dictionary<GpuTextureKey, int> _textures = new();
    private readonly HashSet<GpuTextureKey> _runtimeMipTextures=[];
    private readonly Dictionary<SmoRenderObjectKey, GpuAppearance> _appearances = new();
    private readonly List<GpuRenderItem> _items = [];
    private bool _resetRequested;
    private bool _initialized;
    private int _program;
    private int _whiteTexture;
    private int _gridVertexArray;
    private int _gridVertexBuffer;
    private int _modelLocation;
    private int _viewLocation;
    private int _projectionLocation;
    private int _hasTextureLocation;
    private int _hasBaseTextureLocation;
    private int _skinnedLocation;
    private int _boneMatricesLocation;
    private int _highlightLocation;
    private int _opacityLocation;
    private int _luminanceCoverageLocation;
    private int _renderModeLocation;
    private GpuUploadReport? _uploadReport;
    private GpuAntialiasingReport? _antialiasingReport;
    private int _requestedAntialiasingSamples = 4;
    private int _activeAntialiasingSamples;
    private int _multisampleFramebuffer;
    private int _multisampleColorBuffer;
    private int _multisampleDepthBuffer;
    private int _multisampleWidth;
    private int _multisampleHeight;
    private int _lastReportedAntialiasingRequest = -1;
    private int _lastReportedAntialiasingActual = -1;

    public bool HasItems => _items.Count > 0;

    /// <summary>
    /// Controls only the shared model pass. Editor overlays and the floor grid
    /// remain solid, which keeps gizmos and diagnostics readable in wireframe.
    /// </summary>
    public SmoSceneRenderMode RenderMode { get; set; } =
        SmoSceneRenderMode.Unlit;

    public void SetAntialiasingSamples(int samples)
    {
        int normalized = samples is 2 or 4 or 8 ? samples : 0;
        if (_requestedAntialiasingSamples == normalized)
            return;
        _requestedAntialiasingSamples = normalized;
    }

    public GpuAntialiasingReport? ConsumeAntialiasingReport()
    {
        GpuAntialiasingReport? report = _antialiasingReport;
        _antialiasingReport = null;
        return report;
    }

    public void Add(
            SmoSceneMesh renderMesh,
        SmoRenderObjectKey key,
        Color fallbackColor)
    {
        SmoMesh mesh = renderMesh.Mesh;
        var geometryKey = new GpuGeometryKey(key.FileIndex, mesh.ObjectIndex);
        // Clear() schedules GL deletion for the next render because it may be
        // called outside the context-owning callback. Resources still present
        // in _geometry/_textures are therefore stale and must be queued again
        // when the rebuilt scene is added before that callback runs.
        if (!_pendingGeometry.ContainsKey(geometryKey) &&
            (_resetRequested || !_geometry.ContainsKey(geometryKey)))
        {
            _pendingGeometry.Add(
                geometryKey,
                BuildGeometry(renderMesh, fallbackColor));
        }

        GpuTextureKey? textureKey = renderMesh.Texture is SmoTexture texture && mesh.HasTextureCoordinates
            ? RegisterTexture(key.FileIndex, texture) : null;
        GpuTextureKey? baseTextureKey =
            renderMesh.BaseTexture is SmoTexture baseTexture &&
            mesh.HasTextureCoordinates && mesh.HasTextureCoordinates1
                ? RegisterTexture(key.FileIndex, baseTexture)
                : null;
        RegisterMaterialTextures(key.FileIndex, renderMesh);

        Matrix4x4 reflection = Matrix4x4.CreateScale(1, 1, -1);
        Matrix4x4 model = renderMesh.WorldTransform * reflection;
        bool transparent = renderMesh.RequiresTransparentOrdering;
        _items.Add(new GpuRenderItem(
            key,
            geometryKey,
            textureKey,
            baseTextureKey,
            model,
            transparent,
            renderMesh.MaterialRenderState?.UsesEmissiveApproximation == true,
            renderMesh.MaterialRenderState?
                .UsesLuminanceCoverageApproximation == true,
            renderMesh.InitialSkinMatrices?.ToArray() ?? [])
        {
            MaterialDraw = renderMesh.MaterialDraw,
            Material = renderMesh.LoadedMaterial,
            FogDraw = renderMesh.FogDraw,
            HasUv0 = mesh.HasTextureCoordinates,
            HasUv1 = mesh.HasTextureCoordinates1,
            AlphaSortData = renderMesh.AlphaSortData,
            AlphaSupportWorld = renderMesh.AlphaSupportWorld,
            SourcePriority = renderMesh.SourcePriority,
            OriginalAlphaSphere = mesh.PhysicalOffset >= 0 || renderMesh.HasOriginalAlphaSphere,
            IsSky=renderMesh.ContainerKind==SmoRenderContainerKind.SkyBox,SkyPose=renderMesh.SkyPose,
            AuthoredWorld=renderMesh.WorldTransform
        });
        _appearances[key] = new GpuAppearance(true, false, 1);
    }

    private GpuTextureKey RegisterTexture(int fileIndex, SmoTexture texture)
    {
        var key = new GpuTextureKey(fileIndex, texture.ObjectIndex);
        if(_pendingTextures.TryGetValue(key,out var pending))
        {
            // A legacy binding and a common material stage may refer to the
            // same FAT texture. Preserve the richer canonical runtime chain.
            if(texture.HasRuntimeMipChain&&!pending.HasRuntimeMipChain)_pendingTextures[key]=texture;
        }
        else if(_resetRequested || !_textures.ContainsKey(key) ||
            (texture.HasRuntimeMipChain&&!_runtimeMipTextures.Contains(key)))
            _pendingTextures.Add(key, texture);
        return key;
    }

    public void SetAppearance(
        SmoRenderObjectKey key,
        bool visible,
        bool highlighted,
        float opacity)
    {
        if (_appearances.ContainsKey(key))
            _appearances[key] = new GpuAppearance(
                visible,
                highlighted,
                opacity);
    }

    public void SetBoneMatrices(
        SmoRenderObjectKey key,
        IReadOnlyList<Matrix4x4> boneMatrices)
    {
        foreach (GpuRenderItem item in _items.Where(candidate =>
                     candidate.Key.Equals(key)))
        {
            item.BoneMatrices = boneMatrices.ToArray();
        }
    }

    public void SetModelTransform(SmoRenderObjectKey key, Matrix4x4 modelTransform)
    {
        foreach (GpuRenderItem item in _items.Where(candidate =>
                     candidate.Key.Equals(key)))
        {
            item.Model = modelTransform * Matrix4x4.CreateScale(1, 1, -1);
            item.AlphaSupportWorld = modelTransform;
            if(item.IsSky)item.SkyWorldEdited=modelTransform!=item.AuthoredWorld;
        }
    }

    public void Clear()
    {
        _pendingGeometry.Clear();
        _pendingTextures.Clear();
        _items.Clear();
        _appearances.Clear();
        ClearMaterialBindings();
        _uploadReport = null;
        _resetRequested = true;
    }

    public GpuUploadReport? ConsumeUploadReport()
    {
        GpuUploadReport? report = _uploadReport;
        _uploadReport = null;
        return report;
    }

    public void Render(
        int width,
        int height,
        ProjectionCamera camera,
        Color background,
        double turntableAngle,
        Point3D turntableCenter,
        bool showFloorGrid,
        Color floorGridColor,
        double floorY,
        double gridSpan,
        double sceneDiagonal,
        Action? renderSceneOverlay = null)
    {
        EnsureInitialized();
        if (_resetRequested)
        {
            DeleteSceneResources();
            _resetRequested = false;
        }
        UploadPendingResources();
        BeginMaterialFrame();

        GL.GetInteger(
            GetPName.DrawFramebufferBinding,
            out int outputFramebuffer);
        bool resolveMultisample = BindRenderTarget(
            width,
            height,
            outputFramebuffer);
        try
        {
            GL.Viewport(0, 0, width, height);
            GL.ClearColor(
                background.R / 255f,
                background.G / 255f,
                background.B / 255f,
                1);
            GL.ClearDepth(1);
            GL.Clear(ClearBufferMask.ColorBufferBit |
                     ClearBufferMask.DepthBufferBit);
            if (_items.Count == 0)
                return;

            GL.Enable(EnableCap.DepthTest);
            GL.DepthFunc(DepthFunction.Lequal);
            GL.Disable(EnableCap.CullFace);
            GL.UseProgram(_program);
            GL.ActiveTexture(TextureUnit.Texture0);

            Matrix4x4 view = SmoViewportMath.CreateViewMatrix(camera);
            float aspect = width / (float)Math.Max(height, 1);
            Matrix4x4 projection = SmoViewportMath.CreateProjectionMatrix(
                camera,
                aspect);
            SetMatrix(_viewLocation, view);
            SetMatrix(_projectionLocation, projection);
            GL.Uniform1(_fogEyeDepthLocation, camera is PerspectiveCamera ? 1 : 0);

            Matrix4x4 turntable = CreateTurntable(
                turntableAngle,
                turntableCenter);
            // Preserve the original left-handed shader view (+Z forward),
            // while projection/rasterization continue to use the GL camera.
            Matrix4x4 shaderView = view * Matrix4x4.CreateScale(1, 1, -1);
            SetMatrix(_fixedLightingView, shaderView);
            _gameWorldToShaderView = Matrix4x4.CreateScale(1, 1, -1) * turntable * shaderView;

            DrawSky(turntable,view);

            if (showFloorGrid)
                DrawFloorGrid(
                    turntableCenter,
                    floorY,
                    gridSpan,
                    sceneDiagonal,
                    floorGridColor);

            bool wireframe = RenderMode == SmoSceneRenderMode.Wireframe;
            if (wireframe)
                GL.PolygonMode(TriangleFace.FrontAndBack, PolygonMode.Line);
            GL.Disable(EnableCap.Blend);
            GL.DepthMask(true);
            foreach (GpuRenderItem item in _items.Where(item =>
                         !item.IsSky&&!UsesTransparentPass(item)))
                Draw(item, turntable);

            GL.Enable(EnableCap.Blend);
            GL.DepthMask(false);
            DrawTransparent(turntable,view);
            GL.DepthMask(true);
            GL.Disable(EnableCap.Blend);
            GL.Disable(EnableCap.CullFace);
            GL.Enable(EnableCap.DepthTest);
            GL.DepthFunc(DepthFunction.Lequal);
            for (int stage = 0; stage < 8; ++stage) GL.BindSampler(stage, 0);
            GL.PolygonMode(TriangleFace.FrontAndBack, PolygonMode.Fill);
            GL.BindVertexArray(0);
            GL.UseProgram(0);
            renderSceneOverlay?.Invoke();
        }
        finally
        {
            GL.PolygonMode(TriangleFace.FrontAndBack, PolygonMode.Fill);
            if (resolveMultisample)
            {
                GL.BindFramebuffer(
                    FramebufferTarget.ReadFramebuffer,
                    _multisampleFramebuffer);
                GL.BindFramebuffer(
                    FramebufferTarget.DrawFramebuffer,
                    outputFramebuffer);
                GL.BlitFramebuffer(
                    0, 0, width, height,
                    0, 0, width, height,
                    ClearBufferMask.ColorBufferBit,
                    BlitFramebufferFilter.Nearest);
            }
            GL.BindFramebuffer(
                FramebufferTarget.Framebuffer,
                outputFramebuffer);
        }
    }

    private bool BindRenderTarget(
        int width,
        int height,
        int outputFramebuffer)
    {
        GL.GetInteger(GetPName.MaxSamples, out int maximumSamples);
        int actualSamples = _requestedAntialiasingSamples > 1 &&
                            maximumSamples >= 2
            ? Math.Min(_requestedAntialiasingSamples, maximumSamples)
            : 0;
        if (actualSamples <= 1)
        {
            DeleteMultisampleTarget();
            ReportAntialiasing(actualSamples);
            GL.BindFramebuffer(
                FramebufferTarget.Framebuffer,
                outputFramebuffer);
            return false;
        }

        if (_multisampleFramebuffer == 0 ||
            _activeAntialiasingSamples != actualSamples ||
            _multisampleWidth != width ||
            _multisampleHeight != height)
        {
            DeleteMultisampleTarget();
            _multisampleFramebuffer = GL.GenFramebuffer();
            GL.BindFramebuffer(
                FramebufferTarget.Framebuffer,
                _multisampleFramebuffer);

            _multisampleColorBuffer = GL.GenRenderbuffer();
            GL.BindRenderbuffer(
                RenderbufferTarget.Renderbuffer,
                _multisampleColorBuffer);
            GL.RenderbufferStorageMultisample(
                RenderbufferTarget.Renderbuffer,
                actualSamples,
                RenderbufferStorage.Rgba8,
                width,
                height);
            GL.FramebufferRenderbuffer(
                FramebufferTarget.Framebuffer,
                FramebufferAttachment.ColorAttachment0,
                RenderbufferTarget.Renderbuffer,
                _multisampleColorBuffer);

            _multisampleDepthBuffer = GL.GenRenderbuffer();
            GL.BindRenderbuffer(
                RenderbufferTarget.Renderbuffer,
                _multisampleDepthBuffer);
            GL.RenderbufferStorageMultisample(
                RenderbufferTarget.Renderbuffer,
                actualSamples,
                RenderbufferStorage.DepthComponent24,
                width,
                height);
            GL.FramebufferRenderbuffer(
                FramebufferTarget.Framebuffer,
                FramebufferAttachment.DepthAttachment,
                RenderbufferTarget.Renderbuffer,
                _multisampleDepthBuffer);
            GL.BindRenderbuffer(RenderbufferTarget.Renderbuffer, 0);

            FramebufferErrorCode status = GL.CheckFramebufferStatus(
                FramebufferTarget.Framebuffer);
            if (status != FramebufferErrorCode.FramebufferComplete)
            {
                DeleteMultisampleTarget();
                GL.BindFramebuffer(
                    FramebufferTarget.Framebuffer,
                    outputFramebuffer);
                throw new InvalidOperationException(
                    $"OpenGL MSAA framebuffer incomplete: {status}.");
            }

            _activeAntialiasingSamples = actualSamples;
            _multisampleWidth = width;
            _multisampleHeight = height;
        }

        ReportAntialiasing(actualSamples);
        GL.BindFramebuffer(
            FramebufferTarget.Framebuffer,
            _multisampleFramebuffer);
        return true;
    }

    private void ReportAntialiasing(int actualSamples)
    {
        if (_lastReportedAntialiasingRequest ==
                _requestedAntialiasingSamples &&
            _lastReportedAntialiasingActual == actualSamples)
            return;
        _lastReportedAntialiasingRequest = _requestedAntialiasingSamples;
        _lastReportedAntialiasingActual = actualSamples;
        _antialiasingReport = new GpuAntialiasingReport(
            _requestedAntialiasingSamples,
            actualSamples);
    }

    private void DeleteMultisampleTarget()
    {
        if (_multisampleColorBuffer != 0)
            GL.DeleteRenderbuffer(_multisampleColorBuffer);
        if (_multisampleDepthBuffer != 0)
            GL.DeleteRenderbuffer(_multisampleDepthBuffer);
        if (_multisampleFramebuffer != 0)
            GL.DeleteFramebuffer(_multisampleFramebuffer);
        _multisampleFramebuffer = 0;
        _multisampleColorBuffer = 0;
        _multisampleDepthBuffer = 0;
        _multisampleWidth = 0;
        _multisampleHeight = 0;
        _activeAntialiasingSamples = 0;
    }

    private void DrawFloorGrid(
        Point3D center,
        double floorY,
        double span,
        double sceneDiagonal,
        Color color)
    {
        double step = SmoViewportMath.NiceGridStep(
            Math.Max(span / 20.0, 0.0001));
        const int halfLineCount = 12;
        double extent = step * halfLineCount;
        double centerX = Math.Round(center.X / step) * step;
        double centerZ = Math.Round(center.Z / step) * step;
        floorY -= Math.Max(step * 0.002, sceneDiagonal * 0.0005);

        const int verticesPerLine = 2;
        const int lineCount = (halfLineCount * 2 + 1) * 2;
        float[] vertices = new float[
            lineCount * verticesPerLine * VertexFloatCount];
        int vertex = 0;
        for (int offset = -halfLineCount; offset <= halfLineCount; offset++)
        {
            double coordinate = offset * step;
            float alpha = offset % 5 == 0 ? 235 / 255f : 150 / 255f;
            AddGridVertex(
                vertices, ref vertex,
                centerX + coordinate, floorY, centerZ - extent,
                color, alpha);
            AddGridVertex(
                vertices, ref vertex,
                centerX + coordinate, floorY, centerZ + extent,
                color, alpha);
            AddGridVertex(
                vertices, ref vertex,
                centerX - extent, floorY, centerZ + coordinate,
                color, alpha);
            AddGridVertex(
                vertices, ref vertex,
                centerX + extent, floorY, centerZ + coordinate,
                color, alpha);
        }

        GL.BindBuffer(BufferTarget.ArrayBuffer, _gridVertexBuffer);
        GL.BufferData(
            BufferTarget.ArrayBuffer,
            vertices.Length * sizeof(float),
            vertices,
            BufferUsageHint.StreamDraw);
        SetMatrix(_modelLocation, Matrix4x4.Identity);
        GL.Uniform1(_skinnedLocation, 0);
        GL.Uniform1(_hasTextureLocation, 0);
        GL.Uniform1(_hasBaseTextureLocation, 0);
        GL.Uniform1(_highlightLocation, 0);
        GL.Uniform1(_opacityLocation, 1f);
        GL.Uniform1(_luminanceCoverageLocation, 0);
        GL.Uniform1(_renderModeLocation, (int)SmoSceneRenderMode.Unlit);
        GL.Uniform1(_materialEnabledLocation, 0);
        GL.Uniform1(_fogModeLocation, 0);

        GL.ActiveTexture(TextureUnit.Texture0);
        GL.BindTexture(TextureTarget.Texture2D, _whiteTexture);
        GL.Enable(EnableCap.Blend);
        GL.BlendFunc(
            BlendingFactor.SrcAlpha,
            BlendingFactor.OneMinusSrcAlpha);
        GL.DepthMask(true);
        GL.BindVertexArray(_gridVertexArray);
        GL.DrawArrays(PrimitiveType.Lines, 0, vertex);
        GL.Disable(EnableCap.Blend);
    }

    private static void AddGridVertex(
        float[] vertices,
        ref int vertex,
        double x,
        double y,
        double z,
        Color color,
        float alpha)
    {
        int offset = vertex++ * VertexFloatCount;
        vertices[offset] = (float)x;
        vertices[offset + 1] = (float)y;
        vertices[offset + 2] = (float)z;
        vertices[offset + 5] = color.R / 255f;
        vertices[offset + 6] = color.G / 255f;
        vertices[offset + 7] = color.B / 255f;
        vertices[offset + 8] = alpha;
    }

    private bool UsesTransparentPass(GpuRenderItem item)
    {
        GpuAppearance appearance = _appearances.GetValueOrDefault(
            item.Key,
            new GpuAppearance(true, false, 1));
        return item.Transparent || appearance.Opacity < 0.999f;
    }

    private void Draw(GpuRenderItem item, Matrix4x4 turntable)
    {
        GpuAppearance appearance = _appearances.GetValueOrDefault(
            item.Key,
            new GpuAppearance(true, false, 1));
        if (!appearance.Visible || appearance.Opacity <= 0 ||
            (item.IsSky&&!item.SkyDrawEnabled) ||
            !_geometry.TryGetValue(item.GeometryKey, out GpuGeometry? geometry))
        {
            return;
        }

        Matrix4x4 model = item.Model * turntable;
        BindFog(item);
        SetMatrix(_modelLocation, model);
        GL.Uniform1(_highlightLocation, appearance.Highlighted ? 1 : 0);
        GL.Uniform1(_opacityLocation, appearance.Opacity);
        GL.Uniform1(
            _luminanceCoverageLocation,
            item.LuminanceCoverage ? 1 : 0);
        GL.Uniform1(_renderModeLocation, (int)RenderMode);

        bool skinned = item.BoneMatrices.Length > 0;
        GL.Uniform1(_skinnedLocation, skinned ? 1 : 0);
        if (skinned)
            SetBoneMatrices(_boneMatricesLocation, item.BoneMatrices);

        if (item.Material is not null || item.MaterialDraw is not null)
        {
            DrawMaterial(item, geometry, appearance);
            return;
        }
        GL.Uniform1(_materialEnabledLocation, 0);
        GL.Disable(EnableCap.CullFace);
        GL.Enable(EnableCap.DepthTest);
        GL.DepthFunc(DepthFunction.Lequal);
        bool transparent = UsesTransparentPass(item);
        GL.DepthMask(!transparent);
        if (transparent) GL.Enable(EnableCap.Blend); else GL.Disable(EnableCap.Blend);
        GL.BlendFunc(BlendingFactor.SrcAlpha, item.Additive ? BlendingFactor.One : BlendingFactor.OneMinusSrcAlpha);
        GL.PolygonMode(TriangleFace.FrontAndBack, RenderMode == SmoSceneRenderMode.Wireframe ? PolygonMode.Line : PolygonMode.Fill);
        GL.BindSampler(0, 0); GL.BindSampler(1, 0);

        int texture = _whiteTexture;
        bool hasTexture = TryResolveTexture(item, out texture);
        GL.ActiveTexture(TextureUnit.Texture0);
        GL.Uniform1(_hasTextureLocation, hasTexture ? 1 : 0);
        GL.BindTexture(TextureTarget.Texture2D, texture);

        int baseTexture = _whiteTexture;
        bool hasBaseTexture = item.BaseTextureKey is GpuTextureKey baseKey &&
                              _textures.TryGetValue(baseKey, out baseTexture);
        GL.ActiveTexture(TextureUnit.Texture1);
        GL.Uniform1(_hasBaseTextureLocation, hasBaseTexture ? 1 : 0);
        GL.BindTexture(TextureTarget.Texture2D, baseTexture);
        GL.ActiveTexture(TextureUnit.Texture0);
        GL.BindVertexArray(geometry.VertexArray);
        GL.DrawElements(
            PrimitiveType.Triangles,
            geometry.IndexCount,
            DrawElementsType.UnsignedInt,
            0);
    }

    private bool TryResolveTexture(GpuRenderItem item, out int texture)
    {
        texture = _whiteTexture;
        return item.TextureKey is GpuTextureKey key && _textures.TryGetValue(key, out texture);
    }

    private static void SetBoneMatrices(int location, IReadOnlyList<Matrix4x4> matrices)
    {
        int count = Math.Min(matrices.Count, MaximumBoneCount);
        float[] values = new float[count * 16];
        for (int index = 0; index < count; index++)
        {
            Matrix4x4 matrix = matrices[index];
            int offset = index * 16;
            values[offset] = matrix.M11;
            values[offset + 1] = matrix.M12;
            values[offset + 2] = matrix.M13;
            values[offset + 3] = matrix.M14;
            values[offset + 4] = matrix.M21;
            values[offset + 5] = matrix.M22;
            values[offset + 6] = matrix.M23;
            values[offset + 7] = matrix.M24;
            values[offset + 8] = matrix.M31;
            values[offset + 9] = matrix.M32;
            values[offset + 10] = matrix.M33;
            values[offset + 11] = matrix.M34;
            values[offset + 12] = matrix.M41;
            values[offset + 13] = matrix.M42;
            values[offset + 14] = matrix.M43;
            values[offset + 15] = matrix.M44;
        }
        GL.UniformMatrix4(
            location,
            count,
            true,
            values);
    }

    private const string VertexShaderSource = """
                #version 330 core
                layout(location = 0) in vec3 aPosition;
                layout(location = 1) in vec2 aUv;
                layout(location = 2) in vec4 aColor;
                layout(location = 3) in vec2 aUv1;
                layout(location = 4) in vec4 aBoneWeights;
                layout(location = 5) in vec4 aBoneIndices;
                layout(location = 6) in vec3 aNormal;
                uniform mat4 uModel;
                uniform mat4 uView;
                uniform mat4 uProjection;
                uniform int uSkinned;
                uniform mat4 uBones[32];
                uniform int uMaterialEnabled;
                uniform int uMaterialStageCount;
                uniform int uStageCoordinates[8];
                uniform int uStageTransformFlags[8];
                uniform mat4 uStageUV[8];
                out vec2 vUv;
                out vec2 vUv1;
                out vec4 vColor;
                out vec3 vNormal;
                out vec4 vStageUV[8];
                """ + "\n" + FixedLightingVertexSource + "\n" + """
                void AddBone(
                    inout vec4 skinned,
                    vec4 localPosition,
                    float weight,
                    float rawIndex)
                {
                    int index = int(rawIndex + 0.5);
                    if (index >= 0 && index < 32)
                    {
                        skinned += (localPosition * uBones[index]) * weight;
                    }
                }

                void main()
                {
                    vec4 localPosition = vec4(aPosition, 1.0);
                    vec4 localNormal = vec4(aNormal, 0.0);
                    if (uSkinned != 0)
                    {
                        vec4 skinned = vec4(0.0);
                        vec4 skinnedNormal = vec4(0.0);
                        AddBone(skinned, localPosition,
                            aBoneWeights.x, aBoneIndices.x);
                        AddBone(skinnedNormal, localNormal,
                            aBoneWeights.x, aBoneIndices.x);
                        AddBone(skinned, localPosition,
                            aBoneWeights.y, aBoneIndices.y);
                        AddBone(skinnedNormal, localNormal,
                            aBoneWeights.y, aBoneIndices.y);
                        AddBone(skinned, localPosition,
                            aBoneWeights.z, aBoneIndices.z);
                        AddBone(skinnedNormal, localNormal,
                            aBoneWeights.z, aBoneIndices.z);
                        AddBone(skinned, localPosition,
                            aBoneWeights.w, aBoneIndices.w);
                        AddBone(skinnedNormal, localNormal,
                            aBoneWeights.w, aBoneIndices.w);
                        // Original Fixed.rfx writes weighted xyz only; it
                        // neither divides by the weight sum nor replaces
                        // zero/tiny/negative weights. Homogeneous w stays 1.
                        localPosition = vec4(skinned.xyz, 1.0);
                        localNormal = vec4(normalize(skinnedNormal.xyz), 0.0);
                    }
                    gl_Position = localPosition *
                        uModel * uView * uProjection;
                    vUv = aUv;
                    vUv1 = aUv1;
                    vColor = aColor;
                    vNormal = normalize((localNormal * uModel).xyz);
                    vFixedColor = aColor;
                    if (uFixedLightingEnabled != 0)
                        vFixedColor = FixedLighting(localPosition * uModel * uFixedLightingView,
                            normalize((localNormal * uModel * uFixedLightingView).xyz), aColor);
                    for (int stage = 0; stage < uMaterialStageCount; ++stage)
                    {
                        vec2 uv = uStageCoordinates[stage] == 1 ? aUv1 : aUv;
                        // Original Fixed.rfx uses UV0 for every generated
                        // weighted-shader output. Rigid fixed-function draws
                        // instead use the mapped coordinate selector.
                        if (uSkinned != 0) uv = aUv;
                        vStageUV[stage] = vec4(uv, 1.0, 0.0);
                        if (uMaterialEnabled != 0 && uStageTransformFlags[stage] != 0)
                            vStageUV[stage] = vStageUV[stage] * uStageUV[stage];
                    }
                }
                """;

    private void EnsureInitialized()
    {
        if (_initialized)
            return;


        _program = CreateProgram(VertexShaderSource, MaterialFragmentShaderSource);
        InitializeLightingUniforms();
        InitializeFogUniforms();
        _modelLocation = GL.GetUniformLocation(_program, "uModel");
        _viewLocation = GL.GetUniformLocation(_program, "uView");
        _projectionLocation = GL.GetUniformLocation(_program, "uProjection");
        _hasTextureLocation = GL.GetUniformLocation(_program, "uHasTexture");
        _hasBaseTextureLocation = GL.GetUniformLocation(
            _program, "uHasBaseTexture");
        _skinnedLocation = GL.GetUniformLocation(_program, "uSkinned");
        _boneMatricesLocation = GL.GetUniformLocation(_program, "uBones[0]");
        _highlightLocation = GL.GetUniformLocation(_program, "uHighlight");
        _opacityLocation = GL.GetUniformLocation(_program, "uOpacity");
        _luminanceCoverageLocation = GL.GetUniformLocation(
            _program,
            "uLuminanceCoverage");
        _renderModeLocation = GL.GetUniformLocation(_program, "uRenderMode");
        GL.UseProgram(_program);
        GL.Uniform1(GL.GetUniformLocation(_program, "uTexture"), 0);
        GL.Uniform1(GL.GetUniformLocation(_program, "uBaseTexture"), 1);
        InitializeMaterialUniforms();
        GL.UseProgram(0);

        _whiteTexture = GL.GenTexture();
        GL.BindTexture(TextureTarget.Texture2D, _whiteTexture);
        byte[] white = [255, 255, 255, 255];
        GL.TexImage2D(
            TextureTarget.Texture2D,
            0,
            PixelInternalFormat.Rgba8,
            1,
            1,
            0,
            OpenTK.Graphics.OpenGL4.PixelFormat.Bgra,
            PixelType.UnsignedByte,
            white);
        ConfigureTextureSampling(generateMipmaps: false);

        _gridVertexArray = GL.GenVertexArray();
        _gridVertexBuffer = GL.GenBuffer();
        GL.BindVertexArray(_gridVertexArray);
        GL.BindBuffer(BufferTarget.ArrayBuffer, _gridVertexBuffer);
        int gridStride = VertexFloatCount * sizeof(float);
        GL.VertexAttribPointer(
            0, 3, VertexAttribPointerType.Float, false, gridStride, 0);
        GL.EnableVertexAttribArray(0);
        GL.VertexAttribPointer(
            1, 2, VertexAttribPointerType.Float, false, gridStride,
            3 * sizeof(float));
        GL.EnableVertexAttribArray(1);
        GL.VertexAttribPointer(
            2, 4, VertexAttribPointerType.Float, false, gridStride,
            5 * sizeof(float));
        GL.EnableVertexAttribArray(2);
        GL.BindVertexArray(0);
        _initialized = true;
    }

    private void UploadPendingResources()
    {
        if (_pendingGeometry.Count == 0 && _pendingTextures.Count == 0)
            return;

        Stopwatch timer = Stopwatch.StartNew();
        int geometryCount = _pendingGeometry.Count;
        int textureCount = _pendingTextures.Count;
        foreach ((GpuGeometryKey key, PendingGpuGeometry pending) in
                 _pendingGeometry)
        {
            int vertexArray = GL.GenVertexArray();
            int vertexBuffer = GL.GenBuffer();
            int indexBuffer = GL.GenBuffer();
            GL.BindVertexArray(vertexArray);
            GL.BindBuffer(BufferTarget.ArrayBuffer, vertexBuffer);
            GL.BufferData(
                BufferTarget.ArrayBuffer,
                pending.Vertices.Length * sizeof(float),
                pending.Vertices,
                BufferUsageHint.StaticDraw);
            GL.BindBuffer(BufferTarget.ElementArrayBuffer, indexBuffer);
            GL.BufferData(
                BufferTarget.ElementArrayBuffer,
                pending.Indices.Length * sizeof(uint),
                pending.Indices,
                BufferUsageHint.StaticDraw);

            int stride = VertexFloatCount * sizeof(float);
            GL.VertexAttribPointer(
                0, 3, VertexAttribPointerType.Float, false, stride, 0);
            GL.EnableVertexAttribArray(0);
            GL.VertexAttribPointer(
                1, 2, VertexAttribPointerType.Float, false, stride,
                3 * sizeof(float));
            GL.EnableVertexAttribArray(1);
            GL.VertexAttribPointer(
                2, 4, VertexAttribPointerType.Float, false, stride,
                5 * sizeof(float));
            GL.EnableVertexAttribArray(2);
            GL.VertexAttribPointer(
                3, 2, VertexAttribPointerType.Float, false, stride,
                9 * sizeof(float));
            GL.EnableVertexAttribArray(3);
            GL.VertexAttribPointer(
                4, 4, VertexAttribPointerType.Float, false, stride,
                11 * sizeof(float));
            GL.EnableVertexAttribArray(4);
            GL.VertexAttribPointer(
                5, 4, VertexAttribPointerType.Float, false, stride,
                15 * sizeof(float));
            GL.EnableVertexAttribArray(5);
            GL.VertexAttribPointer(
                6, 3, VertexAttribPointerType.Float, false, stride,
                19 * sizeof(float));
            GL.EnableVertexAttribArray(6);
            _geometry.Add(key, new GpuGeometry(
                vertexArray,
                vertexBuffer,
                indexBuffer,
                pending.Indices.Length,
                pending.Vertices.Length / VertexFloatCount,
                pending.Center));
        }
        _pendingGeometry.Clear();

        int runtimeMipTextures=0,generatedMipTextures=0,uploadedMips=0;long copiedTextureBytes=0;
        foreach ((GpuTextureKey key, SmoTexture texture) in _pendingTextures)
        {
            int handle = GL.GenTexture();
            GL.BindTexture(TextureTarget.Texture2D, handle);
            GL.PixelStore(PixelStoreParameter.UnpackAlignment, 4);
            for(int level=0;level<texture.MipLevels.Count;++level)
            {
                var mip=texture.MipLevels[level];
                GL.TexImage2D(TextureTarget.Texture2D,level,PixelInternalFormat.Rgba8,mip.Width,mip.Height,0,
                    OpenTK.Graphics.OpenGL4.PixelFormat.Bgra,PixelType.UnsignedByte,PixelsForUpload(mip.Bgra32Pixels,ref copiedTextureBytes));
                ++uploadedMips;
            }
            ConfigureTextureSampling(generateMipmaps: !texture.HasRuntimeMipChain,
                suppliedMipLevels:texture.HasRuntimeMipChain?texture.MipLevels.Count:null);
            if(_textures.Remove(key,out int previous))GL.DeleteTexture(previous);
            _textures.Add(key, handle);
            if(texture.HasRuntimeMipChain){_runtimeMipTextures.Add(key);++runtimeMipTextures;}
            else{_runtimeMipTextures.Remove(key);++generatedMipTextures;}
        }
        _pendingTextures.Clear();
        timer.Stop();
        _uploadReport = new GpuUploadReport(
            geometryCount,
            textureCount,
            _items.Count,
            timer.Elapsed.TotalMilliseconds)
        {RuntimeMipTextureCount=runtimeMipTextures,GeneratedMipTextureCount=generatedMipTextures,
            UploadedMipCount=uploadedMips,CopiedTextureBytes=copiedTextureBytes};
    }

    private static PendingGpuGeometry BuildGeometry(
            SmoSceneMesh renderMesh,
        Color fallbackColor)
    {
        SmoMesh mesh = renderMesh.Mesh;
        float[] vertices = new float[checked(mesh.VertexCount * VertexFloatCount)];
        bool useDiffuse = mesh.HasDiffuseColors;
        bool useAlpha = mesh.HasDiffuseColors;
        uint fallbackArgb = renderMesh.LoadedMaterial is not null ? 0xffffffff : renderMesh.MaterialColorArgb ??
            ((uint)fallbackColor.A << 24 |
             (uint)fallbackColor.R << 16 |
             (uint)fallbackColor.G << 8 |
             fallbackColor.B);

        Vector3 minimum = new(float.PositiveInfinity);
        Vector3 maximum = new(float.NegativeInfinity);
        for (int vertex = 0; vertex < mesh.VertexCount; vertex++)
        {
            int offset = vertex * VertexFloatCount;
            Vector3 position = mesh.Positions[vertex];
            minimum = Vector3.Min(minimum, position);
            maximum = Vector3.Max(maximum, position);
            vertices[offset] = position.X;
            vertices[offset + 1] = position.Y;
            vertices[offset + 2] = position.Z;
            if (mesh.HasTextureCoordinates)
            {
                vertices[offset + 3] = mesh.TextureCoordinates[vertex].X;
                vertices[offset + 4] = mesh.TextureCoordinates[vertex].Y;
            }

            uint argb = useDiffuse
                ? mesh.DiffuseColorsArgb[vertex]
                : fallbackArgb;
            vertices[offset + 5] = ((argb >> 16) & 0xFF) / 255f;
            vertices[offset + 6] = ((argb >> 8) & 0xFF) / 255f;
            vertices[offset + 7] = (argb & 0xFF) / 255f;
            vertices[offset + 8] = useAlpha
                ? (argb >> 24) / 255f
                : 1;

            if (mesh.HasTextureCoordinates1)
            {
                vertices[offset + 9] = mesh.TextureCoordinates1[vertex].X;
                vertices[offset + 10] = mesh.TextureCoordinates1[vertex].Y;
            }
            if (mesh.HasSkinningData)
            {
                Vector4 weights = mesh.BlendWeights[vertex];
                SmoBlendIndices indicesValue = mesh.BlendIndices[vertex];
                vertices[offset + 11] = weights.X;
                vertices[offset + 12] = weights.Y;
                vertices[offset + 13] = weights.Z;
                vertices[offset + 14] = weights.W;
                vertices[offset + 15] = indicesValue.X;
                vertices[offset + 16] = indicesValue.Y;
                vertices[offset + 17] = indicesValue.Z;
                vertices[offset + 18] = indicesValue.W;
            }
            Vector3 normal = mesh.HasNormals
                ? mesh.Normals[vertex]
                : Vector3.UnitY;
            vertices[offset + 19] = normal.X;
            vertices[offset + 20] = normal.Y;
            vertices[offset + 21] = normal.Z;
        }

        uint[] indices = new uint[mesh.TriangleIndices.Length];
        for (int index = 0; index < mesh.TriangleIndices.Length; index += 3)
        {
            indices[index] = mesh.TriangleIndices[index];
            indices[index + 1] = mesh.TriangleIndices[index + 2];
            indices[index + 2] = mesh.TriangleIndices[index + 1];
        }
        return new PendingGpuGeometry(
            vertices,
            indices,
            (minimum + maximum) * 0.5f);
    }

    private static byte[] PixelsForUpload(ReadOnlyMemory<byte> memory,ref long copiedBytes)
    {
        if(MemoryMarshal.TryGetArray(memory,out ArraySegment<byte> segment)&&segment.Array is { } bytes&&
            segment.Offset==0&&segment.Count==bytes.Length)return bytes;
        copiedBytes+=memory.Length;return memory.ToArray();
    }

    private static void ConfigureTextureSampling(bool generateMipmaps,int? suppliedMipLevels=null)
    {
        if(suppliedMipLevels is int levels)
        {
            GL.TexParameter(TextureTarget.Texture2D,TextureParameterName.TextureBaseLevel,0);
            GL.TexParameter(TextureTarget.Texture2D,TextureParameterName.TextureMaxLevel,levels-1);
        }
        GL.TexParameter(
            TextureTarget.Texture2D,
            TextureParameterName.TextureWrapS,
            (int)TextureWrapMode.Repeat);
        GL.TexParameter(
            TextureTarget.Texture2D,
            TextureParameterName.TextureWrapT,
            (int)TextureWrapMode.Repeat);
        GL.TexParameter(
            TextureTarget.Texture2D,
            TextureParameterName.TextureMagFilter,
            (int)TextureMagFilter.Linear);
        GL.TexParameter(
            TextureTarget.Texture2D,
            TextureParameterName.TextureMinFilter,
            generateMipmaps || suppliedMipLevels.GetValueOrDefault()>1
                ? (int)TextureMinFilter.LinearMipmapLinear
                : (int)TextureMinFilter.Linear);
        if (generateMipmaps)
            GL.GenerateMipmap(GenerateMipmapTarget.Texture2D);
    }

    private static Matrix4x4 CreateTurntable(
        double angleDegrees,
        Point3D center)
    {
        if (Math.Abs(angleDegrees) < 0.000001)
            return Matrix4x4.Identity;
        Vector3 pivot = new(
            (float)center.X,
            (float)center.Y,
            (float)center.Z);
        return Matrix4x4.CreateTranslation(-pivot) *
               Matrix4x4.CreateRotationY(
                   (float)(angleDegrees * Math.PI / 180.0)) *
               Matrix4x4.CreateTranslation(pivot);
    }

    private static void SetMatrix(int location, Matrix4x4 matrix)
    {
        float[] values =
        [
            matrix.M11, matrix.M12, matrix.M13, matrix.M14,
                matrix.M21, matrix.M22, matrix.M23, matrix.M24,
                matrix.M31, matrix.M32, matrix.M33, matrix.M34,
                matrix.M41, matrix.M42, matrix.M43, matrix.M44
        ];
        GL.UniformMatrix4(location, 1, true, values);
    }

    private static int CreateProgram(string vertexSource, string fragmentSource)
    {
        int vertex = CompileShader(ShaderType.VertexShader, vertexSource);
        int fragment = CompileShader(ShaderType.FragmentShader, fragmentSource);
        int program = GL.CreateProgram();
        GL.AttachShader(program, vertex);
        GL.AttachShader(program, fragment);
        GL.LinkProgram(program);
        GL.GetProgram(program, GetProgramParameterName.LinkStatus, out int linked);
        string log = GL.GetProgramInfoLog(program);
        GL.DetachShader(program, vertex);
        GL.DetachShader(program, fragment);
        GL.DeleteShader(vertex);
        GL.DeleteShader(fragment);
        if (linked == 0)
        {
            GL.DeleteProgram(program);
            throw new InvalidOperationException(
                $"OpenGL program link failed: {log}");
        }
        return program;
    }

    private static int CompileShader(ShaderType type, string source)
    {
        int shader = GL.CreateShader(type);
        GL.ShaderSource(shader, source);
        GL.CompileShader(shader);
        GL.GetShader(shader, ShaderParameter.CompileStatus, out int compiled);
        if (compiled != 0)
            return shader;
        string log = GL.GetShaderInfoLog(shader);
        GL.DeleteShader(shader);
        throw new InvalidOperationException(
            $"OpenGL {type} compilation failed: {log}");
    }

    private void DeleteSceneResources()
    {
        DeletePickingBuffer();
        foreach (GpuGeometry geometry in _geometry.Values)
        {
            GL.DeleteVertexArray(geometry.VertexArray);
            GL.DeleteBuffer(geometry.VertexBuffer);
            GL.DeleteBuffer(geometry.IndexBuffer);
        }
        foreach (int texture in _textures.Values)
            GL.DeleteTexture(texture);
        _geometry.Clear();
        _textures.Clear();
        _runtimeMipTextures.Clear();
        foreach (int sampler in _materialSamplers.Values) GL.DeleteSampler(sampler);
        _materialSamplers.Clear();
    }

    private readonly record struct GpuGeometryKey(
        int FileIndex,
        int ObjectIndex);

    private readonly record struct GpuTextureKey(
        int FileIndex,
        int ObjectIndex);

    private sealed record PendingGpuGeometry(
        float[] Vertices,
        uint[] Indices,
        Vector3 Center);

    private sealed record GpuGeometry(
        int VertexArray,
        int VertexBuffer,
        int IndexBuffer,
        int IndexCount,
        int VertexCount,
        Vector3 Center);

    private sealed class GpuRenderItem(
        SmoRenderObjectKey key,
        GpuGeometryKey geometryKey,
        GpuTextureKey? textureKey,
        GpuTextureKey? baseTextureKey,
        Matrix4x4 model,
        bool transparent,
        bool additive,
        bool luminanceCoverage,
        Matrix4x4[] boneMatrices)
    {
        public SmoRenderObjectKey Key { get; } = key;
        public GpuGeometryKey GeometryKey { get; } = geometryKey;
        public GpuTextureKey? TextureKey { get; } = textureKey;
        public GpuTextureKey? BaseTextureKey { get; } = baseTextureKey;
        public Matrix4x4 Model { get; set; } = model;
        public bool Transparent { get; } = transparent;
        public bool Additive { get; } = additive;
        public bool LuminanceCoverage { get; } = luminanceCoverage;
        public Matrix4x4[] BoneMatrices { get; set; } = boneMatrices;
        public SmoMaterialDraw? MaterialDraw { get; set; }
        public SmoAlphaSortData? AlphaSortData { get; init; }
        public Matrix4x4? AlphaSupportWorld { get; set; }
        public uint SourcePriority { get; init; }
        public bool OriginalAlphaSphere { get; init; }
        public bool IsSky {get;init;}
        public bool SkyDrawEnabled {get;set;}
        public SmoSkyBoxPose? SkyPose {get;init;}
        public Matrix4x4 AuthoredWorld {get;init;}
        public bool SkyWorldEdited {get;set;}
        public int? TextRigidNode {get;set;}
        public int? TextObjectIndex {get;set;}
        public SmoLoadedMaterial? Material { get; init; }
        public SmoFogDraw? FogDraw { get; init; }
        public bool HasUv0 { get; init; }
        public bool HasUv1 { get; init; }
    }

    private readonly record struct GpuAppearance(
        bool Visible,
        bool Highlighted,
        float Opacity);
}
