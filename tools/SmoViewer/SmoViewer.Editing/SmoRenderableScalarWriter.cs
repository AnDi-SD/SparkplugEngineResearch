using SmoViewer.Sparkplug;

namespace SmoViewer.Core;

/// <summary>Existing Renderable assignments edited through the shared reader and writer.</summary>
public static class SmoRenderableScalarWriter
{
    public static unsafe byte[] PatchSkinSort(SmoDocument document, SmoObjectEntry entry,
        bool alphaSort, uint priority)
    {
        var source = SmoScalarTemplateEdit.Source(document, entry, SmoClassIds.Skin, 11);
        var fields = source[8..];
        SerializedBytesHandle owner;
        fixed (byte* input = fields)
            owner = new(NativeMethods.Check(NativeMethods.spv_skin_patch_sort_scalars(input,
                (uint)fields.Length, alphaSort ? 1u : 0u, priority)));
        return SmoScalarTemplateEdit.CopyResult(source, owner);
    }
}
