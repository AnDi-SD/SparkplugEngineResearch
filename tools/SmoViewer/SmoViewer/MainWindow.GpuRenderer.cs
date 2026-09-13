using OpenTK.Graphics.OpenGL4;
using OpenTK.Wpf;
using SmoViewer.Core;
using System.Diagnostics;
using System.Numerics;
using System.Windows.Media;
using System.Windows.Media.Media3D;
using Color = System.Windows.Media.Color;

namespace SmoViewer;

public partial class MainWindow
{
    private readonly GpuSceneRenderer _gpuRenderer = new();
    private bool _gpuRendererAvailable;
    private int _gpuAntialiasingSamples = 4;

    private void InitializeGpuViewport()
    {
        try
        {
            GpuViewport.Start(new GLWpfControlSettings
            {
                MajorVersion = 3,
                MinorVersion = 3,
                Samples = 0
            });
            _gpuRenderer.SetAntialiasingSamples(_gpuAntialiasingSamples);
            _gpuRendererAvailable = true;
            AddLog(
                "GPU-preview включён: исходные texture, UV и vertex diffuse " +
                "передаются видеокарте без CPU triangle-atlas; MSAA 4× включён.");
        }
        catch (Exception exception)
        {
            _gpuRendererAvailable = false;
            GpuViewport.Visibility = System.Windows.Visibility.Collapsed;
            AddLog(
                $"ПРЕДУПРЕЖДЕНИЕ: GPU-preview недоступен ({exception.Message}); " +
                "используется WPF fallback.");
        }
    }

    private void GpuViewport_OnRender(TimeSpan delta)
    {
        if (!_gpuRendererAvailable)
            return;

        try
        {
            System.Windows.DpiScale dpi = VisualTreeHelper.GetDpi(GpuViewport);
            int width = Math.Max(
                1,
                (int)Math.Round(GpuViewport.ActualWidth * dpi.DpiScaleX));
            int height = Math.Max(
                1,
                (int)Math.Round(GpuViewport.ActualHeight * dpi.DpiScaleY));
            ProjectionCamera camera = _guiPreviewActive
                ? _guiCamera
                : SceneCamera;
            _gpuRenderer.Render(
                width,
                height,
                camera,
                _sceneBackgroundColor,
                _modelTurntableEnabled ? _modelTurntableAngle : 0,
                _sceneBounds.HasValue ? _sceneBounds.Center : new Point3D(),
                ShowFloorGridCheck?.IsChecked == true && !_guiPreviewActive,
                _floorGridColor,
                _sceneBounds.HasValue ? _sceneBounds.MinY : 0,
                _sceneBounds.HasValue
                    ? Math.Max(
                        Math.Max(_sceneBounds.Width, _sceneBounds.Depth),
                        _sceneBounds.DiagonalLength * 0.25)
                    : 20,
                _sceneBounds.HasValue ? _sceneBounds.DiagonalLength : 0);

            if (_gpuRenderer.ConsumeUploadReport() is GpuUploadReport report)
            {
                AddLog(
                    $"GPU: загружено {report.GeometryCount} unique mesh buffers, " +
                    $"{report.TextureCount} unique textures за " +
                    $"{report.ElapsedMilliseconds:F1} ms; placements " +
                    $"{report.PlacementCount}.");
            }
            if (_gpuRenderer.ConsumeAntialiasingReport() is
                GpuAntialiasingReport antialiasing)
            {
                AddLog(antialiasing.ActualSamples > 1
                    ? $"Сглаживание OpenGL: MSAA {antialiasing.ActualSamples}×" +
                      (antialiasing.ActualSamples == antialiasing.RequestedSamples
                          ? "."
                          : $" (запрошено {antialiasing.RequestedSamples}×, " +
                            "ограничено возможностями GPU).")
                    : "Сглаживание OpenGL отключено.");
            }
        }
        catch (Exception exception)
        {
            _gpuRendererAvailable = false;
            GpuViewport.Visibility = System.Windows.Visibility.Collapsed;
            AddLog(
                $"ОШИБКА GPU-preview: {exception.Message}. " +
                "Новые файлы будут открываться через WPF fallback.");
        }
    }

