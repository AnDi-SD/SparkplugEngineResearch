using System.Numerics;
using SmoViewer.Sparkplug;

namespace SmoViewer.Core;

public readonly record struct SmoSkinPaletteBinding(uint NodeObjectId, Matrix4x4 InverseBindMatrix);

/// <summary>
/// One operation's actual loaded bone owners for the shared Skin field writer.
/// The caller must retain the referenced source Node payloads and IDs in its
/// destination. This class does not save a container or relocate its objects.
/// </summary>
public sealed class SmoSkinPaletteWriter : IDisposable
{
    private readonly GraphHandle graph;

    public unsafe SmoSkinPaletteWriter(SmoDocument source)
    {
        ArgumentNullException.ThrowIfNull(source);
        fixed (byte* input = source.Data.Span)
            graph = new(NativeMethods.Check(NativeMethods.spv_graph_load_with_trace(
                input, checked((uint)source.Data.Length))));
    }

    /// <summary>One field, without a section terminator; raw matrix bits are retained.</summary>
    public unsafe byte[] WriteField(uint weightCount, IReadOnlyList<SmoSkinPaletteBinding> bindings)
    {
        ArgumentNullException.ThrowIfNull(bindings);
        ObjectDisposedException.ThrowIf(graph.IsClosed, this);
        if (bindings.Count > 1024)
            throw new ArgumentOutOfRangeException(nameof(bindings), "Palette exceeds the bounded writer input.");
        var input = new NativeMethods.SkinPaletteBinding[bindings.Count];
        for (int i = 0; i < input.Length; i++)
            input[i] = new() { NodeId = bindings[i].NodeObjectId, InverseBind = bindings[i].InverseBindMatrix };
        SerializedBytesHandle owner;
        fixed (NativeMethods.SkinPaletteBinding* values = input)
            owner = new(NativeMethods.Check(NativeMethods.spv_skin_write_palette(
                graph, weightCount, values, (uint)input.Length)));
        return SmoMeshDataWriter.CopyResult(owner);
    }

    /// <summary>Exact actual reader location; repeated assignments need a separate editing policy.</summary>
    public static SmoDataBlockHeader GetSinglePaletteField(SmoDocument source, SmoObjectEntry entry)
    {
        var bytes = SmoScalarTemplateEdit.Source(source, entry, SmoClassIds.Skin, 11);
        if (!SmoSkinDecoder.TryDecode(source, entry, out var skin, out string error))
            throw new InvalidDataException(error);
        if (skin.PaletteFields.Count != 1)
            throw new NotSupportedException("SKIN_PALETTE_SHAPE: editing requires exactly one actual Skin palette assignment.");
        var field = skin.PaletteFields[0];
        if (!SmoDataBlockReader.TryReadHeader(bytes, checked(8 + (int)field.HeaderOffset), out var header) ||
            header.FieldType != 0 || header.PayloadOffset != 8L + field.PayloadOffset ||
            header.PayloadSize != field.PayloadSize || header.PayloadEnd > bytes.Length ||
            header.SizeKind != SmoDataBlockSizeCode.UInt32 || header.HasExtendedFieldType)
            throw new NotSupportedException("SKIN_PALETTE_SHAPE: palette header is outside the supported writer shape.");
        return header;
    }

    public void Dispose() => graph.Dispose();
}
