using SmoViewer.Sparkplug;
using System.Diagnostics.CodeAnalysis;
using System.Numerics;

namespace SmoViewer.Core;

public enum SmoMeshRepresentationKind
{
    CrossPlatform,
    Direct3D,
    Ps2Native
}

public sealed record SmoPs2NativeMeshData(
    Vector4 BoundingSphere,
    uint PrimitiveCount,
    uint VertexCount,
    uint VertexFormat,
    uint DmaQwordCount,
    uint AdditionalTextureCoordinateCount,
    uint BlendWeightCount,
    int DmaPayloadSize);

public sealed record SmoMeshBoundingBoxData(Vector3 Minimum,Vector3 Maximum);

public sealed record SmoPcMeshData(
    uint PrimitiveType,
    uint PrimitiveCount,
    uint VertexCount,
    uint VertexFormat,
    uint RuntimeVertexBufferSize,
    int SerializedIndexBufferSize,
    int SerializedStride,
    int RuntimeStride);

public sealed record SmoMeshRepresentationData(
    int FieldType,
    SmoMeshRepresentationKind Kind,
    uint PayloadSize,
    int AbsolutePayloadOffset,
    SmoMesh? Geometry,
    SmoPcMeshData? Pc,
    SmoPs2NativeMeshData? Ps2Native);

public sealed record SmoMeshDataInfo(
    SmoMeshRepresentationData? CrossPlatform,
    SmoMeshRepresentationData? PlatformSpecific,
    SmoMeshBoundingBoxData? BoundingBox)
{
    public bool HasBothRepresentations =>
        CrossPlatform is not null && PlatformSpecific is not null;
}

/// <summary>
/// Strict read-only decoder for the complete observed <c>spMeshData</c>
/// representation container. PC E0/E1 geometry is delegated to
/// <see cref="SmoMeshDecoder"/>; native PS2 DMA/VIF bytes remain opaque but
/// their bounded header, flags and optional bounding box are decoded.
/// </summary>
public static class SmoMeshDataDecoder
{
    public const int Ps2HeaderSize = 40;
    public const int Ps2DmaQwordSize = 16;
    public const int BoundingBoxSize = 6 * sizeof(float);

    public static unsafe bool TryDecode(
        SmoDocument document,
        SmoObjectEntry entry,
        [NotNullWhen(true)] out SmoMeshDataInfo? value,
        out string error)
    {
        ArgumentNullException.ThrowIfNull(document);
        ArgumentNullException.ThrowIfNull(entry);
        value = null;
        error = string.Empty;
        if (entry.TypeHash != SmoClassIds.MeshData)
        {
            error = $"Object [{entry.Index}] is not spMeshData.";
            return false;
        }
        if (!SmoObjectFieldReader.TryRead(document,entry,out var fields,out error))
            return false;
        if (fields.Count < 2 || !IsTerminator(fields[^1]) ||
            fields.Take(fields.Count - 1).Any(IsTerminator))
        {
            error = "spMeshData must contain one final field-zero terminator.";
            return false;
        }

        int offset = 0;
        SmoMeshRepresentationData? cross = null;
        if (TryTake(fields,ref offset,0,out SmoObjectField? crossField))
        {
            SmoMesh? mesh = null;
            SmoPcMeshData? pc = null;
            if (SmoMeshDecoder.TryDecodeRepresentation(
                    entry,crossField!,out mesh,out _))
            {
                pc = FromGeometry(mesh!);
            }
            else if (!TryDecodeCrossPlatformMetadata(crossField!,out pc,out error))
            {
                error = $"Cross-platform mesh representation is invalid: {error}";
                return false;
            }
            cross = new SmoMeshRepresentationData(
                0,SmoMeshRepresentationKind.CrossPlatform,crossField!.PayloadSize,
                crossField.AbsolutePayloadOffset,mesh,pc,null);
        }

        SmoMeshRepresentationData? platform = null;
        if (TryTake(fields,ref offset,1,out SmoObjectField? platformField))
        {
            bool ps2Platform = (document.Header.PlatformMask & 8) != 0 &&
                               (document.Header.PlatformMask & 2) == 0;
            if (ps2Platform)
            {
                if (!TryDecodePs2Native(platformField!,out var native,out error)) return false;
                platform = new SmoMeshRepresentationData(1,SmoMeshRepresentationKind.Ps2Native,
                    platformField!.PayloadSize,platformField.AbsolutePayloadOffset,null,null,native);
            }
            else if (SmoMeshDecoder.TryDecodeRepresentation(
                    entry,platformField!,out SmoMesh? mesh,out _))
            {
                platform = new SmoMeshRepresentationData(
                    1,SmoMeshRepresentationKind.Direct3D,platformField!.PayloadSize,
                    platformField.AbsolutePayloadOffset,mesh,FromGeometry(mesh!),null);
            }
            else if (TryDecodeDirect3DMetadata(
                         platformField!,out SmoPcMeshData? pc,out error))
            {
                // Structurally valid geometry can still contain non-finite
                // optional attributes in deliberately broken game fixtures.
                platform = new SmoMeshRepresentationData(
                    1,SmoMeshRepresentationKind.Direct3D,platformField!.PayloadSize,
                    platformField.AbsolutePayloadOffset,null,pc,null);
            }
            else
            {
                error = $"Platform-specific mesh representation is invalid: {error}";
                return false;
            }
        }

        SmoMeshBoundingBoxData? bounds = null;
        if (TryTake(fields,ref offset,2,out SmoObjectField? boundsField))
        {
            try
            {
                fixed (byte* input = boundsField!.Payload.Span)
                {
                    NativeMethods.Check(NativeMethods.spv_mesh_bounds(input,
                        checked((uint)boundsField.Payload.Length),out var native));
                    bounds = new SmoMeshBoundingBoxData(native.Minimum,native.Maximum);
                }
            }
            catch (InvalidDataException exception) { error = exception.Message; return false; }
        }

        if (cross is null && platform is null && bounds is null)
        {
            error = "spMeshData contains no inspectable representation or bounds.";
            return false;
        }
        if (offset != fields.Count - 1)
        {
            error = $"Unexpected spMeshData field {fields[offset].FieldType} at " +
                    $"relative offset 0x{fields[offset].RelativeHeaderOffset:X}.";
            return false;
        }

        value = new SmoMeshDataInfo(cross,platform,bounds);
        return true;
    }

