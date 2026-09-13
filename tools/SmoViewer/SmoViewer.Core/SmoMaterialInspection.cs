using System.Runtime.CompilerServices;
using System.Diagnostics.CodeAnalysis;
using SmoViewer.Sparkplug;

namespace SmoViewer.Core;

/// <summary>
/// One shared-reader snapshot per immutable document entry. This cache contains
/// metadata copied from native objects; it is not a runtime material graph.
/// </summary>
public static class SmoMaterialInspection
{
    /// <summary>Absolute wire extent observed by the common reference reader.</summary>
    public sealed record ReferenceRange(int AbsolutePayloadOffset, uint Size);
    public sealed record Layer(int Index, uint ClassId, int TextureStatesFieldType,
        IReadOnlyList<uint> TextureStates, ReferenceRange? TextureReference);
    public sealed record Pass(int Index, uint FinalBlendOperation, IReadOnlyList<Layer> Layers);
    public sealed record State(IReadOnlyList<uint> RenderStates, IReadOnlyList<Pass> Passes);

    /// <summary>
    /// Projects the same cached native inspection used by the other material
    /// consumers. Every pass/layer and its final observed texture assignment is
    /// retained. States include actual constructor defaults; StatesField -1
    /// distinguishes them from an authored field8/17. References are metadata,
    /// not resolved resource ownership or an authoring selection policy.
    /// </summary>
    public static unsafe bool TryInspect(SmoDocument document, SmoObjectEntry entry,
        [NotNullWhen(true)] out State? value, out string error)
    {
        value = null;
        if (!TryRead(document, entry, out var snapshot, out error)) return false;
        var info = snapshot!.Info;
        var passes = new Pass[snapshot.Passes.Length];
        for (int p = 0; p < passes.Length; ++p)
        {
            var layers = new List<Layer>();
            foreach (var source in snapshot.Layers.Where(layer => layer.Pass == p))
            {
                var layer = source;
                ReferenceRange? reference = null;
                if (layer.Texture.Size != 0)
                {
                    if ((ulong)layer.Texture.Offset + layer.Texture.Size > entry.SerializedSize - 8)
                    {
                        error = "Observed material texture reference exceeds its owning payload.";
                        return false;
                    }
                    reference = new(checked((int)entry.PhysicalOffset + 8 + (int)layer.Texture.Offset), layer.Texture.Size);
                }
                layers.Add(new(checked((int)layer.Index), layer.ClassId, layer.StatesField,
                    Array.AsReadOnly(new ReadOnlySpan<uint>(layer.States, 9).ToArray()), reference));
            }
            passes[p] = new(p, snapshot.Passes[p].Blend, layers.AsReadOnly());
        }
        value = new(Array.AsReadOnly(new ReadOnlySpan<uint>(info.States, 11).ToArray()), Array.AsReadOnly(passes));
        return true;
    }

    internal sealed record Snapshot(NativeMethods.MaterialInfo Info,
        NativeMethods.MaterialPass[] Passes, NativeMethods.MaterialLayer[] Layers);
    private sealed record Result(Snapshot? Value, string Error);
    private sealed class Cache
    {
        internal readonly Dictionary<int, Result> Entries = new();
    }
    private static readonly ConditionalWeakTable<SmoDocument, Cache> Caches = new();

    internal static bool TryRead(SmoDocument document, SmoObjectEntry entry,
        out Snapshot? value, out string error)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(entry);
        value = null;
        if (entry.TypeHash != SmoClassIds.MaterialData || !entry.IsWithinDataSection ||
            !entry.SignatureMatches || entry.PhysicalOffset < 0 || entry.SerializedSize < 9 ||
            entry.PhysicalEnd > document.Data.Length || entry.SerializedSize > int.MaxValue ||
            (uint)entry.Index >= (uint)document.Objects.Count || !ReferenceEquals(document.Objects[entry.Index], entry))
        {
            error = "Material inspector requires a complete catalogued spMaterialData.";
            return false;
        }
        var cache = Caches.GetValue(document, static _ => new Cache());
        lock (cache.Entries)
        {
            if (!cache.Entries.TryGetValue(entry.Index, out var result))
            {
                result = Read(document, entry);
                cache.Entries.Add(entry.Index, result);
            }
            value = result.Value;
            error = result.Error;
            return value is not null;
        }
    }

    private static unsafe Result Read(SmoDocument document, SmoObjectEntry entry)
    {
        var payload = document.Data.Span.Slice(checked((int)entry.PhysicalOffset + 8), checked((int)entry.SerializedSize - 8));
        try
        {
            IntPtr pointer;
            fixed (byte* bytes = payload)
                pointer = NativeMethods.Check(NativeMethods.spv_material_read(bytes, (uint)payload.Length));
            using var handle = new MaterialHandle(pointer);
            NativeMethods.Check(NativeMethods.spv_material_info(handle, out var info));
            var passes = new NativeMethods.MaterialPass[checked((int)info.Passes)];
            fixed (NativeMethods.MaterialPass* output = passes)
                NativeMethods.Check(NativeMethods.spv_material_passes(handle, output, info.Passes));
            var layers = new NativeMethods.MaterialLayer[checked((int)info.Layers)];
            fixed (NativeMethods.MaterialLayer* output = layers)
                NativeMethods.Check(NativeMethods.spv_material_layers(handle, output, info.Layers));
            return new(new Snapshot(info, passes, layers), string.Empty);
        }
        catch (InvalidDataException exception) { return new(null, exception.Message); }
    }
}
