using System.Numerics;
using SmoViewer.Core;

namespace SmoViewer.Sparkplug;

/// <summary>SMO application adapter; node world updates, PRS and skin matrices execute in Sparkplug C++.</summary>
public sealed partial class SparkplugSceneRuntime : IDisposable
{
    private readonly SceneHandle _handle;
    private readonly GraphHandle _graph;
    private readonly int[] _objectIndices;
    private readonly string[] _names;
    private readonly Matrix4x4[] _worldBuffer;
    private readonly Dictionary<int, Matrix4x4> _worlds = [];
    private readonly Dictionary<int, (uint SkinId, Matrix4x4[] Matrices)> _palettes = [];
    public IReadOnlyDictionary<int, Matrix4x4> Worlds => _worlds;
    public SparkplugMaterialRuntime Materials { get; }
    public IReadOnlyList<string> CompatibilityIssues { get; }
    public const string Backend = "Sparkplug C++: spAnimation / spTransformTrackEval / spNodeController / spNode / spSkin";

    public unsafe SparkplugSceneRuntime(SmoDocument document, IReadOnlyDictionary<int, SmoSkin> skins, bool enableDocumentLighting = false)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(skins);
        IntPtr pointer;
        fixed (byte* bytes = document.Data.Span)
            pointer = NativeMethods.Check(NativeMethods.spv_graph_load_for_tools(bytes, checked((uint)document.Data.Length),0));
        _graph = new GraphHandle(pointer);
        try
        {
            CompatibilityIssues=Array.AsReadOnly(SparkplugGraphCompatibility.ReadTextureIds(_graph).Select(SparkplugGraphCompatibility.Describe).ToArray());
            _handle = new(NativeMethods.Check(NativeMethods.spv_graph_scene_all(_graph)));
            Materials = new SparkplugMaterialRuntime(document, _graph);
            NativeMethods.Check(NativeMethods.spv_scene_node_count(_handle, out uint count));
            var ids = new uint[checked((int)count)];
            fixed (uint* output = ids) NativeMethods.Check(NativeMethods.spv_scene_graph_node_ids(_handle, output, count));
            var entries = document.Objects.ToDictionary(entry => entry.Id);
            _objectIndices = ids.Select(id => entries[id].Index).ToArray();
            _names = ids.Select(id => entries[id].Name).ToArray();
            _worldBuffer = new Matrix4x4[ids.Length];
            InitializeLighting(document);
            // Keys select actual loaded Skin resources. Caller DTO matrices do
            // not replace the graph's bone links or inverse-bind arrays.
            foreach (int index in skins.Keys)
            {
                if ((uint)index >= (uint)document.Objects.Count) throw new InvalidDataException("Skin selection is outside this document.");
                uint skinId = document.Objects[index].Id;
                NativeMethods.Check(NativeMethods.spv_scene_graph_skin_info(_handle, skinId, out _, out uint bones));
                _palettes.Add(index, (skinId, new Matrix4x4[checked((int)bones)]));
            }
            Sample(0);
            if (enableDocumentLighting) EnableDocumentLighting();
        }
        catch { Materials?.Dispose(); _handle?.Dispose(); _graph.Dispose(); throw; }
    }
    public unsafe IReadOnlyList<string> Bind(SparkplugAnimationClip clip)
    {
        var warnings = new List<string>();
        var rolesByName = SmoAnimationBinding.SelectRoles(clip.Tracks.Select(t =>
            (t.NodeName, t.PositionKeys, t.RotationKeys, t.ScaleKeys)), warnings);
        int[] bindings = Enumerable.Repeat(-1, _names.Length*3).ToArray();
        for (int i = 0; i < _names.Length; i++) if (rolesByName.TryGetValue(_names[i], out var roles)) roles.CopyTo(bindings, i*3);
        fixed (int* input = bindings) NativeMethods.Check(NativeMethods.spv_scene_bind(_handle, clip.Handle, input, (uint)bindings.Length));
        return warnings;
    }
    public unsafe void Sample(float time)
    {
        ObjectDisposedException.ThrowIf(_handle.IsClosed, this);
        fixed (Matrix4x4* output = _worldBuffer) NativeMethods.Check(NativeMethods.spv_scene_sample(_handle, time, output, (uint)_worldBuffer.Length*16));
        for (int i = 0; i < _objectIndices.Length; i++) _worlds[_objectIndices[i]] = _worldBuffer[i];
        foreach (var (skinId, matrices) in _palettes.Values)
            fixed (Matrix4x4* output = matrices)
                NativeMethods.Check(NativeMethods.spv_scene_graph_skin_palette(_handle, skinId, output, (uint)matrices.Length*16));
        RefreshLighting();
        _sampleTime = time;
    }
    public Matrix4x4[] Palette(int skinObjectIndex) => _palettes[skinObjectIndex].Matrices;
    public void Dispose() { Materials.Dispose(); _handle.Dispose(); _graph.Dispose(); }
}
