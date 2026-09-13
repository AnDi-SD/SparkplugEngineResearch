using System.Diagnostics.CodeAnalysis;
using System.Numerics;

namespace SmoViewer.Core;

/// <summary>Actual Skin field0 location, relative to the whole object payload after its eight-byte header.</summary>
public readonly record struct SmoSkinPaletteField(
    uint HeaderOffset,
    uint PayloadOffset,
    uint PayloadSize,
    uint AssignmentOrder);

public sealed record SmoSkinBone(
    int PaletteIndex,
    int NodeObjectIndex,
    uint NodeObjectId,
    uint InlineSerializedSize,
    Matrix4x4 InverseBindMatrix)
{
    public SmoNodeRelationshipEncoding Encoding => InlineSerializedSize == 0
        ? SmoNodeRelationshipEncoding.SizedReference
        : SmoNodeRelationshipEncoding.InlineObject;
}

/// <summary>
/// Complete observed read-only state of <c>spSkinSerializer</c>, including its
/// inherited <c>spRenderableSerializer</c> and <c>spModelSerializer</c> sections.
/// </summary>
public sealed record SmoSkin(
    int ObjectIndex,
    string Name,
    SmoRenderableData Renderable,
    SmoNodeRelationship BaseMesh,
    uint? ProjectionGroup,
    byte ModelSerializedFieldMask,
    uint BlendInfluenceCountHint,
    IReadOnlyList<SmoSkinBone> Bones,
    byte SerializedFieldMask)
{
    /// <summary>Ordered shared-reader observations, including zero-weight and repeated palette fields.</summary>
    public IReadOnlyList<SmoSkinPaletteField> PaletteFields { get; init; } = Array.Empty<SmoSkinPaletteField>();

    public uint? AlphaSortEnable => Renderable.AlphaSortEnable;
    public uint? Priority => Renderable.Priority;

    public bool IsFieldSerialized(int fieldType) =>
        fieldType == 0 && (SerializedFieldMask & 1) != 0;
}

/// <summary>
/// Native Renderable/Model/Skin section readers. Unresolved bone IDs remain
/// catalog metadata; matrices are returned verbatim without C# inversion,
/// affine classification or finite-value repair.
/// </summary>
public static class SmoSkinDecoder
{
    public static bool TryDecode(SmoDocument document, SmoObjectEntry entry,
        [NotNullWhen(true)] out SmoSkin? skin, out string error)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(entry);
        skin = null;
        error = string.Empty;
        if (entry.TypeHash != SmoClassIds.Skin)
        {
            error = $"Object [{entry.Index}] is not spSkin.";
            return false;
        }
        if (!SmoModelInspection.TryRead(document, entry, 1, out var native, out var nativeBones, out var nativePaletteFields, out error) ||
            !SmoModelInspection.TryRenderable(document, entry, native, out var renderable, out var mesh, out error))
            return false;
        var bones = new List<SmoSkinBone>(nativeBones.Length);
        foreach (var bone in nativeBones)
        {
            if (!SmoModelInspection.TryReference(document, entry, bone.Reference, SmoClassIds.Node, out var link, out error))
                return false;
            if (link?.TargetObjectIndex is not int node || link.ObjectId != bone.Id || link.InlineSerializedSize != bone.InlineSize)
            {
                error = "Skin palette metadata has no consistent catalogued Node.";
                return false;
            }
            bones.Add(new SmoSkinBone(bones.Count, node, bone.Id, bone.InlineSize, bone.InverseBind));
        }
        skin = new SmoSkin(entry.Index, entry.Name, renderable!, mesh!,
            (native.ModelMask & 2) == 0 ? null : native.Projection, (byte)native.ModelMask,
            native.Weights, bones.AsReadOnly(), (byte)native.SkinMask)
        {
            PaletteFields = Array.AsReadOnly(nativePaletteFields.Select(field => new SmoSkinPaletteField(
                field.HeaderOffset, field.PayloadOffset, field.PayloadSize, field.AssignmentOrder)).ToArray())
        };
        return true;
    }
}