    private bool CanUseDirectGpuPath(DecodedRenderMesh renderMesh) =>
        _gpuRendererAvailable;

    private void AddDirectGpuMesh(
        DecodedRenderMesh renderMesh,
        SceneObjectKey key,
        Color fallbackColor)
    {
        _gpuRenderer.Add(renderMesh, key, fallbackColor);
    }

    private void ResetDirectGpuScene() => _gpuRenderer.Clear();

    private bool HasDirectGpuScene => _gpuRenderer.HasItems;

    private void UpdateDirectGpuAppearance(
        SceneObjectKey key,
        bool visible,
        bool highlighted,
        double opacity)
    {
        _gpuRenderer.SetAppearance(
            key,
            visible,
            highlighted,
            (float)Math.Clamp(opacity, 0, 1));
    }

    private void UpdateDirectGpuSkinning(
        SceneObjectKey key,
        IReadOnlyList<Matrix4x4> boneMatrices) =>
        _gpuRenderer.SetBoneMatrices(key, boneMatrices);

    private void UpdateDirectGpuModelTransform(
        SceneObjectKey key,
        Matrix4x4 modelTransform) =>
        _gpuRenderer.SetModelTransform(key, modelTransform);

    private void GpuAntialiasingCombo_SelectionChanged(
        object sender,
        System.Windows.Controls.SelectionChangedEventArgs e)
    {
        if (GpuAntialiasingCombo?.SelectedItem is not
            System.Windows.Controls.ComboBoxItem item ||
            !int.TryParse(item.Tag?.ToString(), out int samples))
        {
            return;
        }

        _gpuAntialiasingSamples = samples;
        _gpuRenderer.SetAntialiasingSamples(samples);
    }

    private sealed record GpuUploadReport(
        int GeometryCount,
        int TextureCount,
        int PlacementCount,
        double ElapsedMilliseconds);

    private sealed record GpuAntialiasingReport(
        int RequestedSamples,
        int ActualSamples);

    private sealed class GpuSceneRenderer
    {
        // The confirmed PC corpus uses at most 16 palette slots per spSkin.
        // Keep headroom while staying below the OpenGL 3.3 minimum vertex
        // uniform budget together with model/view/projection matrices.
        private const int MaximumBoneCount = 32;
        private const int VertexFloatCount = 19;
        private readonly Dictionary<GpuGeometryKey, PendingGpuGeometry>
            _pendingGeometry = new();
        private readonly Dictionary<GpuTextureKey, SmoTexture> _pendingTextures = new();
        private readonly Dictionary<GpuGeometryKey, GpuGeometry> _geometry = new();
        private readonly Dictionary<GpuTextureKey, int> _textures = new();
        private readonly Dictionary<SceneObjectKey, GpuAppearance> _appearances = new();
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
        private long _textureAnimationStart = Stopwatch.GetTimestamp();

        public bool HasItems => _items.Count > 0;

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
            DecodedRenderMesh renderMesh,
            SceneObjectKey key,
            Color fallbackColor)
        {
            SmoMesh mesh = renderMesh.Mesh;
            var geometryKey = new GpuGeometryKey(key.FileIndex, mesh.ObjectIndex);
            if (!_pendingGeometry.ContainsKey(geometryKey) &&
                !_geometry.ContainsKey(geometryKey))
            {
                _pendingGeometry.Add(
                    geometryKey,
                    BuildGeometry(renderMesh, fallbackColor));
            }

            GpuTextureKey[] textureKeys = renderMesh.AnimationFrames is { Count: > 0 }
                ? renderMesh.AnimationFrames
                    .Select(texture => RegisterTexture(key.FileIndex, texture))
                    .ToArray()
                : renderMesh.Texture is SmoTexture texture &&
                  mesh.HasTextureCoordinates
                    ? [RegisterTexture(key.FileIndex, texture)]
                    : [];
            GpuTextureKey? baseTextureKey =
                renderMesh.BaseTexture is SmoTexture baseTexture &&
                mesh.HasTextureCoordinates && mesh.HasTextureCoordinates1
                    ? RegisterTexture(key.FileIndex, baseTexture)
                    : null;

            Matrix4x4 reflection = Matrix4x4.CreateScale(1, 1, -1);
            Matrix4x4 model = renderMesh.WorldTransform * reflection;
            bool transparent = RequiresTransparentOrdering(renderMesh);
            _items.Add(new GpuRenderItem(
                key,
                geometryKey,
                textureKeys,
                baseTextureKey,
                renderMesh.AnimationFrameDuration,
                model,
                transparent,
                renderMesh.MaterialRenderState?.UsesEmissiveApproximation == true,
                renderMesh.MaterialRenderState?
                    .UsesLuminanceCoverageApproximation == true,
                renderMesh.InitialSkinMatrices?.ToArray() ?? []));
            _appearances[key] = new GpuAppearance(true, false, 1);
        }

