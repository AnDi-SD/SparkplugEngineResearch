using SmoViewer.Sparkplug;

namespace SmoViewer.Core;

/// <summary>
/// Edits existing scalar assignments through the actual material reader and
/// shared writer slices. Retains the original object header, field headers,
/// references and all unrelated bytes; does not fully serialize a material.
/// </summary>
public static class SmoMaterialScalarWriter
{
    public static byte[] PatchSingleStandardLayer(SmoDocument document, SmoObjectEntry entry,
        IReadOnlyList<uint> renderStates, uint blend, IReadOnlyList<uint> textureStates)
    {
        ArgumentNullException.ThrowIfNull(textureStates);
        if (textureStates.Count != 9)
            throw new ArgumentException("PC material writer requires nine texture states.", nameof(textureStates));
        return Patch(document, entry, renderStates, blend, textureStates.ToArray(), 0);
    }

    public static byte[] PatchAllPasses(SmoDocument document, SmoObjectEntry entry,
        IReadOnlyList<uint> renderStates, uint blend) => Patch(document, entry, renderStates, blend, [], 1);

    private static unsafe byte[] Patch(SmoDocument document, SmoObjectEntry entry,
        IReadOnlyList<uint> renderStates, uint blend, uint[] textureStates, uint kind)
    {
        ArgumentNullException.ThrowIfNull(renderStates);
        if (renderStates.Count != 11)
            throw new ArgumentException("PC material writer requires eleven render states.", nameof(renderStates));
        var source = SmoScalarTemplateEdit.Source(document, entry, SmoClassIds.MaterialData, 9);
        var fields = source[8..];
        uint[] states = renderStates.ToArray();
        SerializedBytesHandle owner;
        fixed (byte* input = fields)
        fixed (uint* stateInput = states)
        fixed (uint* textureInput = textureStates)
            owner = new(NativeMethods.Check(NativeMethods.spv_material_patch_scalars(input, (uint)fields.Length,
                stateInput, (uint)states.Length, blend, textureInput, (uint)textureStates.Length, kind)));
        return SmoScalarTemplateEdit.CopyResult(source, owner);
    }
}
