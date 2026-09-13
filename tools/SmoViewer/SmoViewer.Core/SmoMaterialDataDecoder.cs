using SmoViewer.Sparkplug;

namespace SmoViewer.Core;

public enum SmoMaterialRelationshipStorageKind
{
    NullId = 0,
    LegacyIdOnly,
    Reference,
    InlineObject
}

public sealed record SmoMaterialRelationshipData(
    uint ObjectId,
    SmoMaterialRelationshipStorageKind StorageKind,
    uint? InlineSize,
    uint? InlineTypeHash);

public sealed record SmoMaterialColorData(
    uint AmbientArgb,
    uint DiffuseArgb,
    uint SpecularArgb,
    uint EmissiveArgb,
    float SpecularPower);

public sealed record SmoMaterialStaticUvTransformData(
    bool Enabled,
    IReadOnlyList<float> Matrix3x3);

public sealed record SmoMaterialPassData(
    int Index,
    uint FinalBlendOperation,
    uint LayerClassId,
    int TextureStatesFieldType,
    IReadOnlyList<uint> TextureStates,
    SmoMaterialStaticUvTransformData? StaticUvTransform,
    SmoMaterialRelationshipData? Texture,
    SmoMaterialRelationshipData? AnimationController,
    SmoMaterialRelationshipData? UvController);

public sealed record SmoMaterialDataInfo(
    IReadOnlyList<uint> RenderStates,
    bool UsesVertexAlpha,
    IReadOnlyList<SmoMaterialPassData> Passes,
    SmoMaterialColorData Color,
    SmoMaterialRelationshipData ColorController)
{
    public bool UsesLegacyTextureStates =>
        Passes.Any(item => item.TextureStatesFieldType == 8);

    public bool UsesCurrentTextureStates =>
        Passes.Any(item => item.TextureStatesFieldType == 17);
}

/// <summary>
/// Metadata view of the shared spMaterialSerializer. Native code creates the
/// original scalar material/pass/layer objects; resource IDs remain inspected
/// references, not loaded runtime objects. The current consumer DTO supports
/// one standard layer per pass and requires authored specular power.
/// </summary>
public static class SmoMaterialDataDecoder
{
    public const int RenderStateCount = 11;
    public const int TextureStateCount = 9;
    public const int LegacyTextureStatesField = 8;
    public const int CurrentTextureStatesField = 17;

    public static unsafe bool TryDecode(SmoDocument document, SmoObjectEntry entry,
        out SmoMaterialDataInfo? material, out string error)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(entry);
        material = null;
        error = string.Empty;
        if (!SmoMaterialInspection.TryRead(document, entry, out var snapshot, out error)) return false;
        int start = checked((int)entry.PhysicalOffset + 8);
        ReadOnlySpan<byte> payload = document.Data.Span.Slice(start, checked((int)entry.SerializedSize - 8));
        try
        {
            var info = snapshot!.Info;
            if (info.HasColor == 0)
            {
                error = "Material view requires authored color/power; fresh native DX specular power is uninitialized.";
                return false;
            }
            var nativeLayers = snapshot.Layers;
            if (info.Passes != info.Layers || nativeLayers.Where((item, index) => item.Pass != index || item.Index != 0).Any())
            {
                error = "The current material view supports one standard layer per pass; native material retains all layers.";
                return false;
            }
            var passes = new List<SmoMaterialPassData>(nativeLayers.Length);
            foreach (var item in nativeLayers)
            {
                var layer = item;
                if (!TryDecodeObservedReference(document, payload, layer.Texture, 10, SmoClassIds.TextureData, out var texture, out error) ||
                    !TryDecodeObservedReference(document, payload, layer.Animation, 11, SmoClassIds.AnimTextureController, out var animation, out error) ||
                    !TryDecodeObservedReference(document, payload, layer.UvController, 12, SmoClassIds.UvController, out var uvController, out error))
                    return false;
                var states = new ReadOnlySpan<uint>(layer.States, TextureStateCount).ToArray();
                var uv = layer.HasUv == 0 ? null : new SmoMaterialStaticUvTransformData(layer.UvEnabled != 0,
                    new ReadOnlySpan<float>(layer.UvMatrix, 9).ToArray());
                passes.Add(new SmoMaterialPassData(checked((int)layer.Pass), layer.Blend, layer.ClassId,
                    layer.StatesField, states, uv, texture, animation, uvController));
            }
            if (!TryDecodeObservedReference(document, payload, info.ColorController, 6,
                    SmoClassIds.MaterialColorController, out var colorController, out error)) return false;
            var color = new SmoMaterialColorData(info.Colors[0], info.Colors[1], info.Colors[2], info.Colors[3], info.Power);
            material = new SmoMaterialDataInfo(new ReadOnlySpan<uint>(info.States, RenderStateCount).ToArray(),
                info.VertexAlpha != 0, passes.AsReadOnly(), color,
                colorController ?? new(0, SmoMaterialRelationshipStorageKind.NullId, null, null));
            return true;
        }
        catch (InvalidDataException exception)
        {
            error = exception.Message;
            return false;
        }
    }

    private static bool TryDecodeObservedReference(SmoDocument document, ReadOnlySpan<byte> payload,
        NativeMethods.MaterialReference reference, int field, uint expectedType,
        out SmoMaterialRelationshipData? relationship, out string error)
    {
        relationship = null;
        error = string.Empty;
        if (reference.Size == 0) return true;
        if ((ulong)reference.Offset + reference.Size > (ulong)payload.Length)
        {
            error = "Native material reference is outside its source payload.";
            return false;
        }
        return TryDecodeRelationship(document, payload.Slice((int)reference.Offset, (int)reference.Size),
            reference.Size, field, expectedType, out relationship, out error);
    }

    public static bool TryDecodeRelationship(SmoDocument document, SmoObjectField field,
        uint expectedTypeHash, out SmoMaterialRelationshipData? relationship, out string error)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(field);
        return TryDecodeRelationship(document, field.Payload.Span, field.PayloadSize, field.FieldType,
            expectedTypeHash, out relationship, out error);
    }

    private static bool TryDecodeRelationship(SmoDocument document, ReadOnlySpan<byte> payload,
        uint payloadSize, int field, uint expectedTypeHash,
        out SmoMaterialRelationshipData? relationship, out string error)
    {
        relationship = null;
        error = string.Empty;
        if (!SmoNodeDecoder.TryReadReference(payload, payloadSize, 1, out var prefix))
        {
            error = $"Material relationship field {field} has an invalid native reference envelope.";
            return false;
        }
        uint id = prefix.Id, inlineSize = prefix.InlineSize;
        if (id == 0)
        {
            relationship = new(0, SmoMaterialRelationshipStorageKind.NullId, null, null);
            return true;
        }
        // This is catalog binding, not a substitute for ReadReference's actual
        // serializer dispatch, reference ownership or object construction.
        if (!SmoNodeDecoder.TryGetCataloguedObject(document, id, out var target) ||
            target.TypeHash != expectedTypeHash || !target.IsWithinDataSection || !target.SignatureMatches ||
            (inlineSize != 0 && prefix.ClassId != expectedTypeHash))
        {
            error = $"Material relationship field {field} references object ID {id}, but no catalogued " +
                $"{SmoClassRegistry.GetDisplayName(expectedTypeHash)} has that unique ID.";
            return false;
        }
        relationship = new(id, inlineSize == 0 ? SmoMaterialRelationshipStorageKind.Reference :
            SmoMaterialRelationshipStorageKind.InlineObject, inlineSize, inlineSize == 0 ? null : prefix.ClassId);
        return true;
    }
}