        private GpuTextureKey RegisterTexture(int fileIndex, SmoTexture texture)
        {
            var key = new GpuTextureKey(fileIndex, texture.ObjectIndex);
            if (!_pendingTextures.ContainsKey(key) && !_textures.ContainsKey(key))
                _pendingTextures.Add(key, texture);
            return key;
        }

        public void SetAppearance(
            SceneObjectKey key,
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
            SceneObjectKey key,
            IReadOnlyList<Matrix4x4> boneMatrices)
        {
            GpuRenderItem? item = _items.FirstOrDefault(candidate =>
                candidate.Key.Equals(key));
            if (item is not null)
                item.BoneMatrices = boneMatrices.ToArray();
        }

        public void SetModelTransform(SceneObjectKey key, Matrix4x4 modelTransform)
        {
            GpuRenderItem? item = _items.FirstOrDefault(candidate =>
                candidate.Key.Equals(key));
            if (item is not null)
            {
                item.Model = modelTransform * Matrix4x4.CreateScale(1, 1, -1);
            }
        }

        public void Clear()
        {
            _pendingGeometry.Clear();
            _pendingTextures.Clear();
            _items.Clear();
            _appearances.Clear();
            _uploadReport = null;
            _resetRequested = true;
            _textureAnimationStart = Stopwatch.GetTimestamp();
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
            double sceneDiagonal)
        {
            EnsureInitialized();
            if (_resetRequested)
            {
                DeleteSceneResources();
                _resetRequested = false;
            }
            UploadPendingResources();

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

                Matrix4x4 view = CreateView(camera);
                float aspect = width / (float)Math.Max(height, 1);
                double near = Math.Max(camera.NearPlaneDistance, 0.0001);
                double far = Math.Max(
                    camera.FarPlaneDistance, camera.NearPlaneDistance + 1);
                Matrix4x4 projection = camera switch
                {
                    OrthographicCamera orthographic => CreateOrthographicProjection(
                        orthographic.Width, aspect, near, far),
                    PerspectiveCamera perspective => CreateProjection(
                        perspective.FieldOfView, aspect, near, far),
                    _ => CreateProjection(45, aspect, near, far)
                };
                SetMatrix(_viewLocation, view);
                SetMatrix(_projectionLocation, projection);

                Vector3 cameraPosition = new(
                    (float)camera.Position.X,
                    (float)camera.Position.Y,
                    (float)camera.Position.Z);
                Matrix4x4 turntable = CreateTurntable(
                    turntableAngle,
                    turntableCenter);

                if (showFloorGrid)
                    DrawFloorGrid(
                        turntableCenter,
                        floorY,
                        gridSpan,
                        sceneDiagonal,
                        floorGridColor);

                GL.Disable(EnableCap.Blend);
                GL.DepthMask(true);
                foreach (GpuRenderItem item in _items.Where(item => !item.Transparent))
                    Draw(item, turntable);

                GL.Enable(EnableCap.Blend);
                GL.DepthMask(false);
                foreach (GpuRenderItem item in _items
                             .Where(item => item.Transparent)
                             .OrderByDescending(item => DistanceSquared(
                                 item,
                                 turntable,
                                 cameraPosition)))
                {
                    if (item.Additive)
                        GL.BlendFunc(BlendingFactor.SrcAlpha, BlendingFactor.One);
                    else
                        GL.BlendFunc(
                            BlendingFactor.SrcAlpha,
                            BlendingFactor.OneMinusSrcAlpha);
                    Draw(item, turntable);
                }
                GL.DepthMask(true);
                GL.Disable(EnableCap.Blend);
                GL.BindVertexArray(0);
                GL.UseProgram(0);
            }
            finally
            {
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
            double step = NiceGridStep(Math.Max(span / 20.0, 0.0001));
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

        private void Draw(GpuRenderItem item, Matrix4x4 turntable)
        {
            GpuAppearance appearance = _appearances.GetValueOrDefault(
                item.Key,
                new GpuAppearance(true, false, 1));
            if (!appearance.Visible || appearance.Opacity <= 0 ||
                !_geometry.TryGetValue(item.GeometryKey, out GpuGeometry? geometry))
            {
                return;
            }

            Matrix4x4 model = item.Model * turntable;
            SetMatrix(_modelLocation, model);
            GL.Uniform1(_highlightLocation, appearance.Highlighted ? 1 : 0);
            GL.Uniform1(_opacityLocation, appearance.Opacity);
            GL.Uniform1(
                _luminanceCoverageLocation,
                item.LuminanceCoverage ? 1 : 0);

            bool skinned = item.BoneMatrices.Length > 0;
            GL.Uniform1(_skinnedLocation, skinned ? 1 : 0);
            if (skinned)
                SetBoneMatrices(item.BoneMatrices);

            int texture = _whiteTexture;
            bool hasTexture = TryResolveAnimatedTexture(item, out texture);
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

        private bool TryResolveAnimatedTexture(
            GpuRenderItem item,
            out int texture)
        {
            texture = _whiteTexture;
            if (item.TextureKeys.Length == 0)
                return false;

            int frame = 0;
            if (item.TextureKeys.Length > 1 &&
                item.FrameDuration is TimeSpan duration &&
                duration > TimeSpan.Zero)
            {
                double elapsed = Stopwatch.GetElapsedTime(
                    _textureAnimationStart).TotalSeconds;
                frame = (int)(elapsed / duration.TotalSeconds) %
                        item.TextureKeys.Length;
            }
            return _textures.TryGetValue(item.TextureKeys[frame], out texture);
        }

        private void SetBoneMatrices(IReadOnlyList<Matrix4x4> matrices)
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
                _boneMatricesLocation,
                count,
                true,
                values);
        }

