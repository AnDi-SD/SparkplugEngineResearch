using System.Collections.ObjectModel;
using SmoViewer.Core;

namespace SmoViewer.Sparkplug;

/// <summary>A live view of the reconstructed RenderNode cache, keyed by its container occurrence.</summary>
public sealed class SparkplugLightSelection
{
    private readonly List<int> _ordinary = new(8);
    public IReadOnlyList<int> OrdinaryObjectIndices { get; }
    public int? AmbientObjectIndex { get; private set; }
    internal SparkplugLightSelection() => OrdinaryObjectIndices = _ordinary.AsReadOnly();
    internal unsafe void Update(NativeMethods.SceneLightCache row, IReadOnlyDictionary<uint, int> indices)
    {
        if (row.Count > 8) throw new InvalidDataException("Native light cache exceeded its ABI capacity.");
        _ordinary.Clear();
        for (int i = 0; i < row.Count; ++i) _ordinary.Add(indices[row.Lights[i]]);
        AmbientObjectIndex = row.Ambient == 0 ? null : indices[row.Ambient];
    }
}

public sealed partial class SparkplugSceneRuntime
{
    private uint[] _documentObjectIds = [];
    private Dictionary<uint, int> _indicesById = [];
    private readonly Dictionary<int, SparkplugLightSelection> _lightSelections = [];
    private NativeMethods.SceneLightCache[] _lightCacheBuffer = [];
    private bool _lightingConfigured;
    private float _sampleTime;
    public bool LightingConfigured => _lightingConfigured;
    public IReadOnlyList<int> AvailableLightObjectIndices { get; private set; } = Array.Empty<int>();
    public IReadOnlyDictionary<int, SparkplugLightSelection> LightSelections { get; private set; } = null!;

    private unsafe void InitializeLighting(SmoDocument document)
    {
        _documentObjectIds = document.Objects.Select(entry => entry.Id).ToArray();
        _indicesById = document.Objects.ToDictionary(entry => entry.Id, entry => entry.Index);
        LightSelections = new ReadOnlyDictionary<int, SparkplugLightSelection>(_lightSelections);
        NativeMethods.Check(NativeMethods.spv_scene_light_ids(_handle, null, 0, out uint count));
        var ids = new uint[checked((int)count)];
        fixed (uint* output = ids) NativeMethods.Check(NativeMethods.spv_scene_light_ids(_handle, output, count, out _));
        AvailableLightObjectIndices = Array.AsReadOnly(ids.Select(id => _indicesById[id]).ToArray());
    }

    /// <summary>Explicit host preview policy: register all loaded lights in graph order and activate the hierarchy.</summary>
    public void EnableDocumentLighting() => ConfigureLighting(AvailableLightObjectIndices, activeHierarchy: true);

    /// <summary>Register an explicit ordered selection; eligibility and capacity execute in Sparkplug C++.</summary>
    public unsafe void ConfigureLighting(IReadOnlyList<int> lightObjectIndices, bool activeHierarchy)
    {
        ObjectDisposedException.ThrowIf(_handle.IsClosed, this);
        ArgumentNullException.ThrowIfNull(lightObjectIndices);
        if (_lightingConfigured) throw new InvalidOperationException("Clear existing lighting before configuring a new selection.");
        var ids = new uint[lightObjectIndices.Count];
        for (int i = 0; i < ids.Length; ++i)
        {
            int index = lightObjectIndices[i];
            if ((uint)index >= (uint)_documentObjectIds.Length) throw new ArgumentOutOfRangeException(nameof(lightObjectIndices));
            ids[i] = _documentObjectIds[index];
        }
        fixed (uint* input = ids) NativeMethods.Check(NativeMethods.spv_scene_lighting_configure(_handle, input, (uint)ids.Length, activeHierarchy ? 1u : 0u));
        try
        {
            NativeMethods.Check(NativeMethods.spv_scene_lighting_caches(_handle, null, 0, out uint count));
            _lightCacheBuffer = new NativeMethods.SceneLightCache[checked((int)count)];
            _lightingConfigured = true;
            Sample(_sampleTime);
        }
        catch { ClearLighting(); throw; }
    }

    public void SetLightingActive(bool activeHierarchy)
    {
        ObjectDisposedException.ThrowIf(_handle.IsClosed, this);
        NativeMethods.Check(NativeMethods.spv_scene_lighting_active(_handle, activeHierarchy ? 1u : 0u));
        Sample(_sampleTime);
    }

    public void ClearLighting()
    {
        ObjectDisposedException.ThrowIf(_handle.IsClosed, this);
        NativeMethods.Check(NativeMethods.spv_scene_lighting_clear(_handle));
        _lightingConfigured = false;
        _lightCacheBuffer = [];
        _lightSelections.Clear();
    }

    private unsafe void RefreshLighting()
    {
        if (!_lightingConfigured) return;
        fixed (NativeMethods.SceneLightCache* output = _lightCacheBuffer)
            NativeMethods.Check(NativeMethods.spv_scene_lighting_caches(_handle, output, (uint)_lightCacheBuffer.Length, out _));
        foreach (var row in _lightCacheBuffer)
        {
            int container = _indicesById[row.RenderNode];
            if (!_lightSelections.TryGetValue(container, out var selection))
                _lightSelections.Add(container, selection = new SparkplugLightSelection());
            selection.Update(row, _indicesById);
        }
    }
}