    private static SmoPcMeshData FromGeometry(SmoMesh mesh) => new(
        mesh.PrimitiveType, mesh.PrimitiveCount, checked((uint)mesh.VertexCount),
        mesh.VertexFormat, mesh.RuntimeVertexBufferSize, checked(mesh.IndexCount * sizeof(ushort)),
        mesh.Stride, mesh.RuntimeStride);

    private static bool TryDecodeCrossPlatformMetadata(SmoObjectField field,
        [NotNullWhen(true)] out SmoPcMeshData? value, out string error) =>
        TryDecodeBufferMetadata(field, out value, out error);

    private static bool TryDecodeDirect3DMetadata(SmoObjectField field,
        [NotNullWhen(true)] out SmoPcMeshData? value, out string error) =>
        TryDecodeBufferMetadata(field, out value, out error);

    private static bool TryDecodeBufferMetadata(SmoObjectField field,
        [NotNullWhen(true)] out SmoPcMeshData? value, out string error)
    {
        value = null;
        if (!SmoMeshDecoder.TryReadMetadata(field, out var info, out error)) return false;
        value = new SmoPcMeshData(info.PrimitiveType, info.Primitives, info.Vertices,
            info.ComponentFlags, info.RuntimeVbSize, checked((int)(info.Indices * info.IndexElementSize)),
            checked((int)info.SerializedStride), checked((int)info.RuntimeStride));
        return true;
    }

    private static unsafe bool TryDecodePs2Native(SmoObjectField field,
        [NotNullWhen(true)] out SmoPs2NativeMeshData? value, out string error)
    {
        value = null; error = string.Empty;
        try
        {
            fixed (byte* input = field.Payload.Span)
            {
                NativeMethods.Check(NativeMethods.spv_ps2_mesh_header(input,
                    checked((uint)field.Payload.Length),out var native));
                value = new SmoPs2NativeMeshData(native.Sphere,native.Primitives,native.Vertices,
                    native.ComponentFlags,native.PacketQwords,native.AdditionalUvCount,
                    native.WeightCount,checked((int)(native.PacketQwords * 16)));
                return true;
            }
        }
        catch (InvalidDataException exception) { error = exception.Message; return false; }
    }

    private static bool TryTake(
        IReadOnlyList<SmoObjectField> fields,
        ref int offset,
        int fieldType,
        out SmoObjectField? field)
    {
        field = null;
        if (offset >= fields.Count - 1 || fields[offset].FieldType != fieldType ||
            fields[offset].PayloadSize == 0)
        {
            return false;
        }
        field = fields[offset++];
        return true;
    }

    private static bool IsTerminator(SmoObjectField field) =>
        field.FieldType == 0 && field.PayloadSize == 0;

}
