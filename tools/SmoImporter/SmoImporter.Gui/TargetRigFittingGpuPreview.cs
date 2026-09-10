using System.Numerics;
using System.Runtime.InteropServices;
using System.Windows.Media;
using System.Windows.Media.Media3D;
using OpenTK.Graphics.OpenGL4;
using OpenTK.Mathematics;
using OpenTK.Windowing.Common;
using OpenTK.Windowing.Desktop;
using OpenTK.Windowing.GraphicsLibraryFramework;
using SmoExporter.Core;
using SmoImporter.Core;
using SmoViewer.Core;
using SmoViewer.Rendering.Wpf;
using SmoViewer.Scene;
using Vector3 = System.Numerics.Vector3;

namespace SmoImporter.Gui;

/// <summary>
/// Windows host for the shared production shader. The authoring scene is already
/// in external coordinates; local readback applies no additional world/Z mapping.
/// One hidden context and uploaded scene survive fitting-palette edits.
/// </summary>
public sealed class TargetRigFittingGpuPreview : IDisposable
{
    private readonly int _thread = Environment.CurrentManagedThreadId;
    private readonly SmoGpuSceneRenderer _renderer = new();
    private readonly Dictionary<SmoExportMesh, SmoRenderObjectKey> _keys = new(ReferenceEqualityComparer.Instance);
    private NativeWindow? _window;
    private SmoDocument? _document;
    private SmoExportScene? _scene;
    private bool _disposed;

    public int ContextCreationCount { get; private set; }
    public int SceneUploadCount { get; private set; }
    public int ReadbackCount { get; private set; }
    public int CachedMeshCount => _keys.Count;
    public bool IsDisposed => _disposed;
    public string Device { get; private set; } = "";
    public string Version { get; private set; } = "";