        private void EnsureInitialized()
        {
            if (_initialized)
                return;

            const string vertexShader = """
                #version 330 core
                layout(location = 0) in vec3 aPosition;
                layout(location = 1) in vec2 aUv;
                layout(location = 2) in vec4 aColor;
                layout(location = 3) in vec2 aUv1;
                layout(location = 4) in vec4 aBoneWeights;
                layout(location = 5) in vec4 aBoneIndices;
                uniform mat4 uModel;
                uniform mat4 uView;
                uniform mat4 uProjection;
                uniform int uSkinned;
                uniform mat4 uBones[32];
                out vec2 vUv;
                out vec2 vUv1;
                out vec4 vColor;

                void AddBone(
                    inout vec4 skinned,
                    inout float total,
                    vec4 localPosition,
                    float weight,
                    float rawIndex)
                {
                    int index = int(rawIndex + 0.5);
                    if (weight > 0.000001 && index >= 0 && index < 32)
                    {
                        skinned += (localPosition * uBones[index]) * weight;
                        total += weight;
                    }
                }

                void main()
                {
                    vec4 localPosition = vec4(aPosition, 1.0);
                    if (uSkinned != 0)
                    {
                        vec4 skinned = vec4(0.0);
                        float total = 0.0;
                        AddBone(skinned, total, localPosition,
                            aBoneWeights.x, aBoneIndices.x);
                        AddBone(skinned, total, localPosition,
                            aBoneWeights.y, aBoneIndices.y);
                        AddBone(skinned, total, localPosition,
                            aBoneWeights.z, aBoneIndices.z);
                        AddBone(skinned, total, localPosition,
                            aBoneWeights.w, aBoneIndices.w);
                        if (total > 0.000001)
                            localPosition = skinned / total;
                    }
                    gl_Position = localPosition *
                        uModel * uView * uProjection;
                    vUv = aUv;
                    vUv1 = aUv1;
                    vColor = aColor;
                }
                """;
            const string fragmentShader = """
                #version 330 core
                in vec2 vUv;
                in vec2 vUv1;
                in vec4 vColor;
                uniform sampler2D uTexture;
                uniform sampler2D uBaseTexture;
                uniform int uHasTexture;
                uniform int uHasBaseTexture;
                uniform int uHighlight;
                uniform int uLuminanceCoverage;
                uniform float uOpacity;
                out vec4 FragColor;
                void main()
                {
                    // TexImage2D maps the first uploaded SMO row to V=0, so
                    // native UV0 is already in the correct orientation here.
                    vec4 source;
                    if (uHasBaseTexture != 0 && uHasTexture != 0)
                    {
                        vec4 base = texture(uBaseTexture, vUv);
                        vec4 effect = texture(uTexture, vUv1);
                        source = vec4(
                            min(vec3(1.0), base.rgb + effect.rgb * effect.a),
                            base.a);
                    }
                    else
                    {
                        source = uHasTexture != 0
                            ? texture(uTexture, vUv)
                            : vec4(1.0);
                    }
                    vec4 result = source * vColor;
                    if (uLuminanceCoverage != 0)
                        result.a *= max(result.r, max(result.g, result.b));
                    result.a *= uOpacity;
                    if (result.a <= 0.001)
                        discard;
                    if (uHighlight != 0)
                        result.rgb = mix(result.rgb, vec3(1.0, 0.55, 0.10), 0.62);
                    FragColor = result;
                }
                """;

            _program = CreateProgram(vertexShader, fragmentShader);
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
            GL.UseProgram(_program);
            GL.Uniform1(GL.GetUniformLocation(_program, "uTexture"), 0);
            GL.Uniform1(GL.GetUniformLocation(_program, "uBaseTexture"), 1);
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
                _geometry.Add(key, new GpuGeometry(
                    vertexArray,
                    vertexBuffer,
                    indexBuffer,
                    pending.Indices.Length,
                    pending.Center));
            }
            _pendingGeometry.Clear();

