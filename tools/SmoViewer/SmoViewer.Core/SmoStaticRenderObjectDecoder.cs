using SmoViewer.Sparkplug;
using System.Diagnostics.CodeAnalysis;
using System.Numerics;

namespace SmoViewer.Core;

/// <summary>
/// Independent stored matrices and ordered renderable relationships. Missing
/// matrix fields retain native identity defaults; repeated matrices use the last
/// value. References are inspected metadata, not instantiated runtime objects.
/// </summary>
public sealed record SmoStaticRenderObjectData(
    Matrix4x4 Transform,
    Matrix4x4 EngineInverseTransform,
    IReadOnlyList<SmoNodeRelationship> Renderables,
    byte SerializedFieldMask)
{
    /// <summary>Single-renderable view for the historical corpus report.</summary>
    public SmoNodeRelationship Renderable => Renderables.Count == 1
        ? Renderables[0]
        : throw new InvalidOperationException("This view requires exactly one renderable; enumerate Renderables instead.");
}

/// <summary>Original matrix field bodies with the common reference inspector.</summary>
public static class SmoStaticRenderObjectDecoder
{
    public const int MatrixPayloadSize = 16 * sizeof(float);

    public static bool TryDecode(
        SmoDocument document,
        SmoObjectEntry entry,
        [NotNullWhen(true)] out SmoStaticRenderObjectData? value,
        out string error)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(entry);
        value = null;
        error = string.Empty;
        if (entry.TypeHash != SmoClassIds.StaticRenderObject)
        {
            error = $"Object [{entry.Index}] is not spStaticRenderObject.";
            return false;
        }
        if (!SmoObjectFieldReader.TryRead(document,entry,out var fields,out error))
            return false;
        if (fields.Count == 0 || fields[^1].FieldType != 0 || fields[^1].PayloadSize != 0)
        {
            error = "StaticRenderObject requires one complete serializer section.";
            return false;
        }
        var matrices = new List<NativeMethods.NodeField>();
        var renderables = new List<SmoNodeRelationship>();
        foreach (var field in fields.Take(fields.Count - 1))
        {
            if (field.PayloadSize == 0)
            {
                error = "StaticRenderObject contains more than one serializer section.";
                return false;
            }
            if (field.FieldType is 1 or 2)
                matrices.Add(new NativeMethods.NodeField {Field=(uint)field.FieldType,
                    Offset=checked((uint)field.AbsolutePayloadOffset),Size=field.PayloadSize});
            else if (field.FieldType == 0)
            {
                if (!SmoNodeDecoder.TryDecodeRelationship(document,field.Payload.Span,out var link) || link is null ||
                    link.ObjectId == 0 || link.TargetObjectIndex is not int target ||
                    link.TargetTypeHash is not (SmoClassIds.Model or SmoClassIds.Skin))
                {
                    error = "StaticRenderObject inspector cannot resolve a nonnull Model/Skin renderable.";
                    return false;
                }
                // The runtime accepts the Renderable family; Model/Skin are
                // the explicitly supported metadata consumers in this slice.
                if (link.Encoding == SmoNodeRelationshipEncoding.InlineObject &&
                    document.Objects[target].ParentIndex != entry.Index)
                {
                    error = "Inline StaticRenderObject renderable is not its physical child.";
                    return false;
                }
                renderables.Add(link);
            }
            // Original44FE90 skips unknown bounded fields.
        }
        if (!TryReadMatrices(document.Data.Span,matrices.ToArray(),out var native))
        {
            error = "Invalid bounded StaticRenderObject matrix payload.";
            return false;
        }
        value = new SmoStaticRenderObjectData(native.World,native.Inverse,renderables.AsReadOnly(),
            (byte)(native.FieldMask | (renderables.Count == 0 ? 0u : 1u)));
        return true;
    }

    private static unsafe bool TryReadMatrices(ReadOnlySpan<byte> bytes,ReadOnlySpan<NativeMethods.NodeField> fields,
        out NativeMethods.StaticMatrices value)
    {
        fixed (byte* input = bytes)
        fixed (NativeMethods.NodeField* selected = fields)
            return NativeMethods.spv_static_matrices(input,checked((uint)bytes.Length),selected,
                checked((uint)fields.Length),out value) != 0;
    }
}

/// <summary>Reads the stored transform used by placement and scene code.</summary>
public static class SmoStaticRenderObjectTransformDecoder
{
    public static bool TryDecode(SmoDocument document,SmoObjectEntry entry,out Matrix4x4 transform)
    {
        bool decoded = SmoStaticRenderObjectDecoder.TryDecode(document,entry,out var value,out _);
        transform = value?.Transform ?? Matrix4x4.Identity;
        return decoded;
    }
}