    public TargetRigFittingPreviewResult Build(SmoDocument document, SmoExportScene scene,
        TargetRigFittingPoseSnapshot pose)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(scene);
        ArgumentNullException.ThrowIfNull(pose);
        VerifyAccess();
        if (scene.Meshes.All(mesh => mesh.SkinObjectIndex is null))
        {
            TargetRigFittingPreviewResult result = TargetRigFittingPreviewBuilder.Build(scene, pose,
                _ => throw new InvalidOperationException("An unskinned preview requested GPU deformation."));
            if (_scene is not null) Clear();
            return result;
        }
        try
        {
            return WithContext(() =>
            {
                EnsureScene(document, scene);
                return TargetRigFittingPreviewBuilder.Build(scene, pose, request =>
                {
                    if (request.PaletteTransforms.Count is < 1 or > 32)
                        throw new InvalidOperationException("The GPU fitting preview requires a palette of 1 to 32 matrices.");
                    SmoRenderObjectKey key = _keys[request.Mesh];
                    _renderer.SetBoneMatrices(key, request.PaletteTransforms);
                    Vector3[] result = _renderer.ReadPositionsForPicking(key, request.Mesh.ObjectIndex);
                    ++ReadbackCount;
                    return result;
                });
            });
        }
        catch (Exception error) when (error is not InvalidOperationException and not ObjectDisposedException)
        {
            throw new InvalidOperationException("Shared GPU target fitting preview failed: " + error.Message, error);
        }
    }

    private void EnsureScene(SmoDocument document, SmoExportScene scene)
    {
        if (ReferenceEquals(_document, document) && ReferenceEquals(_scene, scene))
            return;
        _renderer.Clear();
        _keys.Clear();
        _document = null;
        _scene = null;
        var templates = new Dictionary<int, SmoMesh>();
        for (int meshSlot = 0; meshSlot < scene.Meshes.Count; ++meshSlot)
        {
            SmoExportMesh mesh = scene.Meshes[meshSlot];
            if (mesh.SkinObjectIndex is null)
                continue;
            SmoExportSkin skin = scene.Skins.SingleOrDefault(value => value.ObjectIndex == mesh.SkinObjectIndex)
                ?? throw new InvalidOperationException("A target mesh has no actual skin palette.");
            int paletteCount = skin.JointObjectIndices.Count;
            if (paletteCount is < 1 or > 32)
                throw new InvalidOperationException("The GPU fitting preview requires a palette of 1 to 32 matrices.");
            if (mesh.BlendWeights.Length != mesh.Positions.Length || mesh.JointIndices.Length != mesh.Positions.Length)
                throw new InvalidOperationException("A target mesh has incomplete skin influence arrays.");
            if (!templates.TryGetValue(mesh.ObjectIndex, out SmoMesh? template))
            {
                template = SmoMeshDecoder.Decode(document, document.Objects[mesh.ObjectIndex]);
                templates.Add(mesh.ObjectIndex, template);
            }
            var indices = new SmoBlendIndices[mesh.JointIndices.Length];
            for (int vertex = 0; vertex < indices.Length; ++vertex)
            {
                var value = mesh.JointIndices[vertex];
                indices[vertex] = new(ExactByte(value.X), ExactByte(value.Y), ExactByte(value.Z), ExactByte(value.W));
                var weights = mesh.BlendWeights[vertex];
                ValidateInfluence(weights.X, indices[vertex].X, paletteCount);
                ValidateInfluence(weights.Y, indices[vertex].Y, paletteCount);
                ValidateInfluence(weights.Z, indices[vertex].Z, paletteCount);
                ValidateInfluence(weights.W, indices[vertex].W, paletteCount);
            }
            SmoMesh transient = SmoMesh.CreateTransient(template, mesh.Positions, [], [], [],
                mesh.BlendWeights, indices, mesh.TriangleIndices);
            // Export variants may contain distinct external-space geometry for
            // the same physical resource. FileIndex is this host's cache slot;
            // ObjectIndex remains the original resource/renderable identity.
            var key = new SmoRenderObjectKey(meshSlot, mesh.RenderableObjectIndex ?? mesh.ObjectIndex);
            var renderMesh = new SmoSceneMesh(transient, key.ObjectIndex, null, null, null, null,
                null, null, false, null, null, null, Matrix4x4.Identity, mesh.SkinObjectIndex,
                null, null, new Dictionary<int, float>());
            _renderer.Add(renderMesh, key, Colors.White);
            _keys.Add(mesh, key);
        }
        RenderUploads();
        ++SceneUploadCount;
        _document = document;
        _scene = scene;
    }

    private static byte ExactByte(float value)
    {
        if (!float.IsFinite(value) || value < 0 || value > byte.MaxValue || value != MathF.Truncate(value))
            throw new InvalidOperationException("A target mesh joint index is not an original byte palette index.");
        return checked((byte)value);
    }

    private static void ValidateInfluence(float weight, byte joint, int paletteCount)
    {
        if (!float.IsFinite(weight))
            throw new InvalidOperationException("A target mesh has a non-finite skin weight.");
        // Preserve every finite raw weight. Only nonzero influences read a
        // matrix which must belong to this mesh's actual palette.
        if (weight != 0 && joint >= paletteCount)
            throw new InvalidOperationException("A target mesh has an active joint outside its actual skin palette.");
    }

    private void RenderUploads()
    {
        var camera = new PerspectiveCamera(new Point3D(0, 0, 5), new Vector3D(0, 0, -1),
            new Vector3D(0, 1, 0), 45) { NearPlaneDistance = .01, FarPlaneDistance = 1000 };
        _renderer.Render(16, 16, camera, Colors.Black, 0, new Point3D(), false, Colors.Gray, 0, 10, 10);
    }

    public void Clear()
    {
        VerifyAccess();
        _document = null;
        _scene = null;
        _keys.Clear();
        if (_window is not null)
            WithContext(() => { _renderer.Clear(); RenderUploads(); return 0; });
    }

    private unsafe T WithContext<T>(Func<T> action)
    {
        GLFWProvider.EnsureInitialized();
        var previousWindow = GLFW.GetCurrentContext();
        IntPtr previousContext = WglGetCurrentContext(), previousDc = WglGetCurrentDC();
        try
        {
            if (_window is null)
            {
                _window = new NativeWindow(new NativeWindowSettings
                {
                    ClientSize = new Vector2i(16, 16), StartVisible = false, StartFocused = false,
                    API = ContextAPI.OpenGL, APIVersion = new System.Version(3, 3),
                    Profile = ContextProfile.Core, Flags = ContextFlags.ForwardCompatible,
                    AutoLoadBindings = true
                });
                _window.MakeCurrent();
                ++ContextCreationCount;
                _renderer.SetAntialiasingSamples(0);
                Device = GL.GetString(StringName.Renderer);
                Version = GL.GetString(StringName.Version);
            }
            _window.MakeCurrent();
            return action();
        }
        finally
        {
            GLFW.MakeContextCurrent(previousWindow);
            // Also preserve a host context not created by GLFW, if one exists.
            if (WglGetCurrentContext() != previousContext && !WglMakeCurrent(previousDc, previousContext))
                throw new InvalidOperationException("Could not restore the previous OpenGL context.");
        }
    }

    private void VerifyAccess()
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (Environment.CurrentManagedThreadId != _thread)
            throw new InvalidOperationException("The fitting GPU host belongs to its creating UI thread.");
    }

    public void Dispose()
    {
        if (_disposed) return;
        VerifyAccess();
        try
        {
            if (_window is not null)
                WithContext(() =>
                {
                    try { _renderer.Clear(); RenderUploads(); }
                    finally { _window.Dispose(); _window = null; }
                    return 0;
                });
        }
        finally
        {
            _keys.Clear();
            _document = null;
            _scene = null;
            _disposed = true;
        }
    }

    [DllImport("opengl32.dll", EntryPoint = "wglGetCurrentContext")]
    private static extern IntPtr WglGetCurrentContext();
    [DllImport("opengl32.dll", EntryPoint = "wglGetCurrentDC")]
    private static extern IntPtr WglGetCurrentDC();
    [DllImport("opengl32.dll", EntryPoint = "wglMakeCurrent")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool WglMakeCurrent(IntPtr dc, IntPtr context);
}