            foreach ((GpuTextureKey key, SmoTexture texture) in _pendingTextures)
            {
                int handle = GL.GenTexture();
                GL.BindTexture(TextureTarget.Texture2D, handle);
                GL.PixelStore(PixelStoreParameter.UnpackAlignment, 4);
                GL.TexImage2D(
                    TextureTarget.Texture2D,
                    0,
                    PixelInternalFormat.Rgba8,
                    texture.Width,
                    texture.Height,
                    0,
                    OpenTK.Graphics.OpenGL4.PixelFormat.Bgra,
                    PixelType.UnsignedByte,
                    texture.Bgra32Pixels.ToArray());
                ConfigureTextureSampling(generateMipmaps: true);
                _textures.Add(key, handle);
            }
            _pendingTextures.Clear();
            timer.Stop();
            _uploadReport = new GpuUploadReport(
                geometryCount,
                textureCount,
                _items.Count,
                timer.Elapsed.TotalMilliseconds);
        }

        private static PendingGpuGeometry BuildGeometry(
            DecodedRenderMesh renderMesh,
            Color fallbackColor)
        {
            SmoMesh mesh = renderMesh.Mesh;
            float[] vertices = new float[checked(mesh.VertexCount * VertexFloatCount)];
            bool textured = renderMesh.Texture is not null &&
                            mesh.HasTextureCoordinates;
            bool useDiffuse = mesh.HasDiffuseColors &&
                              (!textured ||
                               SmoVertexColorUsage.ShouldModulateTexture(mesh));
            bool useAlpha = mesh.HasDiffuseColors &&
                SmoVertexColorUsage.ShouldUseVertexAlphaInPreview(
                    mesh,
                    renderMesh.MaterialRenderState);
            uint fallbackArgb = renderMesh.MaterialColorArgb ??
                ((uint)fallbackColor.A << 24 |
                 (uint)fallbackColor.R << 16 |
                 (uint)fallbackColor.G << 8 |
                 fallbackColor.B);
            if (textured && !useDiffuse)
                fallbackArgb = 0xFFFFFFFF;

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

        private static void ConfigureTextureSampling(bool generateMipmaps)
        {
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
                generateMipmaps
                    ? (int)TextureMinFilter.LinearMipmapLinear
                    : (int)TextureMinFilter.Linear);
            if (generateMipmaps)
                GL.GenerateMipmap(GenerateMipmapTarget.Texture2D);
        }

        private static Matrix4x4 CreateView(ProjectionCamera camera)
        {
            Vector3 position = new(
                (float)camera.Position.X,
                (float)camera.Position.Y,
                (float)camera.Position.Z);
            Vector3 look = new(
                (float)camera.LookDirection.X,
                (float)camera.LookDirection.Y,
                (float)camera.LookDirection.Z);
            Vector3 up = new(
                (float)camera.UpDirection.X,
                (float)camera.UpDirection.Y,
                (float)camera.UpDirection.Z);
            if (look.LengthSquared() < 0.000001f)
                look = -Vector3.UnitZ;
            if (up.LengthSquared() < 0.000001f)
                up = Vector3.UnitY;
            return Matrix4x4.CreateLookAt(position, position + look, up);
        }

        private static Matrix4x4 CreateProjection(
            double fieldOfViewDegrees,
            float aspect,
            double near,
            double far)
        {
            // WPF PerspectiveCamera.FieldOfView is horizontal, while the usual
            // OpenGL helper accepts a vertical angle. Build the equivalent
            // projection explicitly so the GL model and WPF helper overlays
            // occupy exactly the same pixels.
            float radians = (float)(fieldOfViewDegrees * Math.PI / 180.0);
            float horizontalScale = 1f / MathF.Tan(radians * 0.5f);
            float verticalScale = horizontalScale * Math.Max(aspect, 0.0001f);
            float nearValue = (float)near;
            float farValue = (float)far;
            return new Matrix4x4(
                horizontalScale, 0, 0, 0,
                0, verticalScale, 0, 0,
                0, 0,
                (farValue + nearValue) / (nearValue - farValue), -1,
                0, 0,
                2 * farValue * nearValue / (nearValue - farValue), 0);
        }

        private static Matrix4x4 CreateOrthographicProjection(
            double width,
            float aspect,
            double near,
            double far)
        {
            float widthValue = (float)Math.Max(width, 0.0001);
            float heightValue = widthValue / Math.Max(aspect, 0.0001f);
            float nearValue = (float)near;
            float farValue = (float)far;
            return new Matrix4x4(
                2 / widthValue, 0, 0, 0,
                0, 2 / heightValue, 0, 0,
                0, 0, -2 / (farValue - nearValue), 0,
                0, 0, -(farValue + nearValue) /
                          (farValue - nearValue), 1);
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

        private float DistanceSquared(
            GpuRenderItem item,
            Matrix4x4 turntable,
            Vector3 cameraPosition)
        {
            if (!_geometry.TryGetValue(
                    item.GeometryKey,
                    out GpuGeometry? geometry))
            {
                return float.NegativeInfinity;
            }
            return Vector3.DistanceSquared(
                Vector3.Transform(
                    geometry.Center,
                    item.Model * turntable),
                cameraPosition);
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
            Vector3 Center);

        private sealed class GpuRenderItem(
            SceneObjectKey key,
            GpuGeometryKey geometryKey,
            GpuTextureKey[] textureKeys,
            GpuTextureKey? baseTextureKey,
            TimeSpan? frameDuration,
            Matrix4x4 model,
            bool transparent,
            bool additive,
            bool luminanceCoverage,
            Matrix4x4[] boneMatrices)
        {
            public SceneObjectKey Key { get; } = key;
            public GpuGeometryKey GeometryKey { get; } = geometryKey;
            public GpuTextureKey[] TextureKeys { get; } = textureKeys;
            public GpuTextureKey? BaseTextureKey { get; } = baseTextureKey;
            public TimeSpan? FrameDuration { get; } = frameDuration;
            public Matrix4x4 Model { get; set; } = model;
            public bool Transparent { get; } = transparent;
            public bool Additive { get; } = additive;
            public bool LuminanceCoverage { get; } = luminanceCoverage;
            public Matrix4x4[] BoneMatrices { get; set; } = boneMatrices;
        }

        private readonly record struct GpuAppearance(
            bool Visible,
            bool Highlighted,
            float Opacity);
    }
}
