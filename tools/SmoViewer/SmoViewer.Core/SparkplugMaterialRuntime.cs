using SmoViewer.Sparkplug;

namespace SmoViewer.Core;

public sealed record SmoMaterialControllerClock(uint ClassId, float Accumulated,
    float Applied, float? Playback, bool Enabled);
public sealed record SmoMaterialUVSubmission(uint Stage, IReadOnlyList<float> Matrix3x3);
public sealed record SmoMaterialPassUpdate(SmoLoadedMaterial Material,
    IReadOnlyList<SmoMaterialUVSubmission> UVSubmissions);

/// <summary>
/// Live view of the scene's actual loaded material graph. The caller supplies
/// controller deltas, pass order and frame numbers; this is not a replacement
/// for the game's AnimationManager, visibility traversal or frame scheduler.
/// Lifetime is owned by SparkplugSceneRuntime. Calls must be serialized by the host.
/// </summary>
public sealed partial class SparkplugMaterialRuntime : IDisposable
{
    private readonly GraphHandle _graph;
    private readonly SparkplugMaterialView _view;
    private readonly IReadOnlyList<SmoObjectEntry> _entries;
    private readonly IReadOnlyDictionary<uint, int> _indices;

    internal SparkplugMaterialRuntime(SmoDocument document, GraphHandle graph, SparkplugMaterialView? view = null)
    {
        _graph = graph;
        _view = view ?? new SparkplugMaterialView(document, graph);
        _entries = document.Objects;
        _indices = document.Objects.ToDictionary(entry => entry.Id, entry => entry.Index);
    }

    private uint Id(int index)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        ObjectDisposedException.ThrowIf(_graph.IsClosed, this);
        if ((uint)index >= (uint)_entries.Count)
            throw new ArgumentOutOfRangeException(nameof(index), "Resource is outside this document.");
        return _entries[index].Id;
    }

    public SmoLoadedMaterial ReadMaterial(int materialObjectIndex)
    {
        var (value, issue) = _view.Material(Id(materialObjectIndex), refresh: true);
        return value ?? throw new InvalidDataException(issue ?? "Missing actual loaded material.");
    }

    public SmoSkyBoxPose ReadSkyPose(int skyObjectIndex)=>SmoSkyBoxPose.Read(_graph,Id(skyObjectIndex));

    /// <summary>Reference projection only; membership is not an update schedule.</summary>
    public IReadOnlyList<int> ReferencedControllers(int materialObjectIndex)
    {
        var material = ReadMaterial(materialObjectIndex);
        var ids = new List<uint> { material.ColorControllerId };
        foreach (var layer in material.Passes.SelectMany(pass => pass.Layers))
        {
            ids.Add(layer.Animation?.ObjectId ?? 0);
            ids.Add(layer.UvControllerId);
        }
        return Array.AsReadOnly(ids.Where(id => id != 0).Distinct().Select(id => _indices[id]).ToArray());
    }

    public SmoMaterialControllerClock ReadClock(int controllerObjectIndex)
    {
        NativeMethods.Check(NativeMethods.spv_graph_controller_clock(_graph, Id(controllerObjectIndex), out var value));
        return new(value.ClassId, value.Accumulated, value.Applied,
            value.HasPlayback != 0 ? value.Playback : null, value.Enabled != 0);
    }

    /// <summary>
    /// Applies one elapsed input to each explicitly selected controller. It
    /// calls the actual Apply method without inventing the manager's enable gate.
    /// Duplicates and invalid IDs are rejected before any clock changes.
    /// </summary>
    public unsafe void ApplyControllers(IEnumerable<int> controllerObjectIndices, float elapsed)
    {
        ArgumentNullException.ThrowIfNull(controllerObjectIndices);
        ObjectDisposedException.ThrowIf(_graph.IsClosed, this);
        var ids = controllerObjectIndices.Select(Id).ToArray();
        fixed (uint* input = ids)
            NativeMethods.Check(NativeMethods.spv_graph_apply_controllers(_graph, input, checked((uint)ids.Length), elapsed));
    }

    public bool UpdateColor(int materialObjectIndex, uint frame, bool force = false)
    {
        NativeMethods.Check(NativeMethods.spv_graph_update_material_color(_graph,
            Id(materialObjectIndex), frame, force ? 1u : 0u, out uint evaluated));
        return evaluated != 0;
    }

    /// <summary>
    /// Executes one actual pass and captures its UV submissions. A failed
    /// original update can retain changes from preceding layers; no rollback
    /// or substitute evaluation is performed. ReadMaterial can inspect that state.
    /// </summary>
    public unsafe SmoMaterialPassUpdate UpdatePass(int materialObjectIndex, uint pass)
    {
        uint id = Id(materialObjectIndex);
        var native = new NativeMethods.GraphUVSubmission[8];
        uint count;
        fixed (NativeMethods.GraphUVSubmission* output = native)
            NativeMethods.Check(NativeMethods.spv_graph_update_material_pass(_graph, id, pass, output, 8, out count));
        var submissions = new SmoMaterialUVSubmission[checked((int)count)];
        for (int i = 0; i < submissions.Length; ++i)
        {
            var value = native[i];
            submissions[i] = new(value.Stage,
                Array.AsReadOnly(new ReadOnlySpan<float>(value.Matrix, 9).ToArray()));
        }
        return new(ReadMaterial(materialObjectIndex), Array.AsReadOnly(submissions));
    }
}
