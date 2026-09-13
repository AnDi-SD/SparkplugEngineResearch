using System.Collections;
using System.IO;
using System.Reflection;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Media3D;
using OpenTK.Graphics.OpenGL4;
using OpenTK.Mathematics;
using OpenTK.Windowing.Common;
using OpenTK.Windowing.Desktop;
using SmoViewer;
using SmoViewer.Scene;

// Owns only a hidden test context. Every upload, vertex evaluation, readback
// and companion coordinate mapping remains in the actual production window.
internal sealed class GpuAnimationCapture : IDisposable
{
    private const BindingFlags Hidden = BindingFlags.Instance | BindingFlags.NonPublic;
    private readonly MainWindow _window;
    private readonly NativeWindow _context;
    private readonly Action<bool, string> _check;
    private static object? Field(object owner, string name) => owner.GetType().GetField(name, Hidden)!.GetValue(owner);
    private static object? Property(object owner, string name) => owner.GetType().GetProperty(name)!.GetValue(owner);
    private object? Call(string name, params object?[] arguments) =>
        typeof(MainWindow).GetMethod(name, Hidden)!.Invoke(_window, arguments);

    public string Device { get; }
    public string Version { get; }
    public int Frames { get; private set; }
    public int RefreshedMeshes { get; private set; }

    public GpuAnimationCapture(MainWindow window, Action<bool, string> check)
    {
        _window = window;
        _check = check;
        // Sharing/resetting an already initialized GLWpf renderer would need a
        // different test contract. A never-shown window has not uploaded yet.
        object renderer = Field(window, "_gpuRenderer")!;
        check(!window.IsVisible && !(bool)Field(renderer, "_initialized")!,
            "Hidden GUI capture starts before production resources belong to another GL context.");
        _context = new NativeWindow(new NativeWindowSettings
        {
            ClientSize = new Vector2i(16, 16), StartVisible = false, StartFocused = false,
            API = ContextAPI.OpenGL, APIVersion = new System.Version(3, 3),
            Profile = ContextProfile.Core, Flags = ContextFlags.ForwardCompatible,
            AutoLoadBindings = true
        });
        try
        {
            _context.MakeCurrent();
            Device = GL.GetString(StringName.Renderer);
            Version = GL.GetString(StringName.Version);
        }
        catch
        {
            _context.Dispose();
            throw;
        }
    }

    public void RefreshCompanions()
    {
        _context.MakeCurrent();
        var snapshots = new List<(MeshGeometry3D Geometry, Point3DCollection Before, int Count)>();
        var scenes = (IDictionary)Field(_window, "_sceneGeometry")!;
        foreach (DictionaryEntry item in scenes)
        {
            object scene = item.Value!;
            var renderMesh = (SmoSceneMesh)Property(scene, "RenderMesh")!;
            if (!renderMesh.Mesh.HasSkinningData)
                continue;
            if (!(bool)Property(scene, "GpuRendered")! ||
                !(bool)Call("IsGuiGeometryVisible", item.Key, scene)!)
                throw new InvalidDataException("The existing animation fixture has a skin which cannot receive production GPU readback.");
            var geometry = (MeshGeometry3D)Property(scene, "Geometry")!;
            int count = (Property(scene, "SourceVertexIndices") as int[])?.Length ?? renderMesh.Mesh.VertexCount;
            snapshots.Add((geometry, geometry.Positions, count));
        }
        // The existing bbush case is rigid; its empty skin set is intentional.
        _check(!_window.IsVisible && !_context.IsVisible,
            "Both windows remain hidden for skinned and rigid animation fixtures.");
        Call("SelectObjectAt", new Point(-1, -1));
        _check(Field(_window, "_pendingGpuPickPoint") is Point { X: -1, Y: -1 },
            "Production selection queues the negative hit test for the next GL frame.");
        Call("GpuViewport_OnRender", TimeSpan.Zero);
        string diagnostics = string.Join(" | ", ((ListBox)_window.FindName("StateLog")).Items.Cast<object>().TakeLast(3));
        _check((bool)Field(_window, "_gpuRendererAvailable")! &&
            Field(_window, "_pendingGpuPickPoint") is null,
            "Production GPU frame completed its queued picking request. " + diagnostics);
        _check(snapshots.All(item => !ReferenceEquals(item.Before, item.Geometry.Positions) &&
            item.Geometry.Positions.Count == item.Count),
            "Every captured skin received fresh production readback positions; stale/raw companions are rejected. " + diagnostics);
        _check(GL.GetError() == ErrorCode.NoError,
            "Hidden production frame and companion readback completed without GL errors.");
        var productionRenderer=(SmoViewer.Rendering.Wpf.SmoGpuSceneRenderer)Field(_window,"_gpuRenderer")!;
        _check(productionRenderer.RenderIssues.Count==0,
            "Production frame uses available common materials, selected shader lights and alpha order: "+string.Join("; ",productionRenderer.RenderIssues.Take(4)));
        Frames++;
        RefreshedMeshes += snapshots.Count;
    }

    public void Dispose() => _context.Dispose();
}
