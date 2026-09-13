using System.Diagnostics.CodeAnalysis;

namespace SmoViewer.Core;

/// <summary>
/// The inherited <c>spRenderableSerializer</c> state stored before the final
/// serializer section of an <c>spModel</c>. Nullable scalar values preserve
/// the distinction between an omitted legacy default and an explicitly
/// serialized value.
/// </summary>
public sealed record SmoRenderableData(
    SmoNodeRelationship? Material,
    SmoNodeRelationship? Fog,
    uint? AlphaSortEnable,
    uint? Priority,
    byte SerializedFieldMask)
{
    public bool IsFieldSerialized(int fieldType) =>
        fieldType is >= 0 and <= 3 &&
        (SerializedFieldMask & (1 << fieldType)) != 0;
}

/// <summary>Complete observed read-only state of <c>spModelSerializer</c>.</summary>
public sealed record SmoModelData(
    SmoRenderableData Renderable,
    SmoNodeRelationship BaseMesh,
    uint? ProjectionGroup,
    byte SerializedFieldMask)
{
    public bool IsFieldSerialized(int fieldType) =>
        fieldType is >= 0 and <= 1 &&
        (SerializedFieldMask & (1 << fieldType)) != 0;
}

/// <summary>Metadata adapter over the original Renderable/Model section readers.</summary>
public static class SmoModelDecoder
{
    public static bool TryDecode(SmoDocument document, SmoObjectEntry entry,
        [NotNullWhen(true)] out SmoModelData? value, out string error)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(entry);
        value = null;
        error = string.Empty;
        if (entry.TypeHash != SmoClassIds.Model)
        {
            error = $"Object [{entry.Index}] is not spModel.";
            return false;
        }
        if (!SmoModelInspection.TryRead(document, entry, 0, out var native, out _, out error) ||
            !SmoModelInspection.TryRenderable(document, entry, native, out var renderable, out var mesh, out error))
            return false;
        value = new SmoModelData(renderable!, mesh!, (native.ModelMask & 2) == 0 ? null : native.Projection,
            (byte)native.ModelMask);
        return true;
    }
}
