using SmoViewer.Sparkplug;

namespace SmoViewer.Core;

/// <summary>Host ownership and catalog binding for the common Model/Skin reader.</summary>
internal static class SmoModelInspection
{
    internal static bool TryRead(SmoDocument document, SmoObjectEntry entry, uint kind,
        out NativeMethods.ModelInfo info, out NativeMethods.SkinBone[] bones, out string error)
        => TryRead(document, entry, kind, false, out info, out bones, out _, out error);

    internal static bool TryRead(SmoDocument document, SmoObjectEntry entry, uint kind,
        out NativeMethods.ModelInfo info, out NativeMethods.SkinBone[] bones,
        out NativeMethods.SkinPaletteField[] paletteFields, out string error)
        => TryRead(document, entry, kind, true, out info, out bones, out paletteFields, out error);

    private static unsafe bool TryRead(SmoDocument document, SmoObjectEntry entry, uint kind, bool capturePaletteFields,
        out NativeMethods.ModelInfo info, out NativeMethods.SkinBone[] bones,
        out NativeMethods.SkinPaletteField[] paletteFields, out string error)
    {
        info = default;
        bones = [];
        paletteFields = [];
        error = string.Empty;
        if (!entry.IsWithinDataSection || !entry.SignatureMatches || entry.PhysicalOffset < 0 ||
            entry.SerializedSize < 9 || entry.PhysicalEnd > document.Data.Length || entry.SerializedSize > int.MaxValue)
        {
            error = "Model/Skin inspector requires a complete catalogued object.";
            return false;
        }
        var payload = document.Data.Span.Slice(checked((int)entry.PhysicalOffset + 8), checked((int)entry.SerializedSize - 8));
        try
        {
            IntPtr pointer;
            fixed (byte* bytes = payload)
                pointer = NativeMethods.Check(NativeMethods.spv_model_read(bytes, (uint)payload.Length, kind));
            using var handle = new ModelHandle(pointer);
            NativeMethods.Check(NativeMethods.spv_model_info(handle, out info));
            bones = new NativeMethods.SkinBone[checked((int)info.Bones)];
            fixed (NativeMethods.SkinBone* output = bones)
                NativeMethods.Check(NativeMethods.spv_model_bones(handle, output, info.Bones));
            if (capturePaletteFields)
            {
                NativeMethods.Check(NativeMethods.spv_model_palette_fields(handle, null, 0, out var count));
                if (count > 65536)
                    throw new InvalidDataException("Skin palette observation count exceeds the bounded native section.");
                paletteFields = new NativeMethods.SkinPaletteField[checked((int)count)];
                fixed (NativeMethods.SkinPaletteField* output = paletteFields)
                    NativeMethods.Check(NativeMethods.spv_model_palette_fields(handle, output, count, out _));
            }
            return true;
        }
        catch (InvalidDataException exception)
        {
            error = exception.Message;
            return false;
        }
    }

    internal static bool TryReference(SmoDocument document, SmoObjectEntry entry,
        NativeMethods.MaterialReference reference, uint expectedType,
        out SmoNodeRelationship? value, out string error)
    {
        value = null;
        error = string.Empty;
        if (reference.Size == 0) return true;
        if ((ulong)reference.Offset + reference.Size > entry.SerializedSize - 8)
        {
            error = "Inspected reference exceeds Model/Skin payload.";
            return false;
        }
        int offset = checked((int)entry.PhysicalOffset + 8 + (int)reference.Offset);
        if (!SmoNodeDecoder.TryDecodeRelationship(document, document.Data.Span.Slice(offset, (int)reference.Size), out var link) || link is null)
        {
            error = "Model/Skin reference has an invalid shared native envelope.";
            return false;
        }
        if (link.ObjectId == 0) return true;
        if (!SmoNodeDecoder.TryGetCataloguedObject(document, link.ObjectId, out var target) ||
            !target.IsWithinDataSection || !target.SignatureMatches || target.TypeHash != expectedType)
        {
            error = $"Model/Skin metadata reference {link.ObjectId} does not bind to a catalogued " +
                $"{SmoClassRegistry.GetDisplayName(expectedType)}.";
            return false;
        }
        value = link;
        return true;
    }

    internal static bool TryRenderable(SmoDocument document, SmoObjectEntry entry, NativeMethods.ModelInfo info,
        out SmoRenderableData? renderable, out SmoNodeRelationship? mesh, out string error)
    {
        renderable = null;
        mesh = null;
        if (!TryRenderableFields(document, entry, info.RenderableMask, info.Alpha, info.Priority,
                info.Material, info.Fog, out renderable, out error) ||
            !TryReference(document, entry, info.Mesh, SmoClassIds.MeshData, out mesh, out error)) return false;
        if (mesh is null)
        {
            error = "Current Model/Skin consumer view requires a bound MeshData; native reader also supports omitted mesh fields.";
            return false;
        }
        return true;
    }

    // Common host projection for Model/Skin/Text observations from the actual
    // Renderable reader; it does not decode any game fields itself.
    internal static bool TryRenderableFields(SmoDocument document, SmoObjectEntry entry,
        uint mask, uint alpha, uint priority, NativeMethods.MaterialReference materialReference,
        NativeMethods.MaterialReference fogReference, out SmoRenderableData? value, out string error)
    {
        value = null;
        if (!TryReference(document, entry, materialReference, SmoClassIds.MaterialData, out var material, out error) ||
            !TryReference(document, entry, fogReference, SmoClassIds.Fog, out var fog, out error)) return false;
        value = new SmoRenderableData(material, fog, (mask & 4) == 0 ? null : alpha,
            (mask & 8) == 0 ? null : priority, (byte)mask);
        return true;
    }
}
